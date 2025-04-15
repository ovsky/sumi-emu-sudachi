#pragma once

#include <memory>

#include "core/hle/service/caps/caps_types.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {
class AlbumManager;

class IScreenShotApplicationService final : public ServiceFramework<IScreenShotApplicationService> {
public:
    explicit IScreenShotApplicationService(Core::System& system_,
                                           std::shared_ptr<AlbumManager> album_manager);
    ~IScreenShotApplicationService() override;

    void CaptureAndSaveScreenShot();

private:
    Result SetShimLibraryVersion(ShimLibraryVersion version,
                                 AppletResourceUserId id); // 7.0.0+
    Result SaveScreenShot(
        u32 unknown1, u32 unknown2, AppletResourceUserId id, ClientProcessId pid,
        InBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> data);
    Result SaveScreenShotEx0();
    Result SaveScreenShotEx1(); // 8.0.0+
    Result SaveScreenShotEx2();

    std::shared_ptr<AlbumManager> _manager;
};
} // namespace Service::Capture