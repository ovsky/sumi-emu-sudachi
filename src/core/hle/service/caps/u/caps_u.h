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

class IAlbumApplicationService final : public ServiceFramework<IAlbumApplicationService> {
public:
    explicit IAlbumApplicationService(Core::System& system_,
                                      std::shared_ptr<AlbumManager> album_manager);
    ~IAlbumApplicationService() override;

private:
    Result SetShimLibraryVersion(ShimLibraryVersion version, AppletResourceUserId id,
                                 ClientProcessId pid); // 7.0.0+
    Result GetAlbumFileList0AafeAruidDeprecated(
        ContentType contents,
        OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries,
        AppletResourceUserId id, ClientProcessId pid, s64 start_time, s64 end_time,
        Out<u64> out_entries_count);
    Result DeleteAlbumFileByAruid(ContentType contents, ClientProcessId pid,
                                  AppletResourceUserId id, ApplicationAlbumFileEntry entry);
    Result GetAlbumFileSizeByAruid(ClientProcessId pid, ApplicationAlbumFileEntry entry,
                                   AppletResourceUserId id, Out<u64> out_size);
    Result DeleteAlbumFileByAruidForDebug(ClientProcessId pid, ApplicationAlbumFileEntry entry,
                                          AppletResourceUserId id);
    Result LoadAlbumScreenShotImageByAruid(
        ClientProcessId pid, OutBuffer<BufferAttr_HipcMapAlias> out_image,
        OutBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> out_data,
        OutBuffer<BufferAttr_HipcMapAlias> out_work_buffer, ApplicationAlbumFileEntry entry,
        ScreenShotDecodeOption option);
    Result LoadAlbumScreenShotThumbnailImageByAruid(
        ClientProcessId pid, OutBuffer<BufferAttr_HipcMapAlias> out_image,
        OutBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> out_data,
        OutBuffer<BufferAttr_HipcMapAlias> out_work_buffer, ApplicationAlbumFileEntry entry,
        ScreenShotDecodeOption option);
    Result PrecheckToCreateContentsByAruid(ContentType contents, ClientProcessId pid, u64 unknown1,
                                           AppletResourceUserId id);
    Result GetAlbumFileList1AafeAruidDeprecated(
        ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
        AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
        OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries); // 6.0.0+
    Result GetAlbumFileList2AafeUidAruidDeprecated(
        ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
        AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
        OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries,
        UserId uid); // 6.0.0+
    Result GetAlbumFileList3AaeAruid(
        ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
        AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
        OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries); // 7.0.0-16.1.0
    // Result GetAlbumFileList3AaeAruidDeprecated(
    //     OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries,
    //     ClientProcessId pid, ContentType contents, AlbumFileDateTime start_time,
    //     AlbumFileDateTime end_time, AppletResourceUserId id,
    //     Out<u64> out_entries_count); // 17.0.0+
    Result GetAlbumFileList4AaeUidAruid(
        ContentType contents, AlbumFileDateTime start_time, AlbumFileDateTime end_time,
        AppletResourceUserId id, ClientProcessId pid, Out<u64> out_entries_count,
        OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries,
        UserId uid); // 7.0.0-16.1.0
    // Result GetAlbumFileList4AaeUidAruidDeprecated(
    //     OutArray<ApplicationAlbumFileEntry, BufferAttr_HipcMapAlias> out_entries,
    //     ClientProcessId pid, ContentType contents, AlbumFileDateTime start_time,
    //     AlbumFileDateTime end_time, UserId uid, AppletResourceUserId id,
    //     Out<u64> out_entries_count);                 // 17.0.0+
    Result GetAllAlbumFileList3AaeAruid(); // 11.0.0-16.1.0
    // Result GetAllAlbumFileList3AaeAruidDeprecated(); // 17.0.0+
    Result GetAlbumFileList5AaeAruid();    // 17.0.0+
    Result GetAlbumFileList6AaeUidAruid(); // 17.0.0+
    Result GetAllAlbumFileList5AaeAruid(); // 17.0.0+
    Result Unknown148();                   // 18.0.0+
    Result OpenAccessorSessionForApplication();

    std::shared_ptr<AlbumManager> _manager;
};
} // namespace Service::Capture