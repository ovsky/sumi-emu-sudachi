// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "core/frontend/emu_window.h"
#include "sudachi_cmd/emu_window/emu_window_sdl3.h"

namespace Core {
class System;
}

namespace InputCommon {
class InputSubsystem;
}

class EmuWindow_SDL3_Null final : public EmuWindow_SDL3 {
public:
    explicit EmuWindow_SDL3_Null(InputCommon::InputSubsystem* input_subsystem_,
                                 Core::System& system, bool fullscreen);
    ~EmuWindow_SDL3_Null() override;

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;
};
