#include "common/logging/log.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_result.h"
#include "core/hle/service/caps/su/caps_su.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Capture {

IScreenShotApplicationService::IScreenShotApplicationService(
    Core::System& system_, std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:su"}, _manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {32, C<&IScreenShotApplicationService::SetShimLibraryVersion>, "SetShimLibraryVersion"},
        {201, C<&IScreenShotApplicationService::SaveScreenShot>, "SaveScreenShot"},
        {203, C<&IScreenShotApplicationService::SaveScreenShotEx0>, "SaveScreenShotEx0"},
        {205, C<&IScreenShotApplicationService::SaveScreenShotEx1>, "SaveScreenShotEx1"},
        {210, C<&IScreenShotApplicationService::SaveScreenShotEx2>, "SaveScreenShotEx2"}
    };
    // clang-format on

    RegisterHandlers(functions);
} // namespace Service::Capture

IScreenShotApplicationService::~IScreenShotApplicationService() = default;

void IScreenShotApplicationService::CaptureAndSaveScreenShot() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
}

Result IScreenShotApplicationService::SetShimLibraryVersion(ShimLibraryVersion version,
                                                            AppletResourceUserId id) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IScreenShotApplicationService::SaveScreenShot(
    u32 unknown1, u32 unknown2, AppletResourceUserId id, ClientProcessId pid,
    InBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> data) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IScreenShotApplicationService::SaveScreenShotEx0() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IScreenShotApplicationService::SaveScreenShotEx1() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IScreenShotApplicationService::SaveScreenShotEx2() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

} // namespace Service::Capture