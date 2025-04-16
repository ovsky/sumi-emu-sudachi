// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/common_types.h" // For u64 etc.
#include "video_core/memory_manager.h" // Included via fence_manager.h -> vk_buffer_cache.h indirectly, add for clarity
#include "video_core/renderer_vulkan/vk_fence_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

// --- InnerFence Implementation ---

InnerFence::InnerFence(Scheduler& scheduler_, bool is_stubbed_)
    : FenceBase(is_stubbed_), scheduler{scheduler_}, wait_tick{0} {
    // If the fence is stubbed, it might be considered signaled immediately,
    // or the scheduler might handle stubbed ticks appropriately.
    // If stubbed means instantly signaled:
    // if (is_stubbed) {
    //     wait_tick = scheduler.GetCompletedTick(); // Get already completed tick
    // }
    // Otherwise, rely on Queue() to get the relevant tick.
}

InnerFence::~InnerFence() = default; // No explicit Vulkan resources owned here

/**
 * @brief Records a synchronization point in the scheduler's timeline.
 *
 * This function interacts with the scheduler to mark the current point
 * in the command submission sequence. The actual GPU fence associated
 * with this point is managed internally by the (optimized) scheduler.
 */
void InnerFence::Queue() {
    if (IsStubbed()) {
        // Stubbed fences represent no actual GPU work, maybe use a sentinel completed value.
        // wait_tick = scheduler.GetCompletedTick(); // Or a special value like 0?
        // Or perhaps QueueFence on the scheduler handles stubbed fences correctly.
        // Assuming Scheduler::QueueGpuFence handles the stubbed case:
         wait_tick = scheduler.QueueGpuFence(IsStubbed());
         return;
    }
    // Ask the scheduler to queue a fence signal and return the associated tick/timeline value.
    // The scheduler internally uses its buffered VkFence pool.
    wait_tick = scheduler.QueueGpuFence(false);
}

/**
 * @brief Checks if the GPU work associated with this fence's tick has completed.
 * @return True if the work is signaled/completed, false otherwise.
 */
bool InnerFence::IsSignaled() const {
    if (IsStubbed()) {
        return true; // Stubbed fences are always considered signaled.
    }
    if (wait_tick == 0) {
        // Fence hasn't been queued yet, or represents the start. Consider it signaled? Or error?
        // Depending on desired semantics, might return true or false. Let's assume false if not queued.
        return false;
    }
    // Ask the scheduler if the GPU has completed work up to this tick.
    return scheduler.IsTickCompleted(wait_tick);
}

/**
 * @brief Waits on the CPU until the GPU work associated with this fence's tick is completed.
 */
void InnerFence::Wait() {
    if (IsStubbed()) {
        return; // No GPU work to wait for.
    }
    if (wait_tick == 0) {
        // Cannot wait on a fence that hasn't been queued.
        // LOG_WARNING(Render_Vulkan, "Attempted to wait on a fence that was never queued.");
        return;
    }
    // Ask the scheduler to wait until the GPU reaches this tick.
    // The scheduler handles waiting on the correct internal VkFence from its pool.
    scheduler.WaitUntilTick(wait_tick);
}

// --- FenceManager Implementation ---

FenceManager::FenceManager(VideoCore::RasterizerInterface& rasterizer, Tegra::GPU& gpu,
                           TextureCache& texture_cache, BufferCache& buffer_cache,
                           QueryCache& query_cache, const Device& device, Scheduler& scheduler_)
    // Initialize the base GenericFenceManager
    : GenericFenceManager(rasterizer, gpu, texture_cache, buffer_cache, query_cache, device),
      scheduler{scheduler_} {}


/**
 * @brief Creates a high-level fence object.
 * @param is_stubbed If true, the fence represents no actual GPU work.
 * @return A shared pointer to the created InnerFence.
 */
Fence FenceManager::CreateFence(bool is_stubbed) {
    // Create the InnerFence, passing the scheduler reference and stubbed status.
    return std::make_shared<InnerFence>(scheduler, is_stubbed);
}

/**
 * @brief Queues a fence marker in the command stream via the scheduler.
 * @param fence The fence object to queue.
 */
void FenceManager::QueueFence(Fence& fence) {
    // Delegate to the InnerFence's Queue method, which interacts with the scheduler.
    if (fence) {
        fence->Queue();
    }
}

/**
 * @brief Checks if the GPU work associated with the fence has completed.
 * @param fence The fence object to check.
 * @return True if signaled, false otherwise.
 */
bool FenceManager::IsFenceSignaled(Fence& fence) const {
    // Delegate to the InnerFence's IsSignaled method.
    return fence ? fence->IsSignaled() : true; // Null fence is considered signaled? Or handle error?
}

/**
 * @brief Waits on the CPU for the GPU work associated with the fence to complete.
 * @param fence The fence object to wait for.
 */
void FenceManager::WaitFence(Fence& fence) {
    // Delegate to the InnerFence's Wait method.
    if (fence) {
        fence->Wait();
    }
}

} // namespace Vulkan