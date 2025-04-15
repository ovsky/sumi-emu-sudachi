#pragma once

#include "core/hle/result.h"
#include "core/hle/service/am/frontend/applets.h"

namespace Core {
class System;
}

namespace Service::AM::Frontend {

enum class MyPageAppletVersion : u32 {
    Version2 = 0x10000,
};

enum class MyPageAppletType : u32 {
    ShowMyProfile = 7,
};

struct Arg {
    MyPageAppletType type;
    INSERT_PADDING_BYTES(0x4);
    u128 uid;
};
static_assert(sizeof(Arg) == 0x18, "Arg is an invalid size");

class MyPage final : public FrontendApplet {
public:
    explicit MyPage(Core::System& system_, std::shared_ptr<Applet> applet_,
                    LibraryAppletMode applet_mode_, const Core::Frontend::MyPageApplet& frontend_);
    ~MyPage() override;

    void Initialize() override;

    Result GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;
    Result RequestExit() override;

private:
    const Core::Frontend::MyPageApplet& frontend;
};

} // namespace Service::AM::Frontend