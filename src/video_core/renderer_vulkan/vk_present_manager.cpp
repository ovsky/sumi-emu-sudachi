// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm> // Required for std::min
#include <mutex>     // Required for std::scoped_lock, std::unique_lock
#include <utility>   // Required for std::exchange

#include "common/microprofile.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/frontend/emu_window.h"
#include "video_core/renderer_vulkan/vk_present_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_swapchain.h" // Swapchain class handles presentation mode selection
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_surface.h"

// Forward declaration (assuming vk_memory_allocator.h defines this)
#include "video_core/renderer_vulkan/vk_memory_allocator.h"

namespace Vulkan {

MICROPROFILE_DEFINE(Vulkan_WaitPresent, "Vulkan", "Wait For Present", MP_RGB(128, 128, 128));
MICROPROFILE_DEFINE(Vulkan_CopyToSwapchain, "Vulkan", "Copy to swapchain", MP_RGB(192, 255, 192));

namespace {

bool CanBlitToSwapchain(const vk::PhysicalDevice& physical_device, VkFormat format) {
    const VkFormatProperties props{physical_device.GetFormatProperties(format)};
    // Check if the format supports blitting as a destination
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;
}

[[nodiscard]] constexpr VkImageSubresourceLayers MakeImageSubresourceLayers() {
    return VkImageSubresourceLayers{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

[[nodiscard]] constexpr VkImageBlit MakeImageBlit(s32 frame_width, s32 frame_height, s32 swapchain_width,
                                     s32 swapchain_height) {
    return VkImageBlit{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffsets = {{ {0, 0, 0}, {frame_width, frame_height, 1} }}, // Define source region
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffsets = {{ {0, 0, 0}, {swapchain_width, swapchain_height, 1} }}, // Define destination region
    };
}

[[nodiscard]] constexpr VkImageCopy MakeImageCopy(u32 frame_width, u32 frame_height, u32 swapchain_width,
                                     u32 swapchain_height) {
    return VkImageCopy{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffset = {0, 0, 0},
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffset = {0, 0, 0},
        .extent = { // Copy the overlapping region
            .width = std::min(frame_width, swapchain_width),
            .height = std::min(frame_height, swapchain_height),
            .depth = 1,
        },
    };
}

} // Anonymous namespace

PresentManager::PresentManager(const vk::Instance& instance_,
                               Core::Frontend::EmuWindow& render_window_, const Device& device_,
                               MemoryAllocator& memory_allocator_, Scheduler& scheduler_,
                               Swapchain& swapchain_, vk::SurfaceKHR& surface_)
    : instance{instance_}, render_window{render_window_}, device{device_},
      memory_allocator{memory_allocator_}, scheduler{scheduler_}, swapchain{swapchain_},
      surface{surface_},
      // Check blit support based on the *swapchain's* chosen format
      blit_supported{CanBlitToSwapchain(device.GetPhysical(), swapchain.GetImageFormat())},
      use_present_thread{Settings::values.async_presentation.GetValue()} {

    // Initial image count determination based on swapchain settings
    SetImageCount();

    auto& dld = device.GetLogical();
    cmdpool = dld.CreateCommandPool({
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = // Transient buffers reset individually
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device.GetGraphicsFamily(),
    });

    // Allocate command buffers for each frame resource slot
    auto cmdbuffers = cmdpool.Allocate(image_count);

    frames.resize(image_count);
    for (u32 i = 0; i < frames.size(); i++) {
        Frame& frame = frames[i];
        frame.cmdbuf = vk::CommandBuffer{cmdbuffers[i], device.GetDispatchLoader()};

        // Semaphore signaled by main render pass, waited on by present copy submit
        frame.render_ready = dld.CreateSemaphore({});

        // Fence signaled by present copy submit completion, waited on before reusing this Frame struct
        frame.present_done = dld.CreateFence({ .flags = VK_FENCE_CREATE_SIGNALED_BIT }); // Create signaled

        free_queue.push(&frame); // Add initially available frame resource
    }

    // Start presentation thread if enabled
    if (use_present_thread) {
        present_thread = std::jthread([this](std::stop_token token) { PresentThread(token); });
    }
}

// Ensure Vulkan resources are cleaned up
PresentManager::~PresentManager() {
    // Ensure thread is joined before destroying resources it might use
    if (present_thread.joinable()) {
         // Request stop and wait for thread completion
         present_thread.request_stop();
         present_thread.join();
    }

     // Wait for the device to be idle before destroying resources
     // This ensures no pending GPU operations are using them.
    if (device.GetLogical()) {
         device.GetLogical().WaitIdle();

         // Destroy frame resources
         for (Frame& frame : frames) {
             // Fences/Semaphores are destroyed implicitly by vk::UniqueFence/vk::UniqueSemaphore
             // Command buffers are freed when the pool is destroyed
             // frame.image, frame.image_view, frame.framebuffer need explicit destruction
             // Assuming vk::UniqueImage etc. or manual destruction occurs here.
             // If not using RAII wrappers like vk::Unique*, add vkDestroy... calls:
             // if (frame.framebuffer) device.GetLogical().DestroyFramebuffer(*frame.framebuffer);
             // if (frame.image_view) device.GetLogical().DestroyImageView(*frame.image_view);
             // memory_allocator.DestroyImage(frame.image); // Assuming allocator manages this
         }
         frames.clear();

         // Destroy command pool (frees associated command buffers)
         if (cmdpool) {
            device.GetLogical().DestroyCommandPool(*cmdpool);
         }
    }
}


Frame* PresentManager::GetRenderFrame() {
    MICROPROFILE_SCOPE(Vulkan_WaitPresent);

    Frame* frame = nullptr;
    {
        // Wait until a frame resource slot is available in the free queue
        std::unique_lock lock{free_mutex};
        free_cv.wait(lock, [this] { return !free_queue.empty(); });

        // Retrieve the available frame resource slot
        frame = free_queue.front();
        free_queue.pop();
    } // Lock released

    // Wait for the *previous* presentation copy submission using this frame resource slot to finish.
    // This ensures the command buffer and other resources in 'frame' are safe to reuse.
    // This does NOT wait for the main rendering of the previous frame.
    // Timeout can be useful here to detect hangs.
    frame->present_done.Wait(UINT64_MAX);
    frame->present_done.Reset();

    return frame;
}

void PresentManager::Present(Frame* frame) {
    if (!use_present_thread) {
        // Synchronous path: Wait for main render submission if needed (handled by scheduler?)
        // scheduler.WaitWorker(); // Ensure main render commands are submitted before copy

        // Directly execute the copy and present operations
        CopyToSwapchain(frame);

        // Immediately return the frame resource slot to the free queue
        std::scoped_lock lock{free_mutex};
        free_queue.push(frame);
        free_cv.notify_one();
        return;
    }

    // Asynchronous path: Queue the frame for the presentation thread
    // The scheduler ensures this lambda executes after the main render command submission
    // signals the frame->render_ready semaphore.
    scheduler.Record([this, frame](vk::CommandBuffer /* main_render_cmd */) {
        // This lambda runs potentially *after* the main render commands for 'frame'
        // have been submitted and have signaled frame->render_ready.
        std::unique_lock lock{queue_mutex};
        present_queue.push(frame);
        frame_cv.notify_one(); // Wake up the present thread
    });
}

// Recreates per-frame resources (target image, view, framebuffer) when needed (e.g., resize)
void PresentManager::RecreateFrame(Frame* frame, u32 width, u32 height, VkFormat image_view_format,
                                   VkRenderPass rd) {
    // Note: This function recreates the *intermediate* render target image, not the swapchain image.
    auto& dld = device.GetLogical();

    // Destroy existing resources before creating new ones
    // Assuming vk::Unique* handles this, otherwise explicit vkDestroy calls needed here.
    // if (frame->framebuffer) dld.DestroyFramebuffer(*frame->framebuffer);
    // if (frame->image_view) dld.DestroyImageView(*frame.image_view);
    // memory_allocator.DestroyImage(frame.image); // Requires MemoryAllocator interface

    frame->width = width;
    frame->height = height;

    // Create the image that the main rendering pass will render into
    frame->image = memory_allocator.CreateImage({
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, // Allows different view formats if needed
        .imageType = VK_IMAGE_TYPE_2D,
        .format = swapchain.GetImageFormat(), // Use the swapchain's format
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | // Will be source for copy/blit to swapchain
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // Will be rendered to
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    });

    // Create a view for the render target image
    frame->image_view = dld.CreateImageView({
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *frame->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_view_format, // Format used by the render pass
        .components = { .r = VK_COMPONENT_SWIZZLE_IDENTITY, /* .g, .b, .a = IDENTITY */ }, // Standard component mapping
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    });

    // Create the framebuffer linking the render pass and the image view
    const VkImageView image_view_handle = *frame->image_view;
    frame->framebuffer = dld.CreateFramebuffer({
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = rd,
        .attachmentCount = 1,
        .pAttachments = &image_view_handle,
        .width = width,
        .height = height,
        .layers = 1,
    });
}

// Waits for all pending presentation operations to complete. Useful for shutdown or resizing.
void PresentManager::WaitPresent() {
    if (!use_present_thread) {
        // If not using the thread, ensure the last synchronous present finished.
        // This might involve waiting on the last used frame->present_done fence
        // or simply waiting for the device to idle. Device idle is safest.
        device.GetLogical().WaitIdle();
        return;
    }

    // Wait for the presentation queue to become empty
    {
        std::unique_lock queue_lock{queue_mutex};
        frame_cv.wait(queue_lock, [this] { return present_queue.empty(); });
    }

    // At this point, the last frame has been *taken* by the present thread.
    // Wait for the swapchain_mutex to ensure the *copy and present* operation
    // for that last frame has fully completed before returning.
    std::scoped_lock swapchain_lock{swapchain_mutex};
}


void PresentManager::PresentThread(std::stop_token token) {
    Common::SetCurrentThreadName("VulkanPresent");
    while (!token.stop_requested()) {
        Frame* frame = nullptr;
        {
            std::unique_lock lock{queue_mutex};

            // Wait until a frame is available in the presentation queue or stop is requested
            Common::CondvarWait(frame_cv, lock, token, [this] { return !present_queue.empty(); });
            if (token.stop_requested()) {
                return; // Exit if stop requested while waiting
            }

            // Retrieve the frame to present
            frame = present_queue.front();
            present_queue.pop();

            // Notify WaitPresent if the queue is now empty
            if (present_queue.empty()) {
                 frame_cv.notify_all(); // Use notify_all for potentially multiple waiters
            }

             // Lock the swapchain mutex *before* releasing the queue mutex.
             // This ensures WaitPresent() properly synchronizes with the last frame.
             std::exchange(lock, std::unique_lock{swapchain_mutex});
        } // queue_mutex released, swapchain_mutex acquired

        // Perform the copy/blit to the acquired swapchain image and present it
        CopyToSwapchain(frame);

        // Release the swapchain mutex after presentation
        // swapchain_mutex released automatically by scope exit

        // Return the frame resource slot to the free queue
        {
            std::scoped_lock free_lock{free_mutex};
            free_queue.push(frame);
            free_cv.notify_one(); // Notify GetRenderFrame that a slot is free
        }
    }
}

// Handles recreating the swapchain itself (e.g., due to resize, mode change)
void PresentManager::RecreateSwapchain(Frame* frame) {
    // Device should be idle before destroying/recreating swapchain
    device.GetLogical().WaitIdle();

    // Recreate the swapchain (this should ideally select Mailbox mode if available)
    swapchain.Create(*surface, frame->width, frame->height);

    // Update internal image count based on the new swapchain
    SetImageCount();

    // Re-check blit support for the new swapchain format
    blit_supported = CanBlitToSwapchain(device.GetPhysical(), swapchain.GetImageFormat());

    // TODO: May need to resize/recreate the 'frames' vector and command buffers
    // if the new image_count differs from the old one. This is complex and
    // requires careful handling of existing frame resources.
    // The current implementation assumes image_count doesn't change drastically
    // or that recreation happens infrequently. For robustness, resizing logic
    // might be needed here. Example sketch:
    /*
    if (frames.size() != image_count) {
        // 1. Destroy old frame resources (fences, semaphores, images, views, fbs)
        // 2. Destroy old command pool/buffers
        // 3. Resize frames vector
        // 4. Create new command pool/buffers
        // 5. Re-initialize frame resources in the resized vector (similar to constructor)
        LOG_WARNING(Render_Vulkan, "Swapchain image count changed - full frame resource recreation not fully implemented.");
    }
    */

    // After swapchain recreation, existing framebuffers targeting old swapchain images
    // might become invalid if the render pass changes. RecreateFrame might need
    // to be called for all frames if the render pass depends on swapchain properties.
}

// Updates the internal count of frame resources based on the swapchain's image count
void PresentManager::SetImageCount() {
    // Determine the number of frame resource sets to use.
    // Typically matches swapchain image count, but clamp to avoid excessive resource usage.
    // A minimum of 2 is required for double buffering, 3 for triple buffering (common with Mailbox).
    constexpr size_t MAX_FRAMES_IN_FLIGHT = 3; // Use triple buffering for better pacing with Mailbox/Async
    image_count = std::min(swapchain.GetImageCount(), MAX_FRAMES_IN_FLIGHT);
    image_count = std::max<size_t>(image_count, 2); // Ensure at least double buffering
}

// Wrapper around CopyToSwapchainImpl to handle surface loss errors
void PresentManager::CopyToSwapchain(Frame* frame) {
    bool requires_recreation = false;
    while (true) {
        try {
            // Recreate surface and swapchain if the surface was lost
            if (requires_recreation) {
                 LOG_INFO(Render_Vulkan, "Recreating Vulkan surface due to loss.");
                 surface = CreateSurface(instance, render_window.GetWindowInfo());
                 RecreateSwapchain(frame); // This also updates image_count, checks blit support
                 requires_recreation = false; // Reset flag
            }

            // Attempt the copy and present operation
            CopyToSwapchainImpl(frame);
            return; // Success

        } catch (const vk::Exception& except) {
            VkResult result = except.GetResult();
            if (result == VK_ERROR_SURFACE_LOST_KHR) {
                LOG_WARNING(Render_Vulkan, "Vulkan surface lost (VK_ERROR_SURFACE_LOST_KHR).");
                requires_recreation = true; // Trigger recreation on next loop iteration
            } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                // These results from vkQueuePresentKHR (inside swapchain.Present)
                // indicate the swapchain needs recreation, but the surface might still be valid.
                LOG_INFO(Render_Vulkan, "Swapchain out of date or suboptimal ({}). Recreating.", result);
                RecreateSwapchain(frame);
                // No 'return' here, loop again to attempt presentation with the new swapchain
            } else if (result == VK_ERROR_DEVICE_LOST) {
                 device.ReportLoss(); // Handle device loss critically
                 throw; // Rethrow after reporting
            } else {
                 LOG_CRITICAL(Render_Vulkan, "Unhandled Vulkan error during presentation: {}", result);
                 throw; // Rethrow unexpected errors
            }
        }
    }
}

// Core logic for copying the rendered frame to the swapchain image and presenting
void PresentManager::CopyToSwapchainImpl(Frame* frame) {
    MICROPROFILE_SCOPE(Vulkan_CopyToSwapchain);

    // Acquire the next available swapchain image.
    // Handles recreation internally if AcquireNextImageKHR returns OUT_OF_DATE or SUBOPTIMAL.
    while (swapchain.AcquireNextImage()) {
        // If AcquireNextImage triggered a recreation, update internal state
        RecreateSwapchain(frame);
    }

    // --- Record Presentation Command Buffer ---
    const vk::CommandBuffer cmdbuf{frame->cmdbuf};
    cmdbuf.Begin({ .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT });

    const VkImage swapchain_image = swapchain.CurrentImage();
    const VkExtent2D swapchain_extent = swapchain.GetExtent();
    const VkImage rendered_image = *frame->image; // Image rendered by the main pass

    // Barrier 1: Transition rendered image (source) and swapchain image (dest) for transfer
    const std::array pre_transfer_barriers {
        // Transition rendered image from ColorAttachmentOptimal/General -> TransferSrcOptimal
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // Finished rendering to it
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,         // Will be read by copy/blit
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // Assume it was used as attachment
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = rendered_image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        },
        // Transition swapchain image from Undefined/PresentSrc -> TransferDstOptimal
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0, // No need to wait for previous writes (acquire handles this)
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,        // Will be written by copy/blit
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,               // Initial state after acquire
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        },
    };
    cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Wait for render pass
                           VK_PIPELINE_STAGE_TRANSFER_BIT, // Before transfer ops
                           {}, 0, nullptr, 0, nullptr, pre_transfer_barriers.size(), pre_transfer_barriers.data());

    // Perform Copy or Blit
    if (blit_supported && (frame->width != swapchain_extent.width || frame->height != swapchain_extent.height)) {
        // Use Blit for scaling if supported and sizes differ
        cmdbuf.BlitImage(rendered_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         MakeImageBlit(frame->width, frame->height, swapchain_extent.width, swapchain_extent.height),
                         VK_FILTER_LINEAR); // Linear for better quality scaling
    } else {
        // Use Copy for exact size matches or if blit is not supported
        cmdbuf.CopyImage(rendered_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         MakeImageCopy(frame->width, frame->height, swapchain_extent.width, swapchain_extent.height));
    }

    // Barrier 2: Transition swapchain image for presentation
    const VkImageMemoryBarrier post_transfer_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, // Finished writing to swapchain image
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,    // Presentation engine needs to read it (implicitly)
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // Ready for presentation
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchain_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    // Note: No need to transition the `rendered_image` back immediately unless needed for something else.

    cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,          // After transfer ops
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,    // Before presentation (ensures visibility)
                           {}, 0, nullptr, 0, nullptr, 1, &post_transfer_barrier);

    cmdbuf.End();

    // --- Submit Presentation Command Buffer ---
    const VkSemaphore image_available_semaphore = swapchain.CurrentPresentSemaphore(); // From AcquireNextImage
    const VkSemaphore copy_complete_semaphore = swapchain.CurrentRenderSemaphore();  // Signaled by this submit
    const VkSemaphore main_render_complete_semaphore = *frame->render_ready;        // Signaled by main render pass

    const std::array wait_semaphores = { image_available_semaphore, main_render_complete_semaphore };
    // Stages where waits must occur:
    // - Wait for image_available before writing to swapchain image (Transfer stage)
    // - Wait for main_render_complete before reading from rendered_image (Transfer stage)
    const std::array<VkPipelineStageFlags, 2> wait_stage_masks = {
        VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for acquire before transfer
        VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for main render before transfer
    };

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<u32>(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = cmdbuf.address(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &copy_complete_semaphore, // Signal that copy is done
    };

    {
        // Use scheduler's mutex for queue submission serialization if necessary,
        // or a dedicated queue mutex if presentation uses a different queue.
        std::scoped_lock submit_lock{scheduler.submit_mutex};
        // Submit the copy commands, signaling the copy_complete_semaphore
        // and signaling the frame->present_done fence.
        vk::Check(device.GetGraphicsQueue().Submit(submit_info, *frame->present_done));
    }

    // Present the image (waits for copy_complete_semaphore)
    swapchain.Present(copy_complete_semaphore);
}

} // namespace Vulkan