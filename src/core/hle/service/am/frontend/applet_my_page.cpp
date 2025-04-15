#include <cstring>

#include "common/assert.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/my_page.h"
#include "core/hle/service/acc/errors.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/frontend/applet_my_page.h"
#include "core/hle/service/am/service/storage.h"

namespace Service::AM::Frontend {

MyPage::MyPage(Core::System& system_, std::shared_ptr<Applet> applet_,
               LibraryAppletMode applet_mode_, const Core::Frontend::MyPageApplet& frontend_)
    : FrontendApplet{system_, applet_, applet_mode_}, frontend{frontend_} {}

MyPage::~MyPage() = default;

void MyPage::Initialize() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    FrontendApplet::Initialize();
}

Result MyPage::GetStatus() const {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    R_SUCCEED();
}

void MyPage::ExecuteInteractive() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    ASSERT_MSG(false, "Attempted to call interactive execution on non-interactive applet.");
}

void MyPage::Execute() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)
}

Result MyPage::RequestExit() {
    LOG_DEBUG(Service_AM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    frontend.Close();

    R_SUCCEED();
}
} // namespace Service::AM::Frontend