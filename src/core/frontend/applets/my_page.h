// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>

#include "core/frontend/applets/applet.h"
#include "core/hle/result.h"

namespace Core::Frontend {

class MyPageApplet : public Applet {
public:
    virtual ~MyPageApplet();
};

class DefaultMyPageApplet final : public MyPageApplet {
public:
    void Close() const override;
};

} // namespace Core::Frontend
