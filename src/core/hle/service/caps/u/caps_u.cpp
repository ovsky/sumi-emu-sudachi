#include "common/logging/log.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_result.h"
#include "core/hle/service/caps/u/caps_u.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Capture {

IAlbumApplicationService::IAlbumApplicationService(Core::System& system_,
                                                   std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:u"}, _manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {32, C<&IAlbumApplicationService::SetShimLibraryVersion>, "SetShimLibraryVersion"},
        {102, C<&IAlbumApplicationService::GetAlbumFileList0AafeAruidDeprecated>, "GetAlbumFileList0AafeAruidDeprecated"},
        {103, C<&IAlbumApplicationService::DeleteAlbumFileByAruid>, "DeleteAlbumFileByAruid"},
        {104, C<&IAlbumApplicationService::GetAlbumFileSizeByAruid>, "GetAlbumFileSizeByAruid"},
        {105, C<&IAlbumApplicationService::DeleteAlbumFileByAruidForDebug>, "DeleteAlbumFileByAruidForDebug"},
        {110, C<&IAlbumApplicationService::LoadAlbumScreenShotImageByAruid>, "LoadAlbumScreenShotImageByAruid"},
        {120, C<&IAlbumApplicationService::LoadAlbumScreenShotThumbnailImageByAruid>, "LoadAlbumScreenShotThumbnailImageByAruid"},
        {130, C<&IAlbumApplicationService::PrecheckToCreateContentsByAruid>, "PrecheckToCreateContentsByAruid"},
        {140, C<&IAlbumApplicationService::GetAlbumFileList1AafeAruidDeprecated>, "GetAlbumFileList1AafeAruidDeprecated"},
        {141, C<&IAlbumApplicationService::GetAlbumFileList2AafeUidAruidDeprecated>, "GetAlbumFileList2AafeUidAruidDeprecated"},
        {142, C<&IAlbumApplicationService::GetAlbumFileList3AaeAruid>, "GetAlbumFileList3AaeAruid"},
        {143, C<&IAlbumApplicationService::GetAlbumFileList4AaeUidAruid>, "GetAlbumFileList4AaeUidAruid"},
        {144, C<&IAlbumApplicationService::GetAllAlbumFileList3AaeAruid>, "GetAllAlbumFileList3AaeAruid"},
        {145, C<&IAlbumApplicationService::GetAlbumFileList5AaeAruid>, "GetAlbumFileList5AaeAruid"},
        {146, C<&IAlbumApplicationService::GetAlbumFileList6AaeUidAruid>, "GetAlbumFileList6AaeUidAruid"},
        {147, C<&IAlbumApplicationService::GetAllAlbumFileList5AaeAruid>, "GetAllAlbumFileList5AaeAruid"},
        {148, C<&IAlbumApplicationService::Unknown148>, "Unknown148"},
        {60002, C<&IAlbumApplicationService::OpenAccessorSessionForApplication>, "OpenAccessorSessionForApplication"}
    };
    // clang-format on

    RegisterHandlers(functions);
} // namespace Service::Capture

IAlbumApplicationService::~IAlbumApplicationService() = default;

Result IAlbumApplicationService::SetShimLibraryVersion(ShimLibraryVersion version,
                                                       AppletResourceUserId id,
                                                       ClientProcessId pid) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAlbumFileList0AafeAruidDeprecated(
    ContentType contents, OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries,
    AppletResourceUserId id, ClientProcessId pid, s64 start_time, s64 end_time,
    Out<u64> out_entries_count) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::DeleteAlbumFileByAruid(ContentType contents, ClientProcessId pid,
                                                        AppletResourceUserId id,
                                                        ApplicationAlbumFileEntry entry) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAlbumFileSizeByAruid(ClientProcessId pid,
                                                         ApplicationAlbumFileEntry entry,
                                                         AppletResourceUserId id,
                                                         Out<u64> out_size) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::DeleteAlbumFileByAruidForDebug(ClientProcessId pid,
                                                                ApplicationAlbumFileEntry entry,
                                                                AppletResourceUserId id) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::LoadAlbumScreenShotImageByAruid(
    ClientProcessId pid, OutBuffer<BufferAttr_HipcMapAlias> out_image,
    OutBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> out_data,
    OutBuffer<BufferAttr_HipcMapAlias> out_work_buffer, ApplicationAlbumFileEntry entry,
    ScreenShotDecodeOption option) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::LoadAlbumScreenShotThumbnailImageByAruid(
    ClientProcessId pid, OutBuffer<BufferAttr_HipcMapAlias> out_image,
    OutBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> out_data,
    OutBuffer<BufferAttr_HipcMapAlias> out_work_buffer, ApplicationAlbumFileEntry entry,
    ScreenShotDecodeOption option) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::PrecheckToCreateContentsByAruid(ContentType contents,
                                                                 ClientProcessId pid, u64 unknown1,
                                                                 AppletResourceUserId id) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAlbumFileList1AafeAruidDeprecated(
    ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
    AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
    OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAlbumFileList2AafeUidAruidDeprecated(
    ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
    AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
    OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries, UserId uid) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAlbumFileList3AaeAruid(
    ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
    AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
    OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

// Result IAlbumApplicationService::GetAlbumFileList3AaeAruidDeprecated(
//     OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries, ClientProcessId
//     pid, ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
//     AppletResourceUserId id, Out<u64> out_entries_count) {
//     LOG_DEBUG(Service_Capture, "(STUBBED) called.");
//
//     R_SUCCEED();
// }

Result IAlbumApplicationService::GetAlbumFileList4AaeUidAruid(
    ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
    AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
    OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries, UserId uid) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

// Result IAlbumApplicationService::GetAlbumFileList4AaeUidAruidDeprecated(
//     OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries, ClientProcessId
//     pid, ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time, UserId
//     uid, AppletResourceUserId id, Out<u64> out_entries_count) { LOG_DEBUG(Service_Capture,
//     "(STUBBED) called.");
//
//     R_SUCCEED();
// }

Result IAlbumApplicationService::GetAllAlbumFileList3AaeAruid() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

// Result IAlbumApplicationService::GetAllAlbumFileList3AaeAruidDeprecated() {
//     LOG_DEBUG(Service_Capture, "(STUBBED) called.");
//
//     R_SUCCEED();
// }

Result IAlbumApplicationService::GetAlbumFileList5AaeAruid() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAlbumFileList6AaeUidAruid() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::GetAllAlbumFileList5AaeAruid() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::Unknown148() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

Result IAlbumApplicationService::OpenAccessorSessionForApplication() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");

    R_SUCCEED();
}

} // namespace Service::Capture