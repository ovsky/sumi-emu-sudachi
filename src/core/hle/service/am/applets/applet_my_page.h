// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "common/common_funcs.h"
#include "core/hle/service/am/applets/applets.h"

namespace Core {
class System;
}

namespace Service::AM::Applets {

class MyPage final : public Applet {
public:
    explicit MyPage(Core::System& system_, LibraryAppletMode applet_mode_,
                    const Core::Frontend::MyPageApplet& frontend_);
    ~MyPage() override;

    void Initialize() override;

    bool TransactionComplete() const override;
    Result GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;
    Result RequestExit() override;

private:
    const Core::Frontend::MyPageApplet& frontend;
    Core::System& system;
};

} // namespace Service::AM::Applets
