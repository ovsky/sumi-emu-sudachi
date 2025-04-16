// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <array>    // Included for std::array
#include <cstddef>  // Included for offsetof

#include "common/common_types.h"
#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

constexpr u32 NUM_TEXTURE_SCALING_WORDS = 4;
constexpr u32 NUM_IMAGE_SCALING_WORDS = 2;
constexpr u32 NUM_TEXTURE_AND_IMAGE_SCALING_WORDS =
    NUM_TEXTURE_SCALING_WORDS + NUM_IMAGE_SCALING_WORDS;

struct RescalingLayout {
    alignas(16) std::array<u32, NUM_TEXTURE_SCALING_WORDS> rescaling_textures;
    alignas(16) std::array<u32, NUM_IMAGE_SCALING_WORDS> rescaling_images;
    u32 down_factor;
};
struct RenderAreaLayout {
    std::array<f32, 4> render_area;
};
constexpr u32 RESCALING_LAYOUT_WORDS_OFFSET = offsetof(RescalingLayout, rescaling_textures);
constexpr u32 RESCALING_LAYOUT_DOWN_FACTOR_OFFSET = offsetof(RescalingLayout, down_factor);
constexpr u32 RENDERAREA_LAYOUT_OFFSET = offsetof(RenderAreaLayout, render_area);

/**
 * Emits SPIR-V code from the intermediate representation.
 *
 * @param profile The profile information.
 * @param runtime_info Runtime information for shader execution.
 * @param program The intermediate representation program.
 * @param bindings Output parameter for binding information.
 * @param enable_optimization If true, SPIR-V optimization passes will be run. Defaults to true.
 * @return A vector containing the generated SPIR-V binary code (as 32-bit words).
 *
 * @note The actual optimization logic needs to be implemented in the corresponding .cpp file
 * by conditionally invoking SPIR-V optimization tools/libraries based on this flag.
 */
[[nodiscard]] std::vector<u32> EmitSPIRV(const Profile& profile, const RuntimeInfo& runtime_info,
                                         IR::Program& program, Bindings& bindings,
                                         bool enable_optimization = true); // Added optimization flag


/**
 * Convenience overload for EmitSPIRV without explicit RuntimeInfo and Bindings.
 *
 * @param profile The profile information.
 * @param program The intermediate representation program.
 * @param enable_optimization If true, SPIR-V optimization passes will be run. Defaults to true.
 * @return A vector containing the generated SPIR-V binary code (as 32-bit words).
 */
[[nodiscard]] inline std::vector<u32> EmitSPIRV(const Profile& profile, IR::Program& program,
                                                bool enable_optimization = true) { // Added optimization flag
    Bindings binding;
    // Pass the optimization flag to the main implementation
    return EmitSPIRV(profile, {}, program, binding, enable_optimization);
}

} // namespace Shader::Backend::SPIRV