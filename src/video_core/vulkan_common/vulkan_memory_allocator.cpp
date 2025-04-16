// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <bit> // For std::countr_zero if used elsewhere, not strictly needed here anymore
#include <optional>
#include <span>
#include <vector>

// Keep common includes
#include "common/alignment.h" // May not be needed if manual alignment logic is gone
#include "common/assert.h"
#include "common/common_types.h"
#include "common/literals.h"
#include "common/logging/log.h"
// #include "common/polyfill_ranges.h" // Only needed if std::ranges still used

// Keep Vulkan includes
#include "video_core/vulkan_common/vma.h" // VMA header is essential
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

// Bring vk::Buffer and vk::Image definitions likely needed by calling code
#include "video_core/vulkan_common/vulkan_buffer.h"
#include "video_core/vulkan_common/vulkan_image.h"


namespace Vulkan {
namespace {

// --- Helper functions related to VMA usage (mostly kept from original) ---

[[nodiscard]] VkMemoryPropertyFlags MemoryUsagePreferredVmaFlags(MemoryUsage usage) {
    // Prefer coherent memory for host-visible types to avoid manual flush/invalidate
    return usage != MemoryUsage::DeviceLocal ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                             : VkMemoryPropertyFlagBits{};
}

[[nodiscard]] VmaAllocationCreateFlags MemoryUsageVmaFlags(MemoryUsage usage) {
    // Add MAPPED_BIT for relevant types so VMA handles mapping.
    // Add HOST_ACCESS flags to give VMA hints about usage patterns.
    switch (usage) {
    case MemoryUsage::Upload:
    case MemoryUsage::Stream:
        return VMA_ALLOCATION_CREATE_MAPPED_BIT |
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case MemoryUsage::Download:
        return VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    case MemoryUsage::DeviceLocal:
        return {}; // No mapping or specific host access needed
    }
    // Should not be reached given the enum definition
    ASSERT_MSG(false, "Invalid memory usage={}", static_cast<int>(usage));
    return {};
}

[[nodiscard]] VmaMemoryUsage MemoryUsageVma(MemoryUsage usage) {
    // Use VMA_MEMORY_USAGE_AUTO* to let VMA choose the best heap based on flags.
    // This is generally optimal, especially on UMA architectures (common on mobile).
    switch (usage) {
    case MemoryUsage::DeviceLocal:
    case MemoryUsage::Stream: // Prefer device for stream, but allow host visibility via flags
        return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    case MemoryUsage::Upload:
    case MemoryUsage::Download: // Prefer host for upload/download
        return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    }
     // Should not be reached
    ASSERT_MSG(false, "Invalid memory usage={}", static_cast<int>(usage));
    return VMA_MEMORY_USAGE_AUTO; // Default fallback
}

// --- Removed Manual Allocation related classes and functions ---
// Removed: struct Range
// Removed: AllocationChunkSize() - VMA handles chunk sizing internally
// Removed: MemoryUsagePropertyFlags() - VMA uses VmaMemoryUsage and flags directly
// Removed: class MemoryAllocation
// Removed: class MemoryCommit
// Removed: MemoryAllocator::Commit()
// Removed: MemoryAllocator::TryCommit()
// Removed: MemoryAllocator::TryAllocMemory()
// Removed: MemoryAllocator::ReleaseMemory()
// Removed: MemoryAllocator::MemoryPropertyFlags()
// Removed: MemoryAllocator::FindType()

} // Anonymous namespace

// --- Updated MemoryAllocator Class ---

MemoryAllocator::MemoryAllocator(const Device& device_)
    : device{device_}, allocator{device.GetAllocator()}
// Removed: properties - VMA handles property checking internally
// Removed: buffer_image_granularity - VMA handles this internally
{
    // Keep the logic for RenderDoc + small host-visible device-local heaps if necessary.
    // This filter affects the memoryTypeBits passed to vmaCreateBuffer for Stream usage.
    if (device.HasDebuggingToolAttached()) {
        using namespace Common::Literals;
        // Assuming ForEachDeviceLocalHostVisibleHeap iterates memory *types* that are both
        // DEVICE_LOCAL and HOST_VISIBLE and checks their corresponding heap sizes.
        ForEachDeviceLocalHostVisibleHeap(device, [this](u32 type_index, const VkMemoryHeap& heap) {
            if (heap.size <= 256_MiB) {
                LOG_WARNING(Render, "Disabling small (<= 256 MiB) DEVICE_LOCAL|HOST_VISIBLE memory type {} due to RenderDoc.", type_index);
                // Filter out this specific memory type index.
                disallowed_memory_types_for_stream |= (1u << type_index);
            }
        });
    }
}

MemoryAllocator::~MemoryAllocator() {
    // VMA allocator is owned and destroyed by the Vulkan::Device class,
    // assuming `device.GetAllocator()` returns the VmaAllocator managed there.
    // If MemoryAllocator were to *create* the VmaAllocator, it would need to destroy it here:
    // if (allocator != VK_NULL_HANDLE) {
    //     vmaDestroyAllocator(allocator);
    // }
}

vk::Image MemoryAllocator::CreateImage(const VkImageCreateInfo& ci) const {
    // Use VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE for general images.
    // Let VMA choose between dedicated allocation or sub-allocation.
    const VmaAllocationCreateInfo alloc_ci = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT, // Keep budget check
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0, // Usually none required, VMA derives from usage
        .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // Prefer true device local
        .memoryTypeBits = 0, // Let VMA choose from all allowed types
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
        .priority = 0.5f, // Default priority
    };

    VkImage handle{};
    VmaAllocation allocation{};
    // VmaAllocationInfo alloc_info{}; // Not typically needed unless debugging VMA itself

    // Use VMA to create the image and allocate/bind memory simultaneously
    vk::Check(vmaCreateImage(allocator, &ci, &alloc_ci, &handle, &allocation, nullptr /*&alloc_info*/));

    // The vk::Image wrapper should store the VmaAllocation handle
    // and use vmaDestroyImage in its destructor.
    return vk::Image(handle, *device.GetLogical(), allocator, allocation,
                     device.GetDispatchLoader());
}

vk::Buffer MemoryAllocator::CreateBuffer(const VkBufferCreateInfo& ci, MemoryUsage usage) const {
    // Determine VMA flags based on the abstract MemoryUsage enum
    const VmaAllocationCreateInfo alloc_ci = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | MemoryUsageVmaFlags(usage),
        .usage = MemoryUsageVma(usage),
        .requiredFlags = 0, // Let VMA derive requirements from usage
        .preferredFlags = MemoryUsagePreferredVmaFlags(usage),
        // Apply the filter for Stream usage to avoid problematic small heaps when debugging
        .memoryTypeBits = (usage == MemoryUsage::Stream) ? ~disallowed_memory_types_for_stream : 0u, // 0 means VMA considers all types compatible with usage/flags
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
        .priority = (usage == MemoryUsage::DeviceLocal) ? 0.5f : 1.0f, // Prioritize host-visible slightly? (Optional)
    };

    VkBuffer handle{};
    VmaAllocationInfo alloc_info{}; // Need this to get mapped pointer if requested
    VmaAllocation allocation{};
    VkMemoryPropertyFlags property_flags{}; // To check coherency

    // Use VMA to create the buffer and allocate/bind memory
    vk::Check(vmaCreateBuffer(allocator, &ci, &alloc_ci, &handle, &allocation, &alloc_info));

    // Check the actual properties of the allocated memory (e.g., for coherency)
    // This might differ from preferredFlags if VMA had to fall back.
    vmaGetAllocationMemoryProperties(allocator, allocation, &property_flags);

    // Get the persistently mapped pointer if VMA_ALLOCATION_CREATE_MAPPED_BIT was set
    u8* data = reinterpret_cast<u8*>(alloc_info.pMappedData);
    const std::span<u8> mapped_data = data ? std::span<u8>{data, ci.size} : std::span<u8>{};
    const bool is_coherent = (property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

    // The vk::Buffer wrapper should store the VmaAllocation handle
    // and use vmaDestroyBuffer in its destructor. It should also store
    // the mapped_data span and coherency flag.
    return vk::Buffer(handle, *device.GetLogical(), allocator, allocation, mapped_data, is_coherent,
                      device.GetDispatchLoader());
}

// --- Helper for RenderDoc check (Implementation needed if not elsewhere) ---
void MemoryAllocator::ForEachDeviceLocalHostVisibleHeap(const Device& device, std::function<void(u32, const VkMemoryHeap&)> func) {
     const auto memory_props = device.GetPhysical().GetMemoryProperties().memoryProperties;
     for (u32 i = 0; i < memory_props.memoryTypeCount; ++i) {
         const bool is_device_local = (memory_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
         const bool is_host_visible = (memory_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

         if (is_device_local && is_host_visible) {
             const u32 heap_index = memory_props.memoryTypes[i].heapIndex;
             func(i, memory_props.memoryHeaps[heap_index]);
         }
     }
}


} // namespace Vulkan
