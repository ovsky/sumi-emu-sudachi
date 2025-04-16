// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>     // Required for std::atomic_* potentially used in dependencies
#include <memory>
#include <mutex>
#include <span>       // Required for vk::Span or std::span
#include <thread>
#include <utility>
#include <vector>     // Required for std::vector potentially used in dependencies

#include "common/common_types.h" // Required for u64
#include "common/logging/log.h"  // LOG_ calls if needed
#include "common/microprofile.h"
#include "common/thread.h" // For Common::SetCurrentThreadName, Common::CondvarWait

#include "video_core/renderer_vulkan/vk_command_pool.h"
#include "video_core/renderer_vulkan/vk_framebuffer.h" // Include Framebuffer definition
#include "video_core/renderer_vulkan/vk_instance.h"   // For vk::Check debug
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_query_cache.h" // Required member
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h" // Required member (if used)
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

// LOG_DECLARE_CATEGORY(Render_Vulkan) // Example if logging is used

// Forward declare settings if used directly (seems only in conditional compilation)
// namespace Settings { extern bool IsGPULevelHigh(); } // Example

namespace Vulkan {

// Define Microprofile scopes if not already defined globally
MICROPROFILE_DEFINE(Vulkan_WorkerThread, "Vulkan", "Worker Thread", MP_RGB(128, 255, 128));
MICROPROFILE_DEFINE(Vulkan_WorkerWait, "Vulkan", "Worker Thread Wait", MP_RGB(255, 128, 128));
MICROPROFILE_DEFINE(Vulkan_Submit, "Vulkan", "Queue Submit", MP_RGB(100, 100, 255));
// Declare scopes defined elsewhere
MICROPROFILE_DECLARE(Vulkan_WaitForWorker);

// --- Scheduler::CommandChunk ---

// Executes all commands stored in the chunk and resets the chunk state.
void Scheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf,
                                         vk::CommandBuffer upload_cmdbuf) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf, upload_cmdbuf);
        // Manually call destructor as placement new was used in Record().
        command->~Command();
        command = next;
    }
    // Reset chunk state
    submit = false;
    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

// --- Scheduler ---

// Constructor: Initializes members and starts the worker thread.
Scheduler::Scheduler(const Device& device_, StateTracker& state_tracker_, QueryCache* query_cache_) // Added query_cache
    : device{device_}, state_tracker{state_tracker_}, query_cache{query_cache_}, // Initialize query_cache
      master_semaphore{std::make_unique<MasterSemaphore>(device)},
      // Pass master_semaphore for internal command pool synchronization if needed
      command_pool{std::make_unique<CommandPool>(*master_semaphore, device)} {

    // Get an initial command chunk ready.
    AcquireNewChunk();
    // Allocate initial command buffers for the worker thread.
    AllocateWorkerCommandBuffer();
    // Start the worker thread.
    worker_thread = std::jthread([this](std::stop_token token) { WorkerThread(token); });
}

// Destructor ensures worker thread is joined properly before destroying members.
Scheduler::~Scheduler() {
    if (worker_thread.joinable()) {
        worker_thread.request_stop();
         // Notify worker thread in case it's waiting on the condition variable
         event_cv.notify_all();
         worker_thread.join();
    }
    // CommandPool and MasterSemaphore unique_ptrs handle their own cleanup automatically.
    // Ensure worker is joined BEFORE members it uses (like command_pool, master_semaphore) are destroyed.
}


/**
 * @brief Submits recorded commands asynchronously and returns the tick associated with this submission.
 * Does not wait for completion. Used for standard frame submissions in a buffered pipeline.
 * @param signal_semaphore Optional semaphore to signal upon completion (e.g., frame render ready).
 * @param wait_semaphore Optional semaphore to wait on before execution (e.g., image available).
 * @return The timeline semaphore value (tick) associated with this submission.
 */
u64 Scheduler::Flush(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    // Submit the current chunk and associated work to the worker thread.
    const u64 signal_value = SubmitExecution(signal_semaphore, wait_semaphore);
    // Prepare context for the next set of commands (e.g., notify query cache).
    AllocateNewContext();
    return signal_value;
}

/**
 * @brief Submits recorded commands and waits specifically for *this* submission to complete on the GPU.
 * @param signal_semaphore Optional semaphore to signal upon completion.
 * @param wait_semaphore Optional semaphore to wait on before execution.
 */
void Scheduler::Finish(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    // Submit the execution and get the tick associated with *this* submission.
    const u64 signal_value = SubmitExecution(signal_semaphore, wait_semaphore);
    // Wait specifically for this submission (and all prior work) to complete.
    WaitUntilTick(signal_value); // Corrected: Wait for the tick of the submitted work.
    // Prepare context for the next frame/batch.
    AllocateNewContext();
}

/**
 * @brief Waits for all currently queued work AND the last submission to finish on the GPU.
 * WARNING: This is a strong synchronization point (full CPU-GPU sync) and should be used
 * sparingly (e.g., shutdown, context reset, specific critical sync points).
 * For optimal performance in a frame-buffered pipeline, use tick-based waits
 * (`WaitUntilTick`) based on resource dependencies managed by the main render loop.
 */
void Scheduler::WaitWorker() {
    MICROPROFILE_SCOPE(Vulkan_WaitForWorker);

    // Ensure any pending commands in the current chunk are sent to the worker.
    DispatchWork();

    // Wait for the worker thread's queue to become empty.
    {
        std::unique_lock ql{queue_mutex};
        // Use wait_for or timeout if needed to prevent potential deadlocks
        event_cv.wait(ql, [this] { return work_queue.empty(); });
    }

    // Wait for the last command chunk processing by the worker thread to complete.
    // This ensures the worker isn't modifying state while we potentially wait on the GPU.
    std::scoped_lock el{execution_mutex};

    // Additionally, ensure the GPU has processed up to the latest *signaled* tick.
    // This prevents returning while the GPU might still be executing the last submitted commands.
    WaitUntilTick(GetCompletedTick()); // Wait for the latest known completed value
}

/**
 * @brief Moves the current command chunk to the worker queue if it's not empty.
 * Acquires a new chunk for subsequent recording.
 */
void Scheduler::DispatchWork() {
    // Only dispatch if the current chunk has commands.
    if (chunk->Empty()) {
        return;
    }

    {
        std::scoped_lock ql{queue_mutex};
        work_queue.push(std::move(chunk)); // Move chunk to the queue
    }
    event_cv.notify_all(); // Notify worker thread there's work
    AcquireNewChunk();     // Get a fresh chunk for subsequent recording
}

// --- State Management & Render Pass Handling ---

void Scheduler::RequestRenderpass(const Framebuffer* framebuffer) {
     // Check if the requested render pass is already active
     const VkRenderPass renderpass = framebuffer->RenderPass();
     const VkFramebuffer framebuffer_handle = framebuffer->Handle();
     const VkExtent2D render_area = framebuffer->RenderArea();
     if (state.renderpass == renderpass && state.framebuffer == framebuffer_handle &&
         state.render_area.width == render_area.width &&
         state.render_area.height == render_area.height) {
         return; // Already in the correct render pass state
     }

     // End the current render pass if one is active
     EndRenderPass();

     // Update internal state
     state.renderpass = renderpass;
     state.framebuffer = framebuffer_handle;
     state.render_area = render_area;

     // Record the BeginRenderPass command
     Record([renderpass, framebuffer_handle, render_area](vk::CommandBuffer cmdbuf) {
         // TODO: Handle clear values properly if needed by the render pass.
         const VkRenderPassBeginInfo renderpass_bi{
             .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
             .renderPass = renderpass,
             .framebuffer = framebuffer_handle,
             .renderArea = {{0, 0}, render_area},
             .clearValueCount = 0, // Placeholder - adjust if clears are needed
             .pClearValues = nullptr,
         };
         cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
     });

     // Store information about the framebuffer attachments for EndRenderPass barriers
     num_renderpass_images = framebuffer->NumImages();
     renderpass_images = framebuffer->Images();
     renderpass_image_ranges = framebuffer->ImageRanges();
}

void Scheduler::RequestOutsideRenderPassOperationContext() {
    // Ensure no render pass is active for operations like copies, blits, compute.
    EndRenderPass();
}

// Updates the cached graphics pipeline state. Returns true if changed.
bool Scheduler::UpdateGraphicsPipeline(GraphicsPipeline* pipeline) {
    if (state.graphics_pipeline == pipeline) {
        return false;
    }
    state.graphics_pipeline = pipeline;
    return true;
}

// Updates the cached rescaling state. Returns true if changed.
bool Scheduler::UpdateRescaling(bool is_rescaling) {
    if (state.rescaling_defined && is_rescaling == state.is_rescaling) {
        return false;
    }
    state.rescaling_defined = true;
    state.is_rescaling = is_rescaling;
    return true;
}


// --- Worker Thread ---

void Scheduler::WorkerThread(std::stop_token stop_token) {
    Common::SetCurrentThreadName("VulkanWorker");
    MICROPROFILE_SCOPE(Vulkan_WorkerThread); // Profile the entire worker scope

    // Lambda to safely try popping work from the queue
    const auto TryPopQueue = [this](auto& work) -> bool {
        if (work_queue.empty()) {
            return false;
        }
        work = std::move(work_queue.front());
        work_queue.pop();
        // Notify WaitWorker if the queue becomes empty
        if (work_queue.empty()) {
             event_cv.notify_all();
        }
        return true;
    };

    while (!stop_token.stop_requested()) {
        std::unique_ptr<CommandChunk> work;
        {
            std::unique_lock lk{queue_mutex};
            MICROPROFILE_SCOPE(Vulkan_WorkerWait); // Profile time spent waiting

            // Wait until work is available or stop is requested
            Common::CondvarWait(event_cv, lk, stop_token, [&] { return TryPopQueue(work); });

            if (stop_token.stop_requested()) {
                // If stop requested while waiting or after popping, exit cleanly
                return;
            }

            // Lock execution mutex *before* releasing queue mutex to prevent race conditions
            // in WaitWorker() and ensure sequential execution of chunks.
            std::exchange(lk, std::unique_lock{execution_mutex});
        } // queue_mutex released, execution_mutex acquired

        // Execute the commands in the chunk
        const bool is_submission = work->HasSubmit();
        work->ExecuteAll(current_cmdbuf, current_upload_cmdbuf);

        // If this chunk contained a submission command (SubmitExecution),
        // we need new command buffers for the next chunk.
        if (is_submission) {
            // Command buffers were submitted by MasterSemaphore::SubmitQueue
            // Allocate new ones for the next set of commands.
             AllocateWorkerCommandBuffer();
        }

        // Return the processed chunk to the reserve pool
        {
            std::scoped_lock rl{reserve_mutex};
            chunk_reserve.emplace_back(std::move(work));
        }
        // execution_mutex is released automatically upon exiting the scope
    }
}

// --- Command Buffer Allocation ---

/**
 * @brief Allocates (or recycles) and begins new primary and upload command buffers.
 * Called initially and after each submission by the worker thread.
 */
void Scheduler::AllocateWorkerCommandBuffer() {
     // Get potentially recycled command buffers from the pool.
     // Assuming Commit() retrieves a buffer suitable for graphics/transfer based on pool setup.
     // If CommandPool distinguishes, use CommitGraphics() / CommitTransfer()
    current_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_cmdbuf.Begin({ .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT });

    current_upload_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_upload_cmdbuf.Begin({ .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT });

    // Reset state tracker for the new command buffer context
    state_tracker.InvalidateCommandBufferState();
}

// --- Submission Logic ---

/**
 * @brief Records the final submission commands (barriers, end, submit) and marks the chunk for submission.
 * This function assumes it's called from the main thread recording commands.
 * The actual vkQueueSubmit happens when the worker thread executes the recorded command.
 * @param signal_semaphore Optional semaphore to signal upon completion.
 * @param wait_semaphore Optional semaphore to wait on before execution.
 * @return The timeline semaphore value (tick) for this submission.
 */
u64 Scheduler::SubmitExecution(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    MICROPROFILE_SCOPE(Vulkan_Submit); // Profile the submission process

    // Ensure any pending operations like render passes are ended before submission.
    EndPendingOperations();
    // Invalidate cached scheduler state (pipeline, rescaling, etc.) before submission.
    // StateTracker invalidation happens when new cmd bufs are allocated by the worker.
    InvalidateState();

    // Get the *next* available tick value from the timeline semaphore.
    // This value will be signaled when this submission completes.
    const u64 signal_value = master_semaphore->NextTick();

    // Record the final commands needed for submission (ending command buffers, barriers, submit call).
    // Use RecordWithUploadBuffer as submission involves both command buffers.
    RecordWithUploadBuffer([=, this](vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
        // Ensure upload buffer writes are visible before subsequent queue operations.
        // This barrier assumes uploads happened on the TRANSFER stage.
        static constexpr VkMemoryBarrier upload_write_barrier {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            // Make visible to any subsequent stage on the graphics queue reading the uploaded data
            .dstAccessMask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                             VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
                             VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        };
        upload_cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,          // After upload writes
                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Before subsequent commands read data
                                      {}, upload_write_barrier);

        // End both command buffers.
        upload_cmdbuf.End();
        cmdbuf.End();

        // Optional: Execute a callback right before submitting to the queue.
        if (on_submit) {
            on_submit();
        }

        // Lock the submission mutex to ensure serialized submission to the Vulkan queue.
        std::scoped_lock lock{submit_mutex};

        // Perform the actual submission using the MasterSemaphore.
        // This function handles the VkSubmitInfo setup and vkQueueSubmit call,
        // signaling the timeline semaphore with 'signal_value'.
        switch (const VkResult result = master_semaphore->SubmitQueue(
                    cmdbuf, upload_cmdbuf, signal_semaphore, wait_semaphore, signal_value)) {
        case VK_SUCCESS:
            break; // Expected outcome
        case VK_ERROR_DEVICE_LOST:
            device.ReportLoss(); // Critical error, report and potentially terminate
             throw vk::Exception(result, "Device lost during queue submission");
             break;
        default:
             // Log unexpected errors
             // LOG_CRITICAL(Render_Vulkan, "vkQueueSubmit failed with error: {}", vk::ResultToString(result));
             vk::Check(result); // Throw for other unexpected Vulkan errors
            break;
        }
    });

    // Mark this chunk as containing the submission logic. The worker thread uses this.
    chunk->MarkSubmit();
    // Send the chunk to the worker thread for execution (which includes the submit command).
    DispatchWork();
    // Return the tick associated with this submission.
    return signal_value;
}

// --- Context Allocation / State Reset ---

/**
 * @brief Prepares the scheduler state for the next frame or batch of work.
 * Called after a Flush or Finish by the main thread.
 */
void Scheduler::AllocateNewContext() {
    // Re-enable query streams/segments if they were disabled before submission.
    if (query_cache) {
#if ANDROID // Example conditional compilation
        // Adapt based on actual settings/logic
        // Example: Check settings before notifying
        // if (Settings::IsGPULevelHigh()) {
            query_cache->NotifySegment(true);
        // }
#else
        query_cache->NotifySegment(true);
#endif
    }
    // State invalidation now happens primarily in AllocateWorkerCommandBuffer,
    // which is called after submission by the worker thread, preparing for the *next* chunk.
}

/**
 * @brief Resets cached scheduler state variables that might become invalid across submissions.
 */
void Scheduler::InvalidateState() {
    state.graphics_pipeline = nullptr;
    state.rescaling_defined = false;
    state.renderpass = nullptr;
    state.framebuffer = nullptr;
    // StateTracker invalidation is handled when new command buffers are allocated.
}

/**
 * @brief Ends any pending operations like render passes or query segments before submission.
 */
void Scheduler::EndPendingOperations() {
    // Finalize query segments if active
    if (query_cache) {
#if ANDROID
        // Adapt based on actual settings/logic (Currently commented out in original)
        // if (Settings::IsGPULevelHigh()) {
            // query_cache->DisableStreams();
        // }
#else
        // query_cache->DisableStreams();
#endif
        query_cache->NotifySegment(false); // Notify end of query segment
    }
    // Ensure the current render pass (if any) is ended before submitting.
    EndRenderPass();
}

/**
 * @brief Records the EndRenderPass command if a render pass is currently active.
 * Also records barriers to ensure attachment writes are visible to subsequent operations.
 */
void Scheduler::EndRenderPass() {
    // Only end if a render pass is actually active.
    if (!state.renderpass) {
        return;
    }

    // Capture necessary state for the lambda
    // Ensure these pointers remain valid until the lambda executes on the worker thread.
    // If Framebuffer can be destroyed before execution, copy needed data instead of capturing pointers.
    const u32 num_images = num_renderpass_images;
    const VkImage* images = renderpass_images;
    const VkImageSubresourceRange* ranges = renderpass_image_ranges;

    Record([num_images, images, ranges](vk::CommandBuffer cmdbuf) {
        // Use a vector for dynamic sizing if needed, or ensure max size is sufficient
        constexpr size_t MAX_BARRIERS = 9; // Max attachments handled by the fixed array
        std::array<VkImageMemoryBarrier, MAX_BARRIERS> barriers;

        u32 barrier_count = 0;
        if (num_images > MAX_BARRIERS) {
             // LOG_WARNING(Render_Vulkan, "More render pass attachments ({}) than barrier array size ({})", num_images, MAX_BARRIERS);
             barrier_count = MAX_BARRIERS; // Clamp for safety
        } else {
             barrier_count = num_images;
        }


        for (u32 i = 0; i < barrier_count; ++i) {
            VkImageLayout old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
             if (ranges[i].aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
                  old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
             }
            // Transition to GENERAL layout. Could be optimized if next layout is known.
            VkImageLayout new_layout = VK_IMAGE_LAYOUT_GENERAL;

            barriers[i] = VkImageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | // Writes from color/depth
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, // General visibility
                .oldLayout = old_layout,
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = images[i],
                .subresourceRange = ranges[i],
            };
        }

        cmdbuf.EndRenderPass();

        // Barrier after render pass to make writes available
        cmdbuf.PipelineBarrier(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // Stages where writes occurred
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Make visible before next commands
            0, // No dependency flags
            0, nullptr, // No global memory barriers
            0, nullptr, // No buffer memory barriers
            barrier_count, vk::Span(barriers.data(), barrier_count)); // Use vk::Span or std::span
    });

    // Clear cached render pass state
    state.renderpass = nullptr;
    state.framebuffer = nullptr;
    num_renderpass_images = 0; // Reset count
    renderpass_images = nullptr;
    renderpass_image_ranges = nullptr;
}


// --- Command Chunk Management ---

/**
 * @brief Acquires a new or recycled CommandChunk for recording commands.
 */
void Scheduler::AcquireNewChunk() {
    std::scoped_lock rl{reserve_mutex};

    if (chunk_reserve.empty()) {
        // Allocate a new chunk if the reserve pool is empty.
        chunk = std::make_unique<CommandChunk>();
    } else {
        // Reuse a chunk from the reserve pool.
        chunk = std::move(chunk_reserve.back());
        chunk_reserve.pop_back();
        // Chunk state (first, last, offset, submit) is guaranteed to be reset
        // after ExecuteAll in the worker thread before being put in reserve.
    }
}

// --- FenceManager Interface Implementation ---
// These methods allow FenceManager to interact with the scheduler's timeline.

/**
 * @brief Gets the timeline semaphore value (tick) that will be signaled by the *next* submission.
 * Used by FenceManager to associate a fence with upcoming work.
 */
u64 Scheduler::GetSubmitTick() const {
    // Return the next value that will be used by the MasterSemaphore upon submission.
    return master_semaphore->PeekNextTick();
}

/**
 * @brief Checks if the GPU has completed execution up to the specified timeline tick.
 * @param tick The timeline semaphore value to check.
 * @return True if the GPU has signaled this value or a later one, false otherwise.
 */
bool Scheduler::IsTickCompleted(u64 tick) const {
    // Delegate the check to the MasterSemaphore. Returns true if tick <= completed_value.
    return master_semaphore->IsSignaled(tick);
}

/**
 * @brief Waits on the CPU until the GPU completes execution up to the specified timeline tick.
 * @param tick The timeline semaphore value to wait for.
 */
void Scheduler::WaitUntilTick(u64 tick) {
    // Delegate the wait operation to the MasterSemaphore.
    // The MasterSemaphore handles the vkWaitSemaphores call.
    master_semaphore->Wait(tick);
}

/**
 * @brief Gets the last timeline semaphore value known to be completed by the GPU.
 */
u64 Scheduler::GetCompletedTick() const {
    // Delegate to the MasterSemaphore to get the latest signaled value.
    return master_semaphore->GetValue();
}

// --- Internal Helper Methods (Needed by original Finish) ---

/**
 * @brief Gets the last completed tick value. Helper for original Finish logic.
 * Equivalent to GetCompletedTick().
 */
u64 Scheduler::CurrentTick() const {
    return GetCompletedTick();
}

/**
 * @brief Waits until the specified tick. Helper for original Finish logic.
 * Equivalent to WaitUntilTick().
 */
void Scheduler::Wait(u64 tick) {
     WaitUntilTick(tick);
}


} // namespace Vulkan