// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory> // Included for std::shared_ptr or similar if needed for shared layouts
#include <mutex>
#include <type_traits>
#include <vector> // Included for std::vector

#include "common/thread_worker.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h" // May be less relevant per-pipeline with bindless
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace VideoCore {
class ShaderNotify;
}

namespace Vulkan {

struct GraphicsPipelineCacheKey {
    std::array<u64, 6> unique_hashes;
    FixedPipelineState state;
    // Consider adding a flag here if bindless support affects pipeline caching fundamentally,
    // though often it's handled by shader hashes changing.
    // bool bindless_enabled = false;

    size_t Hash() const noexcept;

    bool operator==(const GraphicsPipelineCacheKey& rhs) const noexcept;

    bool operator!=(const GraphicsPipelineCacheKey& rhs) const noexcept {
        return !operator==(rhs);
    }

    size_t Size() const noexcept {
        // Adjust size calculation if members change
        return sizeof(unique_hashes) + state.Size();
    }
};
// Assertions remain valid if members are standard layout / trivially copyable
static_assert(std::is_standard_layout_v<GraphicsPipelineCacheKey>);
static_assert(std::is_trivially_copyable_v<GraphicsPipelineCacheKey>);

} // namespace Vulkan

namespace std {
template <>
struct hash<Vulkan::GraphicsPipelineCacheKey> {
    size_t operator()(const Vulkan::GraphicsPipelineCacheKey& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std

namespace Vulkan {

class Device;
class PipelineStatistics;
class RenderPassCache;
class RescalingPushConstant; // Might need extension for texture indices
class RenderAreaPushConstant;
class Scheduler;

// Forward declare potential bindless manager if layout is external
// class BindlessManager;

class GraphicsPipeline {
    static constexpr size_t NUM_STAGES = Tegra::Engines::Maxwell3D::Regs::MaxShaderStage;

public:
    explicit GraphicsPipeline(
        Scheduler& scheduler, BufferCache& buffer_cache, TextureCache& texture_cache,
        vk::PipelineCache& pipeline_cache, VideoCore::ShaderNotify* shader_notify,
        const Device& device, /* DescriptorPool& descriptor_pool, // May not be needed directly if using global bindless set */
        /* GuestDescriptorQueue& guest_descriptor_queue, // May be less relevant with bindless */
        Common::ThreadWorker* worker_thread,
        PipelineStatistics* pipeline_statistics, RenderPassCache& render_pass_cache,
        const GraphicsPipelineCacheKey& key, std::array<vk::ShaderModule, NUM_STAGES> stages,
        const std::array<const Shader::Info*, NUM_STAGES>& infos,
        bool use_bindless // Flag indicating if this pipeline should use the bindless path
        /* Potentially pass global bindless layout here if not accessible via Device/TextureCache */
        /* vk::DescriptorSetLayout global_bindless_layout = VK_NULL_HANDLE */
        );

    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = delete;
    GraphicsPipeline(GraphicsPipeline&&) noexcept = delete;

    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
    GraphicsPipeline(const GraphicsPipeline&) = delete;

    // Destructor needed to clean up Vulkan resources
    ~GraphicsPipeline();

    void AddTransition(GraphicsPipeline* transition);

    void Configure(bool is_indexed) {
        // Configuration might differ significantly based on bindless usage
        // The configure_func might need to be aware of the bindless state.
        configure_func(this, is_indexed);
    }

    [[nodiscard]] GraphicsPipeline* Next(const GraphicsPipelineCacheKey& current_key) noexcept {
        if (key == current_key) {
            return this;
        }
        const auto it{std::find(transition_keys.begin(), transition_keys.end(), current_key)};
        return it != transition_keys.end() ? transitions[std::distance(transition_keys.begin(), it)]
                                           : nullptr;
    }

    [[nodiscard]] bool IsBuilt() const noexcept {
        return is_built.load(std::memory_order::relaxed);
    }

    // Consider making ConfigureSpecFunc aware of bindless state if needed
    template <typename Spec>
    static auto MakeConfigureSpecFunc() {
        return [](GraphicsPipeline* pl, bool is_indexed) { pl->ConfigureImpl<Spec>(is_indexed); };
    }

    void SetEngine(Tegra::Engines::Maxwell3D* maxwell3d_, Tegra::MemoryManager* gpu_memory_) {
        maxwell3d = maxwell3d_;
        gpu_memory = gpu_memory_;
    }

    [[nodiscard]] vk::PipelineLayout GetPipelineLayout() const { return pipeline_layout; }
    [[nodiscard]] vk::Pipeline GetPipeline() const { return pipeline; }
    [[nodiscard]] bool UsesBindless() const { return uses_bindless_textures; }
    // [[nodiscard]] vk::DescriptorSetLayout GetDescriptorSetLayout() const { return descriptor_set_layout; } // May return global or specific layout

private:
    template <typename Spec>
    void ConfigureImpl(bool is_indexed);

    // ConfigureDraw might need texture indices if using bindless
    void ConfigureDraw(const RescalingPushConstant& rescaling,
                       const RenderAreaPushConstant& render_area /*, BindlessTextureIndices indices */);

    // MakePipeline needs to know whether to use the bindless layout
    void MakePipeline(VkRenderPass render_pass);

    void Validate(); // Validation might need bindless context

    // Member Variables
    const GraphicsPipelineCacheKey key;
    Tegra::Engines::Maxwell3D* maxwell3d;
    Tegra::MemoryManager* gpu_memory;
    const Device& device;
    TextureCache& texture_cache; // Needed for accessing global bindless set/layout?
    BufferCache& buffer_cache;
    vk::PipelineCache& pipeline_cache;
    Scheduler& scheduler;
    // GuestDescriptorQueue& guest_descriptor_queue; // Less relevant?

    const bool uses_bindless_textures; // Flag to indicate bindless path usage

    // Function pointer for configuration logic
    void (*configure_func)(GraphicsPipeline*, bool){};

    // Pipeline transitions
    std::vector<GraphicsPipelineCacheKey> transition_keys;
    std::vector<GraphicsPipeline*> transitions;

    // Shader stages and info
    std::array<vk::ShaderModule, NUM_STAGES> spv_modules;
    std::array<Shader::Info, NUM_STAGES> stage_infos; // Shader::Info needs to reflect bindless usage

    // --- Traditional Descriptor Set Members (Potentially unused in bindless path) ---
    std::array<u32, 5> enabled_uniform_buffer_masks{}; // Still relevant for UBOs
    VideoCommon::UniformBufferSizes uniform_buffer_sizes{}; // Still relevant for UBOs
    u32 num_textures{}; // Less relevant in bindless path (might be 0)
    // vk::DescriptorSetLayout descriptor_set_layout; // May point to global layout or be specific
    // DescriptorAllocator descriptor_allocator; // Likely removed - handled globally
    // vk::DescriptorUpdateTemplate descriptor_update_template; // Likely removed - incompatible/irrelevant

    // --- Core Pipeline Objects ---
    vk::PipelineLayout pipeline_layout = VK_NULL_HANDLE; // Created using appropriate DSL (bindless or traditional)
    vk::Pipeline pipeline = VK_NULL_HANDLE;             // The compiled graphics pipeline state object

    // --- Build Synchronization ---
    std::condition_variable build_condvar;
    std::mutex build_mutex;
    std::atomic_bool is_built{false};
    // bool uses_push_descriptor{false}; // REMOVED - Incompatible with update_after_bind needed for bindless
};

} // namespace Vulkan