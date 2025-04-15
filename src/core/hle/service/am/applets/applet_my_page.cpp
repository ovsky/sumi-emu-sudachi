#include <cstring>

#include "common/assert.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/my_page.h"
#include "core/hle/service/acc/errors.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/applet_my_page.h"

namespace Service::AM::Applets {

MyPage::MyPage(Core::System& system_, LibraryAppletMode applet_mode_,
               const Core::Frontend::MyPageApplet& frontend_)
    : Applet{system_, applet_mode_}, frontend{frontend_}, system{system_} {}

MyPage::~MyPage() = default;

void MyPage::Initialize() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)
}

bool MyPage::TransactionComplete() const {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    return false;
};

Result MyPage::GetStatus() const {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    R_SUCCEED();
};

void MyPage::ExecuteInteractive() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)
};

void MyPage::Execute() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)
};

Result MyPage::RequestExit() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    R_SUCCEED();
};

} // namespace Service::AM::Applets