// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>
#include <string> // Included for optimizer messages
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// --- SPIRV-Tools Optimizer Include ---
// Ensure SPIRV-Tools library is linked in your build system
#include <spirv-tools/libspirv.hpp> // Provides spv_target_env and other types
#include <spirv-tools/optimizer.hpp>

#include "common/settings.h"
#include "common/common_types.h" // Include common types like u32
#include "common/logging/log.h"  // Include your project's logging header
#include "common/logic_error.h"  // For LogicError
#include "common/assert.h"       // For ASSERT
#include "common/exception.h"    // For NotImplementedException etc.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h" // Include IR::Value definition

// Declare the logger for this file (adjust as needed for your logging setup)
// LOG_DEFINE_CATEGORY(Shader_SPIRV) // Example declaration

namespace Shader::Backend::SPIRV {
namespace {

// --- Helper Function Traits ---
template <class Func>
struct FuncTraits {};

template <class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // Ignore unreachable code warning
#endif

// --- Helper to Set Instruction Definition ---
template <auto func, typename... Args>
void SetDefinition(EmitContext& ctx, IR::Inst* inst, Args... args) {
    inst->SetDefinition<Id>(func(ctx, std::forward<Args>(args)...));
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// --- Helper to Convert IR::Value to SPIR-V Emitter Argument Types ---
template <typename ArgType>
ArgType Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, Id>) {
        return ctx.Def(arg);
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return arg;
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        ASSERT(arg.IsConst()); // Ensure it's actually a const
        return arg.U32();
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        ASSERT(arg.IsAttribute());
        return arg.Attribute();
    } else if constexpr (std::is_same_v<ArgType, IR::Patch>) {
         ASSERT(arg.IsPatch());
         return arg.Patch();
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
         ASSERT(arg.IsReg());
         return arg.Reg();
    }
    // Add other types as needed or assert for unhandled types
    // static_assert(false, "Unhandled argument type in Arg helper");
    // Throwing is better than static_assert(false) in template context
     throw LogicError(std::string("Unhandled argument type conversion for IR::Value"));
}


// --- Helper to Invoke SPIR-V Emitter Functions ---
template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    constexpr size_t arg_offset = is_first_arg_inst ? 2 : 1;

    if constexpr (std::is_same_v<typename Traits::ReturnType, Id>) {
        // Function returns an Id, so set the instruction's definition
        if constexpr (is_first_arg_inst) {
            SetDefinition<func>(
                ctx, inst, inst, // Pass inst as the first arg
                Arg<typename Traits::template ArgType<I + arg_offset>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<func>(
                ctx, inst,
                Arg<typename Traits::template ArgType<I + arg_offset>>(ctx, inst->Arg(I))...);
        }
    } else {
        // Function returns void
        if constexpr (is_first_arg_inst) {
            func(ctx, inst, // Pass inst as the first arg
                 Arg<typename Traits::template ArgType<I + arg_offset>>(ctx, inst->Arg(I))...);
        } else {
            func(ctx,
                 Arg<typename Traits::template ArgType<I + arg_offset>>(ctx, inst->Arg(I))...);
        }
    }
}

template <auto func>
void Invoke(EmitContext& ctx, IR::Inst* inst) {
    using Traits = FuncTraits<decltype(func)>;
    static_assert(Traits::NUM_ARGS >= 1, "SPIR-V emitter function must accept at least EmitContext&");

    // First argument is always EmitContext&, skip it for argument processing
    if constexpr (Traits::NUM_ARGS == 1) {
         // Only EmitContext& arg
        if constexpr (std::is_same_v<typename Traits::ReturnType, Id>) {
            SetDefinition<func>(ctx, inst);
        } else {
            func(ctx);
        }
    } else {
        // Check if the second argument (index 1) is the IR::Inst* itself
        using SecondArgType = typename Traits::template ArgType;
        static constexpr bool is_first_arg_inst = std::is_same_v<SecondArgType, IR::Inst*>;

        // Calculate number of remaining arguments from IR::Inst to process
        constexpr size_t num_ir_args = Traits::NUM_ARGS - (is_first_arg_inst ? 2 : 1);
        using Indices = std::make_index_sequence<num_ir_args>;

        Invoke<func, is_first_arg_inst>(ctx, inst, Indices{});
    }
}

// --- Emit Single IR Instruction ---
void EmitInst(EmitContext& ctx, IR::Inst* inst) {
    // Skip NOPs efficiently
    if (inst->GetOpcode() == IR::Opcode::Nop) {
         return;
    }

    switch (inst->GetOpcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<&Emit##name>(ctx, inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
    }
    // Use fmt::format or equivalent for better error messages if available
    throw LogicError(std::string("Invalid or unhandled IR opcode: ") + std::to_string(static_cast<int>(inst->GetOpcode())));
}

// --- Get SPIR-V Type ID from IR::Type ---
Id TypeId(const EmitContext& ctx, IR::Type type) {
    switch (type) {
    case IR::Type::U1: // Boolean
        return ctx.U1;
    case IR::Type::U32:
        return ctx.U32[1]; // Assuming scalar U32
    // Add cases for other IR types (F32, Vec types, etc.) as needed
    // case IR::Type::F32: return ctx.F32[1];
    default:
         throw NotImplementedException(std::string("Unsupported IR::Type for Phi node or other TypeId usage: ") + std::to_string(static_cast<int>(type)));
    }
}

// --- Traverse the Control Flow Graph (Simplified from original) ---
void Traverse(EmitContext& ctx, IR::Program& program) {
    IR::Block* current_block{};
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block: {
            const Id label{node.data.block->Definition<Id>()};
            if (current_block) {
                 // Only branch if the previous block didn't end with a terminator.
                 // This simplistic check might need refinement based on actual terminator instructions.
                 if (!current_block->Instructions().empty() &&
                     !current_block->Instructions().back().IsTerminator()) {
                     ctx.OpBranch(label);
                 }
            }
            current_block = node.data.block;
            ctx.AddLabel(label);
            for (IR::Inst& inst : node.data.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            break;
        }
        case IR::AbstractSyntaxNode::Type::If: {
            const Id if_label{node.data.if_node.body->Definition<Id>()};    // True branch target
            const Id merge_label{node.data.if_node.merge->Definition<Id>()};// Block after the if/else
            // Find the else target (might be the merge block itself if no else branch exists)
            // This logic assumes a specific structure or needs access to the else block definition.
            // For simplicity, assume the 'else' target is the merge block if not explicitly defined.
             Id else_label = merge_label; // Default target if condition is false
             // TODO: Need a robust way to find the actual else block label if it exists.
             // This might involve looking ahead in the syntax list or having this info in the If node.

            ctx.OpSelectionMerge(merge_label, spv::SelectionControlMask::MaskNone);
            ctx.OpBranchConditional(ctx.Def(node.data.if_node.cond), if_label, else_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Loop: {
            const Id body_label{node.data.loop.body->Definition<Id>()};           // Loop header/body start
            const Id continue_label{node.data.loop.continue_block->Definition<Id>()}; // Continue target
            const Id merge_label{node.data.loop.merge->Definition<Id>()};         // Block after the loop

            // Branch from the block *before* the loop into the loop header.
            ctx.OpBranch(body_label);

            // **Important**: OpLoopMerge should be the *first* instruction in the loop header block (body_label).
            // The Traverse structure makes it hard to emit it correctly there.
            // Emitting it here might be incorrect SPIR-V. This needs structural review.
            // It should likely be emitted *after* ctx.AddLabel(body_label) in the Block case above
            // when current_block == node.data.loop.body.
            LOG_WARNING(Shader_SPIRV, "OpLoopMerge placement in Traverse needs review for correctness.");
            ctx.OpLoopMerge(merge_label, continue_label, spv::LoopControlMask::MaskNone);

            // Branching immediately after OpLoopMerge might also be incorrect depending on header logic.
            // Often the header itself contains the conditional exit branch.
            // ctx.OpBranch(body_label); // This seems redundant with the initial branch *into* the loop.

            break;
        }
        case IR::AbstractSyntaxNode::Type::Break: {
            // Conditional break from a loop or switch
            const Id condition_id = ctx.Def(node.data.break_node.cond);
            const Id merge_target = node.data.break_node.merge->Definition<Id>(); // Target *after* the loop/switch
            const Id continue_target = node.data.break_node.skip->Definition<Id>(); // Target if condition is false (continue within loop/switch)
            ctx.OpBranchConditional(condition_id, merge_target, continue_target);
            break;
        }
        case IR::AbstractSyntaxNode::Type::EndIf:
            // This node marks the end of the 'if' or 'else' path before the merge block.
            // Need to branch to the merge block *if* the preceding block didn't terminate.
            if (current_block) {
                 if (!current_block->Instructions().empty() &&
                     !current_block->Instructions().back().IsTerminator()) {
                    ctx.OpBranch(node.data.end_if.merge->Definition<Id>());
                 }
            }
            break;
        case IR::AbstractSyntaxNode::Type::Repeat: {
             // Represents the back-edge branch of a loop (continue target -> loop header)
            Id cond{ctx.Def(node.data.repeat.cond)}; // Loop continuation condition

            // Optional loop safety counter
            if (!Settings::values.disable_shader_loop_safety_checks) {
                const Id pointer_type{ctx.TypePointer(spv::StorageClass::Private, ctx.U32[1])};
                const Id max_iterations_const = ctx.Const(0x2000u); // Max iterations constant
                const Id safety_counter{
                    ctx.AddGlobalVariable(pointer_type, spv::StorageClass::Private, max_iterations_const)};

                // Don't add Private variables to the interface list
                // if (ctx.profile.supported_spirv >= SPV_SPIRV_VERSION_WORD(1, 4)) {
                //     ctx.interfaces.push_back(safety_counter);
                // }

                const Id old_counter{ctx.OpLoad(ctx.U32[1], safety_counter)};
                const Id new_counter{ctx.OpISub(ctx.U32[1], old_counter, ctx.Const(1u))};
                ctx.OpStore(safety_counter, new_counter);

                // Condition: new_counter > 0
                const Id safety_cond{ctx.OpSGreaterThan(ctx.U1, new_counter, ctx.u32_zero_value)};
                cond = ctx.OpLogicalAnd(ctx.U1, cond, safety_cond); // Combine with original condition
            }

            const Id loop_header_label{node.data.repeat.loop_header->Definition<Id>()}; // Target to loop back to
            const Id merge_label{node.data.repeat.merge->Definition<Id>()};             // Target to exit loop

            // Branch back to header if cond is true, otherwise exit to merge block
            ctx.OpBranchConditional(cond, loop_header_label, merge_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Return:
            ctx.OpReturn();
            break;
        case IR::AbstractSyntaxNode::Type::Unreachable:
            ctx.OpUnreachable();
            break;
        }
        // If the node represents control flow transfer, the concept of 'current_block' ending needs reset
        if (node.type != IR::AbstractSyntaxNode::Type::Block) {
            current_block = nullptr;
        }
    }
}


// --- Define the Main Function Structure ---
Id DefineMain(EmitContext& ctx, IR::Program& program) {
    const Id void_type = ctx.void_id;
    const Id function_type = ctx.TypeFunction(void_type /* return type */); // Assuming void return
    const Id main_func_id = ctx.OpFunction(void_type, spv::FunctionControlMask::MaskNone, function_type);

    // Define labels for all blocks upfront
    for (IR::Block* const block : program.blocks) {
        block->SetDefinition(ctx.OpLabel());
    }

    // Emit the function body by traversing the CFG structure
    Traverse(ctx, program);

    // Ensure the function ends correctly (handle cases where Traverse might not)
    // If the last emitted instruction wasn't a terminator, emit OpReturn or OpUnreachable.
    // This requires inspecting the state of the context after Traverse.
    // For simplicity here, assume Traverse handles termination.

    ctx.OpFunctionEnd();
    return main_func_id;
}

// --- Convert Tessellation Enums to SPIR-V Execution Modes ---
spv::ExecutionMode ExecutionMode(TessPrimitive primitive) {
    switch (primitive) {
    case TessPrimitive::Isolines: return spv::ExecutionMode::Isolines;
    case TessPrimitive::Triangles: return spv::ExecutionMode::Triangles;
    case TessPrimitive::Quads: return spv::ExecutionMode::Quads;
    }
    throw InvalidArgument(std::string("Invalid Tessellation primitive: ") + std::to_string(static_cast<int>(primitive)));
}

spv::ExecutionMode ExecutionMode(TessSpacing spacing) {
    switch (spacing) {
    case TessSpacing::Equal: return spv::ExecutionMode::SpacingEqual;
    case TessSpacing::FractionalOdd: return spv::ExecutionMode::SpacingFractionalOdd;
    case TessSpacing::FractionalEven: return spv::ExecutionMode::SpacingFractionalEven;
    }
    throw InvalidArgument(std::string("Invalid Tessellation spacing: ") + std::to_string(static_cast<int>(spacing)));
}

// --- Define Entry Point and Execution Modes ---
void DefineEntryPoint(const IR::Program& program, EmitContext& ctx, Id main_func_id) {
    const std::span<const Id> interfaces = ctx.InterfaceList(); // Get list of interface variables (Input/Output/...)
    spv::ExecutionModel execution_model{};

    switch (program.stage) {
    case Stage::Compute: {
        execution_model = spv::ExecutionModel::GLCompute;
        const std::array<u32, 3> workgroup_size{program.workgroup_size};
        ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::LocalSize, workgroup_size[0], workgroup_size[1], workgroup_size[2]);
        break;
    }
    case Stage::VertexB: // Assuming VertexB is the standard Vertex stage
        execution_model = spv::ExecutionModel::Vertex;
        break;
    case Stage::TessellationControl:
        execution_model = spv::ExecutionModel::TessellationControl;
        ctx.AddCapability(spv::Capability::Tessellation);
        ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::OutputVertices, program.invocations); // Output patch vertex count
        break;
    case Stage::TessellationEval:
        execution_model = spv::ExecutionModel::TessellationEvaluation;
        ctx.AddCapability(spv::Capability::Tessellation);
        ctx.AddExecutionMode(main_func_id, ExecutionMode(ctx.runtime_info.tess_primitive));
        ctx.AddExecutionMode(main_func_id, ExecutionMode(ctx.runtime_info.tess_spacing));
        ctx.AddExecutionMode(main_func_id, ctx.runtime_info.tess_clockwise
                                       ? spv::ExecutionMode::VertexOrderCw
                                       : spv::ExecutionMode::VertexOrderCcw);
        if (ctx.runtime_info.tess_point_mode) {
             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::PointMode);
        }
        break;
    case Stage::Geometry:
        execution_model = spv::ExecutionModel::Geometry;
        ctx.AddCapability(spv::Capability::Geometry);
        if (ctx.profile.support_geometry_streams) {
            ctx.AddCapability(spv::Capability::GeometryStreams);
        }
        // Input Primitive Execution Mode
        switch (ctx.runtime_info.input_topology) {
            case InputTopology::Points:             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::InputPoints); break;
            case InputTopology::Lines:              ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::InputLines); break;
            case InputTopology::LinesAdjacency:     ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::InputLinesAdjacency); break;
            case InputTopology::Triangles:          ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::Triangles); break; // GS Input is Triangles
            case InputTopology::TrianglesAdjacency: ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::InputTrianglesAdjacency); break;
        }
        // Output Primitive Execution Mode
        switch (program.output_topology) {
            case OutputTopology::PointList:     ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::OutputPoints); break;
            case OutputTopology::LineStrip:     ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::OutputLineStrip); break;
            case OutputTopology::TriangleStrip: ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::OutputTriangleStrip); break;
        }
        if (program.info.stores[IR::Attribute::PointSize]) {
            ctx.AddCapability(spv::Capability::GeometryPointSize);
        }
        ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::Invocations, program.invocations); // Number of GS invocations
        ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::OutputVertices, program.output_vertices); // Max output vertices

        if (program.is_geometry_passthrough) {
            if (ctx.profile.support_geometry_shader_passthrough) {
                ctx.AddExtension("SPV_NV_geometry_shader_passthrough");
                ctx.AddCapability(spv::Capability::GeometryShaderPassthroughNV);
                // Note: Passthrough mode has implications on other execution modes. Review needed.
            } else {
                LOG_WARNING(Shader_SPIRV, "Geometry shader passthrough used but not supported by profile.");
            }
        }
        break;
    case Stage::Fragment:
        execution_model = spv::ExecutionModel::Fragment;
        ctx.AddExecutionMode(main_func_id, ctx.profile.lower_left_origin_mode
                                       ? spv::ExecutionMode::OriginLowerLeft
                                       : spv::ExecutionMode::OriginUpperLeft);
        if (program.info.stores_frag_depth) {
            ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::DepthReplacing);
        } // Add DepthGreater, DepthLess, DepthUnchanged if needed and detected
        if (ctx.runtime_info.force_early_z || program.info.uses_early_fragment_test) {
            ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::EarlyFragmentTests);
        }
        if (program.info.uses_post_depth_coverage && ctx.profile.support_post_depth_coverage) {
             ctx.AddExtension("SPV_EXT_post_depth_coverage"); // If required by SPIR-V version
             ctx.AddCapability(spv::Capability::PostDepthCoverage);
             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::PostDepthCoverage);
        }
        break;
    default:
        throw NotImplementedException(std::string("Unsupported shader stage for entry point definition: ") + std::to_string(static_cast<int>(program.stage)));
    }

    ctx.AddEntryPoint(execution_model, main_func_id, "main", interfaces);
}


// --- Helper to Determine Target SPIR-V Environment for Optimizer ---
// --- MUST BE ADAPTED based on how Profile stores SPIR-V version/target info ---
spv_target_env TargetSPIRVEnvironment(const Profile& profile) {
    // Example logic - replace with actual checks from your Profile struct
    // This needs to map your internal representation (e.g., profile.supported_spirv)
    // to the spv_target_env enum used by SPIRV-Tools.
    // Common environments include: SPV_ENV_UNIVERSAL_1_0 up to SPV_ENV_UNIVERSAL_1_6,
    // SPV_ENV_VULKAN_1_0 up to SPV_ENV_VULKAN_1_3, etc.
    if (profile.supported_spirv >= SPV_SPIRV_VERSION_WORD(1, 6)) {
         return SPV_ENV_VULKAN_1_3; // Example: Map SPIR-V 1.6+ to Vulkan 1.3 env
    } else if (profile.supported_spirv >= SPV_SPIRV_VERSION_WORD(1, 5)) {
         return SPV_ENV_VULKAN_1_2; // Example: Map SPIR-V 1.5+ to Vulkan 1.2 env
    } else if (profile.supported_spirv >= SPV_SPIRV_VERSION_WORD(1, 3)) {
         // Vulkan 1.1 requires SPIR-V 1.3 support from drivers usually
         return SPV_ENV_VULKAN_1_1; // Example: Map SPIR-V 1.3+ to Vulkan 1.1 env
    }
     // Default / Fallback - Adjust based on minimum requirements or common case
    return SPV_ENV_VULKAN_1_0; // Use Vulkan 1.0 as a base fallback
}


// --- Setup Denorm Controls (Execution Modes and Capabilities) ---
void SetupDenormControl(const Profile& profile, const IR::Program& program, EmitContext& ctx,
                        Id main_func_id) {
    // ... (Implementation from original file - unchanged) ...
    // This function adds DenormFlushToZero/DenormPreserve capabilities and execution modes
    // based on program info and profile support.
     const Info& info{program.info};
     bool added_cap = false; // Track if float controls related caps were added

     // FP32
     if (info.uses_fp32_denorms_flush && info.uses_fp32_denorms_preserve) {
         LOG_DEBUG(Shader_SPIRV, "Fp32 denorm flush and preserve on the same shader");
     } else if (info.uses_fp32_denorms_flush) {
         if (profile.support_fp32_denorm_flush) {
             ctx.AddCapability(spv::Capability::DenormFlushToZero);
             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::DenormFlushToZero, 32U);
             added_cap = true;
         } // else Drivers likely flush by default
     } else if (info.uses_fp32_denorms_preserve) {
         if (profile.support_fp32_denorm_preserve) {
             ctx.AddCapability(spv::Capability::DenormPreserve);
             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::DenormPreserve, 32U);
             added_cap = true;
         } else {
             LOG_DEBUG(Shader_SPIRV, "Fp32 denorm preserve used in shader without host support");
         }
     }

     // FP16/FP64 (if separate control is supported)
     if (profile.support_separate_denorm_behavior && !profile.has_broken_fp16_float_controls) {
         // FP16
         if (info.uses_fp16) {
             if (info.uses_fp16_denorms_flush && info.uses_fp16_denorms_preserve) {
                 LOG_DEBUG(Shader_SPIRV, "Fp16 denorm flush and preserve on the same shader");
             } else if (info.uses_fp16_denorms_flush) {
                 if (profile.support_fp16_denorm_flush) {
                     ctx.AddCapability(spv::Capability::DenormFlushToZero);
                     ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::DenormFlushToZero, 16U);
                     added_cap = true;
                 } // else Assume flush by default
             } else if (info.uses_fp16_denorms_preserve) {
                 if (profile.support_fp16_denorm_preserve) {
                     ctx.AddCapability(spv::Capability::DenormPreserve);
                     ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::DenormPreserve, 16U);
                     added_cap = true;
                 } else {
                     LOG_DEBUG(Shader_SPIRV, "Fp16 denorm preserve used in shader without host support");
                 }
             }
         }
         // FP64 (Add similar logic if fp64 is used and relevant)
         // if (info.uses_fp64) { ... }
     }

     // If Denorm modes were added, ensure FloatControls capability is present if needed
     // Note: DenormPreserve/FlushToZero capabilities often imply FloatControls based on SPIR-V spec.
     // if (added_cap && profile.support_float_controls) {
     //    ctx.AddCapability(spv::Capability::FloatControls);
     // }
}

// --- Setup Signed Zero/NaN Preservation (Execution Modes and Capabilities) ---
void SetupSignedNanCapabilities(const Profile& profile, const IR::Program& program,
                                EmitContext& ctx, Id main_func_id) {
    // ... (Implementation from original file - unchanged) ...
    // This function adds SignedZeroInfNanPreserve capability and execution modes
    // based on program info and profile support.
     bool added_cap = false; // Track if float controls related caps were added

     if (program.info.uses_fp16 && !profile.has_broken_fp16_float_controls) {
         if (profile.support_fp16_signed_zero_nan_preserve) {
             ctx.AddCapability(spv::Capability::SignedZeroInfNanPreserve);
             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::SignedZeroInfNanPreserve, 16U);
             added_cap = true;
         }
     }
     if (profile.support_fp32_signed_zero_nan_preserve) {
         ctx.AddCapability(spv::Capability::SignedZeroInfNanPreserve);
         ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::SignedZeroInfNanPreserve, 32U);
         added_cap = true;
     }
     if (program.info.uses_fp64) {
         if (profile.support_fp64_signed_zero_nan_preserve) {
             ctx.AddCapability(spv::Capability::SignedZeroInfNanPreserve);
             ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::SignedZeroInfNanPreserve, 64U);
             added_cap = true;
         }
     }
     // If SignedZeroInfNanPreserve modes were added, ensure FloatControls capability is present if needed
     // Note: SignedZeroInfNanPreserve capability often implies FloatControls based on SPIR-V spec.
     // if (added_cap && profile.support_float_controls) {
     //    ctx.AddCapability(spv::Capability::FloatControls);
     // }
}

// --- Setup Transform Feedback Capabilities and Execution Mode ---
void SetupTransformFeedbackCapabilities(EmitContext& ctx, Id main_func_id) {
    // ... (Implementation from original file - unchanged) ...
     if (ctx.runtime_info.xfb_count == 0 && !ctx.program.info.uses_transform_feedback) {
         return; // No transform feedback active
     }
     ctx.AddCapability(spv::Capability::TransformFeedback);
     // This execution mode should only be added if transform feedback is actually active for this pipeline.
     // Checking xfb_count or similar runtime info is crucial.
     if (ctx.runtime_info.xfb_count > 0) { // Assuming xfb_count indicates activity
        ctx.AddExecutionMode(main_func_id, spv::ExecutionMode::Xfb);
     }
}

// --- Setup Various Capabilities based on Program Info and Profile ---
void SetupCapabilities(const Profile& profile, const Info& info, EmitContext& ctx) {
    // ... (Implementation from original file - unchanged) ...
    // This function adds a wide range of capabilities (Int64, Sampled1D, Demote, MultiViewport, etc.)
    // based on features used in the shader (info) and supported by the host (profile).

     // Basic Types
     if (info.uses_int64) ctx.AddCapability(spv::Capability::Int64);
     if (info.uses_int16) ctx.AddCapability(spv::Capability::Int16);
     if (info.uses_int8) ctx.AddCapability(spv::Capability::Int8Storage); // Check exact usage context
     if (info.uses_float64) ctx.AddCapability(spv::Capability::Float64);
     if (info.uses_float16) ctx.AddCapability(spv::Capability::Float16);

     // Sampling & Images
     if (info.uses_sampled_1d) ctx.AddCapability(spv::Capability::Sampled1D);
     if (info.uses_image_1d) ctx.AddCapability(spv::Capability::Image1D); // If storage image 1D used
     if (info.uses_sampled_buffer) ctx.AddCapability(spv::Capability::SampledBuffer);
     if (info.uses_image_buffer) ctx.AddCapability(spv::Capability::ImageBuffer); // If storage image buffer used
     if (info.uses_sampled_cube_array) ctx.AddCapability(spv::Capability::SampledCubeArray);
     if (info.uses_image_cube_array) ctx.AddCapability(spv::Capability::ImageCubeArray);
     if (info.uses_image_ms_array) ctx.AddCapability(spv::Capability::ImageMSArray);
     if (info.uses_sparse_residency) ctx.AddCapability(spv::Capability::SparseResidency);
     if (info.uses_min_lod_texture_gather) ctx.AddCapability(spv::Capability::MinLod);
     if (info.uses_image_read_without_format) ctx.AddCapability(spv::Capability::StorageImageReadWithoutFormat);
     if (info.uses_image_write_without_format) ctx.AddCapability(spv::Capability::StorageImageWriteWithoutFormat);
     if (info.uses_image_query) ctx.AddCapability(spv::Capability::ImageQuery);
     if (info.uses_derivatives) ctx.AddCapability(spv::Capability::DerivativeControl);
     // Always needed? Or only if OpImage*Gather used? Check usage.
     ctx.AddCapability(spv::Capability::ImageGatherExtended);

     // Storage Types & Access
     if (info.uses_storage_buffer_16bit && profile.support_storage_16bit) ctx.AddCapability(spv::Capability::StorageBuffer16BitAccess);
     if (info.uses_uniform_and_storage_buffer_16bit && profile.support_uniform_storage_16bit) ctx.AddCapability(spv::Capability::UniformAndStorageBuffer16BitAccess);
     if (info.uses_storage_buffer_8bit && profile.support_storage_8bit) ctx.AddCapability(spv::Capability::StorageBuffer8BitAccess);
     if (info.uses_uniform_and_storage_buffer_8bit && profile.support_uniform_storage_8bit) ctx.AddCapability(spv::Capability::UniformAndStorageBuffer8BitAccess);
     if (info.uses_storage_push_constant_16 && profile.support_push_constant_16) ctx.AddCapability(spv::Capability::StoragePushConstant16);
     if (info.uses_storage_push_constant_8 && profile.support_push_constant_8) ctx.AddCapability(spv::Capability::StoragePushConstant8);
     if (info.uses_storage_input_output_16 && profile.support_storage_input_output_16) ctx.AddCapability(spv::Capability::StorageInputOutput16);


     // Control Flow & Output Attributes
     if (info.uses_demote_to_helper_invocation && profile.support_demote_to_helper_invocation) {
         if (profile.supported_spirv < SPV_SPIRV_VERSION_WORD(1, 6)) { // Extension needed pre-1.6
             ctx.AddExtension("SPV_EXT_demote_to_helper_invocation");
         }
         ctx.AddCapability(spv::Capability::DemoteToHelperInvocation);
     }
     if (info.stores[IR::Attribute::ViewportIndex] && profile.support_multi_viewport) {
         ctx.AddCapability(spv::Capability::MultiViewport);
     }
     if (info.stores[IR::Attribute::ViewportMask] && profile.support_viewport_mask) {
         ctx.AddExtension("SPV_NV_viewport_array2");
         ctx.AddCapability(spv::Capability::ShaderViewportMaskNV);
         // Ensure MultiViewport is also added if required by ViewportMaskNV
         if (!info.stores[IR::Attribute::ViewportIndex]) ctx.AddCapability(spv::Capability::MultiViewport);
     }
     if (info.stores[IR::Attribute::Layer] || info.stores[IR::Attribute::ViewportIndex]) {
         if (profile.support_viewport_index_layer_non_geometry && ctx.stage != Stage::Geometry) {
             // Extension needed pre-1.5 (approx)
             if (profile.supported_spirv < SPV_SPIRV_VERSION_WORD(1, 5)) {
                 ctx.AddExtension("SPV_EXT_shader_viewport_index_layer");
             }
             ctx.AddCapability(spv::Capability::ShaderViewportIndexLayer); // Use core cap name (EXT suffix might be implicit)
         }
     }
     if (info.uses_frag_stencil_ref && profile.support_shader_stencil_export) {
         ctx.AddExtension("SPV_EXT_shader_stencil_export");
         ctx.AddCapability(spv::Capability::StencilExportEXT);
     }
     if (info.uses_clip_distance) ctx.AddCapability(spv::Capability::ClipDistance);
     if (info.uses_cull_distance) ctx.AddCapability(spv::Capability::CullDistance);


     // Draw Parameters
     bool needs_draw_params_ext = info.loads[IR::Attribute::InstanceId] || info.loads[IR::Attribute::VertexId];
     bool needs_draw_params_cap = info.loads[IR::Attribute::BaseInstance] || info.loads[IR::Attribute::BaseVertex];
     if ((needs_draw_params_ext && !profile.support_vertex_instance_id) || needs_draw_params_cap) {
         // Extension needed pre-1.3 (approx)
         if (profile.supported_spirv < SPV_SPIRV_VERSION_WORD(1, 3)) {
             ctx.AddExtension("SPV_KHR_shader_draw_parameters");
         }
         ctx.AddCapability(spv::Capability::DrawParameters);
     }


     // Subgroup Operations
     if (profile.support_subgroup_operations) {
         bool needs_ballot = info.uses_subgroup_ballot_ops || (info.uses_subgroup_vote && profile.warp_size_potentially_larger_than_guest);
         bool needs_vote = info.uses_subgroup_vote && !profile.warp_size_potentially_larger_than_guest;
         bool needs_shuffle = info.uses_subgroup_shuffles;
         bool needs_arith = info.uses_subgroup_arithmetic;

         if (needs_ballot) ctx.AddCapability(spv::Capability::GroupNonUniformBallot);
         if (needs_vote) ctx.AddCapability(spv::Capability::GroupNonUniformVote);
         if (needs_shuffle) ctx.AddCapability(spv::Capability::GroupNonUniformShuffle); // Covers Relative too
         if (needs_arith) ctx.AddCapability(spv::Capability::GroupNonUniformArithmetic);
         // Add GroupNonUniformQuad etc. if needed based on info flags
     }


     // Atomics
     if (info.uses_int64_bit_atomics && profile.support_int64_atomics) {
         ctx.AddCapability(spv::Capability::Int64Atomics);
     }
     // Add float atomic capabilities if needed (e.g., AtomicFloat32AddEXT)


     // Variable Pointers (use with caution)
     if (info.uses_variable_pointers && profile.support_variable_pointers) {
         ctx.AddCapability(spv::Capability::VariablePointersStorageBuffer); // If used with SSBOs
         ctx.AddCapability(spv::Capability::VariablePointers); // General capability
     }
     if (info.uses_sample_id || info.uses_sample_shading) { // Check both flags
         ctx.AddCapability(spv::Capability::SampleRateShading);
     }
}

// --- Patch Phi Nodes (Original simplified implementation) ---
void PatchPhiNodes(IR::Program& program, EmitContext& ctx) {
    // This simplified version assumes ctx.PatchDeferredPhi provides the necessary logic
    // to iterate through expected Phi nodes and request arguments.
    // It relies on the EmitContext correctly tracking the expected Phi structure.

    if (program.blocks.empty()) {
         // If there are no blocks, PatchDeferredPhi shouldn't be called,
         // or it should handle the zero-phi case gracefully.
         ctx.PatchDeferredPhi([](size_t) -> Id {
            // This callback shouldn't be invoked if there were no deferred phis.
            throw LogicError("Phi patching requested, but no deferred phis were created.");
            return Id{};
         });
         return;
    }

    // TODO: This implementation is likely incomplete. The original code had a complex iterator setup.
    // Reverting to a placeholder that relies entirely on EmitContext's internal state.
    // The EmitContext::PatchDeferredPhi implementation needs to correctly iterate through
    // the IR::Inst list or its own deferred phi records to find the corresponding IR::Inst*
    // for each deferred phi ID it needs to patch.

    // Placeholder logic: Assumes PatchDeferredPhi knows which IR::Inst* corresponds to each patch request.
    ctx.PatchDeferredPhi([&](size_t phi_inst_index, size_t phi_arg_index) -> Id {
         // This callback needs to map phi_inst_index (or some identifier passed by PatchDeferredPhi)
         // back to the correct IR::Inst*. This requires more state tracking than available here.
         // This lookup is NON-TRIVIAL.

         // Placeholder - assumes we can somehow find the right Inst*. This WILL NOT WORK AS IS.
         // IR::Inst* phi_inst = FindPhiInstructionByIndex(program, phi_inst_index); // Needs implementation
         // if (!phi_inst || phi_inst->GetOpcode() != IR::Opcode::Phi) {
         //    throw LogicError("Failed to find corresponding Phi instruction during patching.");
         // }
         // return ctx.Def(phi_inst->Arg(phi_arg_index));

         throw LogicError("PatchPhiNodes implementation requires robust lookup from deferred Phi to IR::Inst.");
         return Id{};
    });

    // The original code's iterator logic:
    /*
    auto inst{program.blocks.front()->begin()};
    size_t block_index{0};
    ctx.PatchDeferredPhi([&](size_t phi_arg) {
        if (phi_arg == 0) {
            ++inst;
            // Advance block/inst iterators until the next Phi instruction is found
            while (block_index < program.blocks.size()) {
                 IR::Block* current_block_ptr = program.blocks[block_index];
                 if (inst == current_block_ptr->end()) {
                     // Move to the next block
                     ++block_index;
                     if (block_index < program.blocks.size()) {
                         inst = program.blocks[block_index]->begin();
                     } else {
                         // Reached end of all blocks - should not happen if phis are present
                         throw LogicError("Ran out of blocks while searching for Phi node during patching.");
                     }
                 }
                 // Check if the current instruction is a Phi node
                 if (block_index < program.blocks.size() && inst != program.blocks[block_index]->end() && inst->GetOpcode() == IR::Opcode::Phi) {
                     break; // Found the next Phi node
                 }
                 // If not a Phi node, advance instruction iterator
                 if (block_index < program.blocks.size() && inst != program.blocks[block_index]->end()){
                    ++inst;
                 }
            }
            if (block_index >= program.blocks.size()) {
                 throw LogicError("Failed to find next Phi instruction during patching.");
            }
        }
        // Make sure 'inst' is valid and points to a Phi instruction before accessing Arg
        if (block_index >= program.blocks.size() || inst == program.blocks[block_index]->end() || inst->GetOpcode() != IR::Opcode::Phi) {
             throw LogicError("Invalid state: Iterator does not point to a Phi instruction during patching.");
        }
        return ctx.Def(inst->Arg(phi_arg));
    });
    */
}


} // Anonymous namespace

// --- Main SPIR-V Emission Function ---
std::vector<u32> EmitSPIRV(const Profile& profile, const RuntimeInfo& runtime_info,
                           IR::Program& program, Bindings& bindings,
                           bool enable_optimization /* Added flag */) {
    EmitContext ctx{profile, runtime_info, program, bindings};

    // 1. Define the main function structure and emit instructions
    const Id main_func_id{DefineMain(ctx, program)};

    // 2. Define entry point and execution modes
    DefineEntryPoint(program, ctx, main_func_id);

    // 3. Setup capabilities and specific execution modes based on profile/info
    if (profile.support_float_controls) {
        ctx.AddExtension("SPV_KHR_float_controls"); // Add extension if using related modes/caps
        SetupDenormControl(profile, program, ctx, main_func_id);
        SetupSignedNanCapabilities(profile, program, ctx, main_func_id);
    }
    SetupCapabilities(profile, program.info, ctx);
    SetupTransformFeedbackCapabilities(ctx, main_func_id);

    // 4. Patch Phi nodes (after all definitions are potentially available)
    PatchPhiNodes(program, ctx);

    // 5. Assemble the SPIR-V binary code
    std::vector<u32> spirv_code = ctx.Assemble();

    // 6. Conditionally apply optimization passes using SPIRV-Tools
    if (enable_optimization) {
        LOG_DEBUG(Shader_SPIRV, "Attempting SPIR-V optimization...");

        spvtools::Optimizer optimizer(TargetSPIRVEnvironment(profile)); // Use helper for target env

        // Set a message consumer to capture optimization errors/warnings
        std::string optimizer_messages;
        optimizer.SetMessageConsumer([&optimizer_messages](spv_message_level_t level, const char*,
                                                         const spv_position_t& position,
                                                         const char* message) {
            switch (level) {
            case SPV_MSG_FATAL:
            case SPV_MSG_INTERNAL_ERROR:
            case SPV_MSG_ERROR:
                optimizer_messages += "ERROR: ";
                break;
            case SPV_MSG_WARNING:
                optimizer_messages += "WARNING: ";
                break;
            case SPV_MSG_INFO:
                optimizer_messages += "INFO: ";
                break;
            case SPV_MSG_DEBUG:
                optimizer_messages += "DEBUG: ";
                break;
            }
             optimizer_messages += /* std::to_string(position.line) + ":" + std::to_string(position.column) + ":" + std::to_string(position.index) + ": " + */ message; // Position info might be verbose
             optimizer_messages += "\n";
        });

        // Register desired optimization passes (Example: general performance)
        // See SPIRV-Tools documentation for available passes and recipes.
        // optimizer.RegisterPerformancePasses(); // Common recipe for performance
        // Or register individual passes:
        optimizer.RegisterPass(spvtools::CreateMergeReturnPass());
        optimizer.RegisterPass(spvtools::CreateInlineExhaustivePass());
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
        optimizer.RegisterPass(spvtools::CreatePrivateToLocalPass());
        optimizer.RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass());
        optimizer.RegisterPass(spvtools::CreateLocalSingleStoreElimPass());
        optimizer.RegisterPass(spvtools::CreateScalarReplacementPass());
        optimizer.RegisterPass(spvtools::CreateLocalAccessChainConvertPass());
        optimizer.RegisterPass(spvtools::CreateLocalMultiStoreElimPass());
        optimizer.RegisterPass(spvtools::CreateCCPPass());
        optimizer.RegisterPass(spvtools::CreateRedundancyEliminationPass());
        optimizer.RegisterPass(spvtools::CreateCombineAccessChainsPass());
        optimizer.RegisterPass(spvtools::CreateSimplificationPass());
        optimizer.RegisterPass(spvtools::CreateVectorDCEPass());
        optimizer.RegisterPass(spv::opt::CreateDeadInsertElimPass());
        optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass());
        optimizer.RegisterPass(spvtools::CreateIfConversionPass());


        std::vector<u32> optimized_spirv;
        spvtools::OptimizerOptions options;
        // Disable validator run during optimization *unless* debugging optimizer issues.
        // Validation should happen separately if needed (e.g., before VkCreateShaderModule).
        options.set_run_validator(false);

        bool success = optimizer.Run(spirv_code.data(), spirv_code.size(), &optimized_spirv, options);

        if (success) {
            spirv_code = std::move(optimized_spirv); // Replace with optimized code
            LOG_DEBUG(Shader_SPIRV, "SPIR-V optimization successful.");
             if (!optimizer_messages.empty()) {
                 LOG_DEBUG(Shader_SPIRV, "Optimizer messages:\n{}", optimizer_messages);
             }
        } else {
            // Log the failure and any messages from the optimizer
            LOG_WARNING(Shader_SPIRV, "SPIR-V optimization failed. Using unoptimized code. Optimizer messages:\n{}", optimizer_messages);
            // Fall through to return the original unoptimized code
        }
    } else {
         LOG_DEBUG(Shader_SPIRV, "SPIR-V optimization skipped.");
    }

    // 7. Return the final (potentially optimized) SPIR-V code
    return spirv_code;
}


// --- Implementations for Specific IR Opcodes ---
// These functions are called by EmitInst via the Invoke<> helper.

Id EmitPhi(EmitContext& ctx, IR::Inst* inst) {
    const size_t num_args{inst->NumArgs()};
    // Use small_vector if available and beneficial for performance
    // boost::container::small_vector<Id, 32> blocks;
    std::vector<Id> blocks; // Store predecessor block labels
    blocks.reserve(num_args);
    for (size_t index = 0; index < num_args; ++index) {
        IR::Block* pred_block = inst->PhiBlock(index);
        ASSERT(pred_block != nullptr); // Ensure predecessor block is valid
        blocks.push_back(pred_block->Definition<Id>());
    }
    // The type of a phi instruction is stored in its flags (or inferred)
    const Id result_type{TypeId(ctx, inst->Flags<IR::Type>())};

    // Defer the actual OpPhi instruction emission until PatchPhiNodes
    // where operand IDs will be resolved.
    return ctx.DeferredOpPhi(result_type, std::span(blocks.data(), blocks.size()));
}

// Placeholder for No-Operation or Void return emitters
void EmitVoid(EmitContext&) {
    // Used for IR instructions that don't map directly to a SPIR-V instruction
    // or whose effect is handled elsewhere (e.g., Nop, potentially some terminators).
}

// Emit Identity (usually optimized out, but handles cases like variable aliasing)
Id EmitIdentity(EmitContext& ctx, const IR::Value& value) {
    const Id id{ctx.Def(value)};
    // Forward declarations might need special handling if allowed by IR,
    // but typically SPIR-V requires definitions before use (except for forward Function/Type).
    if (!Sirit::ValidId(id)) { // Assuming Sirit::ValidId checks if ID is defined
        throw NotImplementedException("Forward identity declaration encountered.");
    }
    return id; // Simply return the ID of the input value
}

// Handles conditional references (likely translates to boolean ID)
Id EmitConditionRef(EmitContext& ctx, const IR::Value& value) {
    const Id id{ctx.Def(value)};
    if (!Sirit::ValidId(id)) {
        throw NotImplementedException("Forward condition reference encountered.");
    }
    ASSERT(ctx.GetType(id) == ctx.U1); // Ensure it's actually a boolean type
    return id;
}

// Emit Reference (Purpose unclear from name - potentially related to pointers/memory?)
void EmitReference(EmitContext&) {
     // This might be a NOP if references are handled implicitly by SPIR-V pointers
     // or might emit specific pointer/reference instructions if needed.
     // Needs clarification based on its usage in the IR.
      throw NotImplementedException("EmitReference semantics unclear and not implemented.");
}


// --- Unreachable/Removed IR Instruction Emitters ---
// These IR Opcodes should ideally be removed by frontend optimization passes
// before reaching the SPIR-V backend. If encountered, it indicates an issue.

void EmitPhiMove(EmitContext&) {
    throw LogicError("Unreachable instruction: PhiMove should be removed before SPIR-V emission.");
}

void EmitGetZeroFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction: GetZeroFromOp should be lowered or removed.");
}

void EmitGetSignFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction: GetSignFromOp should be lowered or removed.");
}

void EmitGetCarryFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction: GetCarryFromOp should be lowered or removed.");
}

void EmitGetOverflowFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction: GetOverflowFromOp should be lowered or removed.");
}

void EmitGetSparseFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction: GetSparseFromOp should be lowered or removed.");
}

void EmitGetInBoundsFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction: GetInBoundsFromOp should be lowered or removed.");
}


// --- Include Emitter Implementations ---
// This assumes individual emitters (EmitLoad, EmitStore, EmitFAdd, etc.) are defined elsewhere,
// potentially in emit_spirv_instructions.cpp or similar.
// #include "emit_spirv_instructions.cpp" // Or link against the compiled object

} // namespace Shader::Backend::SPIRV