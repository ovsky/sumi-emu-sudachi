#include "common/logging/log.h"
#include "core/hle/service/caps/a/caps_a.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_result.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Capture {

IAlbumAccessorService::IAlbumAccessorService(Core::System& system_,
                                             std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:a"}, _manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, C<&IAlbumAccessorService::GetAlbumFileCount>, "GetAlbumFileCount"},
        {1, C<&IAlbumAccessorService::GetAlbumFileList>, "GetAlbumFileList"},
        {2, C<&IAlbumAccessorService::LoadAlbumFile>, "LoadAlbumFile"},
        {3, C<&IAlbumAccessorService::DeleteAlbumFile>, "DeleteAlbumFile"},
        {4, C<&IAlbumAccessorService::StorageCopyAlbumFile>, "StorageCopyAlbumFile"},
        {5, C<&IAlbumAccessorService::IsAlbumMounted>, "IsAlbumMounted"},
        {6, C<&IAlbumAccessorService::GetAlbumUsage>, "GetAlbumUsage"},
        {7, C<&IAlbumAccessorService::GetAlbumFileSize>, "GetAlbumFileSize"},
        {8, C<&IAlbumAccessorService::LoadAlbumFileThumbnail>, "LoadAlbumFileThumbnail"},
        {9, C<&IAlbumAccessorService::LoadAlbumScreenShotImage>, "LoadAlbumScreenShotImage"},
        {10, C<&IAlbumAccessorService::LoadAlbumScreenShotThumbnailImage>, "LoadAlbumScreenShotThumbnailImage"},
        {11, C<&IAlbumAccessorService::GetAlbumEntryFromApplicationAlbumEntry>, "GetAlbumEntryFromApplicationAlbumEntry"},
        {12, C<&IAlbumAccessorService::LoadAlbumScreenShotImageEx>, "LoadAlbumScreenShotImageEx"},
        {13, C<&IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx>, "LoadAlbumScreenShotThumbnailImageEx"},
        {14, C<&IAlbumAccessorService::LoadAlbumScreenShotImageEx0>, "LoadAlbumScreenShotImageEx0"},
        {15, C<&IAlbumAccessorService::GetAlbumUsage3>, "GetAlbumUsage3"},
        {16, C<&IAlbumAccessorService::GetAlbumMountResult>, "GetAlbumMountResult"},
        {17, C<&IAlbumAccessorService::GetAlbumUsage16>, "GetAlbumUsage16"},
        {18, C<&IAlbumAccessorService::GetAppletProgramIdTable>, "GetAppletProgramIdTable"},
        {19, C<&IAlbumAccessorService::GetAlbumFileName>, "GetAlbumFileName"},
        {100, C<&IAlbumAccessorService::GetAlbumFileCountEx0>, "GetAlbumFileCountEx0"},
        {101, C<&IAlbumAccessorService::GetAlbumFileListEx0>, "GetAlbumFileListEx0"},
        {110, C<&IAlbumAccessorService::GetAlbumFileListEx1>, "GetAlbumFileListEx1"},
        {120, C<&IAlbumAccessorService::GetAlbumFileListEx2>, "GetAlbumFileListEx2"},
        {130, C<&IAlbumAccessorService::LoadAlbumFileRawData>, "LoadAlbumFileRawData"},
        {140, C<&IAlbumAccessorService::GetAlbumFileCreatedEvent>, "GetAlbumFileCreatedEvent"},
        {141, C<&IAlbumAccessorService::Unknown141>, "Unknown141"},
        {150, C<&IAlbumAccessorService::LoadAlbumSystemReservedInfo>, "LoadAlbumSystemReservedInfo"},
        {151, C<&IAlbumAccessorService::Unknown151>, "Unknown151"},
        {160, C<&IAlbumAccessorService::Unknown160>, "Unknown160"},
        {202, C<&IAlbumAccessorService::SaveEditedScreenShot>, "SaveEditedScreenShot"},
        {301, C<&IAlbumAccessorService::GetLastOverlayScreenShotThumbnail>, "GetLastOverlayScreenShotThumbnail"},
        {302, C<&IAlbumAccessorService::GetLastOverlayMovieThumbnail>, "GetLastOverlayMovieThumbnail"},
        {401, C<&IAlbumAccessorService::GetAutoSavingStorage>, "GetAutoSavingStorage"},
        {501, C<&IAlbumAccessorService::GetRequiredStorageSpaceSizeToCopyAll>, "GetRequiredStorageSpaceSizeToCopyAll"},
        {1001, C<&IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx0>, "LoadAlbumScreenShotThumbnailImageEx0"},
        {1002, C<&IAlbumAccessorService::LoadAlbumScreenShotImageEx1>, "LoadAlbumScreenShotImageEx1"},
        {1003, C<&IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx1>, "LoadAlbumScreenShotThumbnailImageEx1"},
        {8001, C<&IAlbumAccessorService::ForceAlbumUnmounted>, "ForceAlbumUnmounted"},
        {8002, C<&IAlbumAccessorService::ResetAlbumMountStatus>, "ResetAlbumMountStatus"},
        {8011, C<&IAlbumAccessorService::RefreshAlbumCache>, "RefreshAlbumCache"},
        {8012, C<&IAlbumAccessorService::GetAlbumCache>, "GetAlbumCache"},
        {8013, C<&IAlbumAccessorService::GetAlbumCacheEx>, "GetAlbumCacheEx"},
        {8021, C<&IAlbumAccessorService::GetAlbumEntryFromApplicationAlbumEntryAruid>, "GetAlbumEntryFromApplicationAlbumEntryAruid"},
        {8022, C<&IAlbumAccessorService::Unknown8022>, "Unknown8022"},
        {10011, C<&IAlbumAccessorService::SetInternalErrorConversionEnabled>, "SetInternalErrorConversionEnabled"},
        {50000, C<&IAlbumAccessorService::LoadMakerNoteInfoForDebug>, "LoadMakerNoteInfoForDebug"},
        {50001, C<&IAlbumAccessorService::Unknown50001>, "Unknown50001"},
        {50011, C<&IAlbumAccessorService::GetAlbumAccessResultForDebug>, "GetAlbumAccessResultForDebug"},
        {50012, C<&IAlbumAccessorService::SetAlbumAccessResultForDebug>, "SetAlbumAccessResultForDebug"},
        {60002, C<&IAlbumAccessorService::OpenAccessorSession>, "OpenAccessorSession"}
    };
    // clang-format on

    RegisterHandlers(functions);
}

IAlbumAccessorService::~IAlbumAccessorService() = default;

Result IAlbumAccessorService::GetAlbumFileCount() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileList(
    Out<u64> out_count, AlbumStorage album_storage,
    OutArray<AlbumEntry, BufferAttr_HipcMapAlias> out_album_entries) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_RETURN(TranslateResult(
        _manager->GetAlbumFileList(out_album_entries, *out_count, album_storage, 0)));
}

Result IAlbumAccessorService::LoadAlbumFile() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::DeleteAlbumFile(AlbumFileId album_file_id) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_RETURN(TranslateResult(_manager->DeleteAlbumFile(album_file_id)));
}

Result IAlbumAccessorService::StorageCopyAlbumFile() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::IsAlbumMounted(Out<bool> out_is_album_mounted,
                                             AlbumStorage album_storage) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    const auto result = _manager->IsAlbumMounted(album_storage);
    *out_is_album_mounted = result.IsSuccess();
    R_RETURN(TranslateResult(result));
}

Result IAlbumAccessorService::GetAlbumUsage() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileSize() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumFileThumbnail() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotImage() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotThumbnailImage() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumEntryFromApplicationAlbumEntry() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotImageEx() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotImageEx0() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumUsage3() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumMountResult() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumUsage16() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAppletProgramIdTable(
    Out<u32> out_buffer_size,
    OutArray<u8, BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_buffer) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    *out_buffer_size = 0;
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileName() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileCountEx0() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileListEx0(
    Out<u64> out_count, AlbumStorage album_storage, u8 flags,
    OutArray<AlbumEntry, BufferAttr_HipcMapAlias> out_album_entries) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_RETURN(TranslateResult(
        _manager->GetAlbumFileList(out_album_entries, *out_count, album_storage, flags)));
}

Result IAlbumAccessorService::GetAlbumFileListEx1() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileListEx2() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumFileRawData() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumFileCreatedEvent() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::Unknown141() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumSystemReservedInfo() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::Unknown151() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::Unknown160() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::SaveEditedScreenShot() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetLastOverlayScreenShotThumbnail() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetLastOverlayMovieThumbnail() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAutoSavingStorage(Out<bool> out_is_autosaving_storage) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_RETURN(TranslateResult(_manager->GetAutoSavingStorage(*out_is_autosaving_storage)));
}

Result IAlbumAccessorService::GetRequiredStorageSpaceSizeToCopyAll() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx0() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadAlbumScreenShotImageEx1(
    const AlbumFileId& album_file_id, const ScreenShotDecodeOption& screenshot_decoder_options,
    OutLargeData<LoadAlbumScreenShotImageOutput, BufferAttr_HipcMapAlias> out_image_output,
    OutArray<u8, BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_image,
    OutArray<u8, BufferAttr_HipcMapAlias> out_buffer) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_RETURN(TranslateResult(_manager->LoadAlbumScreenShotImage(
        *out_image_output, out_image, album_file_id, screenshot_decoder_options)));
}

Result IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx1(
    const AlbumFileId& album_file_id, const ScreenShotDecodeOption& screenshot_decoder_options,
    OutLargeData<LoadAlbumScreenShotImageOutput, BufferAttr_HipcMapAlias> out_image_output,
    OutArray<u8, BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_image,
    OutArray<u8, BufferAttr_HipcMapAlias> out_buffer) {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_RETURN(TranslateResult(_manager->LoadAlbumScreenShotThumbnailImage(
        *out_image_output, out_image, album_file_id, screenshot_decoder_options)));
}

Result IAlbumAccessorService::ForceAlbumUnmounted() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::ResetAlbumMountStatus() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::RefreshAlbumCache() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumCache() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumCacheEx() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumEntryFromApplicationAlbumEntryAruid() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::Unknown8022() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::SetInternalErrorConversionEnabled() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::LoadMakerNoteInfoForDebug() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::Unknown50001() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::GetAlbumAccessResultForDebug() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::SetAlbumAccessResultForDebug() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::OpenAccessorSession() {
    LOG_DEBUG(Service_Capture, "(STUBBED) called.");
    R_SUCCEED();
}

Result IAlbumAccessorService::TranslateResult(Result in_result) {
    if (in_result.IsSuccess()) {
        return in_result;
    }

    if ((in_result.raw & 0x3801ff) == ResultUnknown1024.raw) {
        if (in_result.GetDescription() - 0x514 < 100) {
            return ResultInvalidFileData;
        }
        if (in_result.GetDescription() - 0x5dc < 100) {
            return ResultInvalidFileData;
        }

        if (in_result.GetDescription() - 0x578 < 100) {
            if (in_result == ResultFileCountLimit) {
                return ResultUnknown22;
            }
            return ResultUnknown25;
        }

        if (in_result.raw < ResultUnknown1801.raw) {
            if (in_result == ResultUnknown1202) {
                return ResultUnknown810;
            }
            if (in_result == ResultUnknown1203) {
                return ResultUnknown810;
            }
            if (in_result == ResultUnknown1701) {
                return ResultUnknown5;
            }
        } else if (in_result.raw < ResultUnknown1803.raw) {
            if (in_result == ResultUnknown1801) {
                return ResultUnknown5;
            }
            if (in_result == ResultUnknown1802) {
                return ResultUnknown6;
            }
        } else {
            if (in_result == ResultUnknown1803) {
                return ResultUnknown7;
            }
            if (in_result == ResultUnknown1804) {
                return OutOfRange;
            }
        }
        return ResultUnknown1024;
    }

    if (in_result.GetModule() == ErrorModule::FS) {
        if ((in_result.GetDescription() >> 0xc < 0x7d) ||
            (in_result.GetDescription() - 1000 < 2000) ||
            (((in_result.GetDescription() - 3000) >> 3) < 0x271)) {
            // TODO: Translate FS error
            return in_result;
        }
    }

    return in_result;
}

} // namespace Service::Capture