#pragma once

#include "core/hle/service/caps/caps_types.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {
class AlbumManager;

class IAlbumAccessorService final : public ServiceFramework<IAlbumAccessorService> {
public:
    explicit IAlbumAccessorService(Core::System& system_,
                                   std::shared_ptr<AlbumManager> album_manager);
    ~IAlbumAccessorService() override;

private:
    Result GetAlbumFileCount();
    Result GetAlbumFileList(Out<u64> out_count, AlbumStorage album_storage,
                            OutArray<AlbumEntry, BufferAttr_HipcMapAlias> out_album_entries);
    Result LoadAlbumFile();
    Result DeleteAlbumFile(AlbumFileId album_file_id);
    Result StorageCopyAlbumFile();
    Result IsAlbumMounted(Out<bool> out_is_album_mounted, AlbumStorage album_storage);
    Result GetAlbumUsage();
    Result GetAlbumFileSize();
    Result LoadAlbumFileThumbnail();
    Result LoadAlbumScreenShotImage();               // 2.0.0+
    Result LoadAlbumScreenShotThumbnailImage();      // 2.0.0+
    Result GetAlbumEntryFromApplicationAlbumEntry(); // 2.0.0+
    Result LoadAlbumScreenShotImageEx();             // 3.0.0+
    Result LoadAlbumScreenShotThumbnailImageEx();    // 3.0.0+
    Result LoadAlbumScreenShotImageEx0();            // 3.0.0+
    Result GetAlbumUsage3();                         // 4.0.0+
    Result GetAlbumMountResult();                    // 4.0.0+
    Result GetAlbumUsage16();                        // 4.0.0+
    Result GetAppletProgramIdTable(
        Out<u32> out_buffer_size,
        OutArray<u8, BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure>
            out_buffer);           // 6.0.0+
    Result GetAlbumFileName();     // 11.0.0+
    Result GetAlbumFileCountEx0(); // 5.0.0+
    Result GetAlbumFileListEx0(
        Out<u64> out_count, AlbumStorage album_storage, u8 flags,
        OutArray<AlbumEntry, BufferAttr_HipcMapAlias> out_album_entries); // 5.0.0+
    Result GetAlbumFileListEx1();                                         // 15.0.0+
    Result GetAlbumFileListEx2();                                         // 17.0.0+
    Result LoadAlbumFileRawData();                                        // 17.0.0+
    Result GetAlbumFileCreatedEvent();                                    // 17.0.0+
    Result Unknown141();                                                  // 18.0.0+
    Result LoadAlbumSystemReservedInfo();                                 // 17.0.0+
    Result Unknown151();                                                  // 18.0.0+
    Result Unknown160();                                                  // 18.0.0+
    Result SaveEditedScreenShot();                                        // 1.0.0-2.3.0
    Result GetLastOverlayScreenShotThumbnail();
    Result GetLastOverlayMovieThumbnail(); // 4.0.0+
    Result GetAutoSavingStorage(Out<bool> out_is_autosaving_storage);
    Result GetRequiredStorageSpaceSizeToCopyAll();
    Result LoadAlbumScreenShotThumbnailImageEx0(); // 3.0.0+
    Result LoadAlbumScreenShotImageEx1(
        const AlbumFileId& album_file_id, const ScreenShotDecodeOption& screenshot_decoder_options,
        OutLargeData<LoadAlbumScreenShotImageOutput, BufferAttr_HipcMapAlias> out_image_output,
        OutArray<u8, BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_image,
        OutArray<u8, BufferAttr_HipcMapAlias> out_buffer); // 3.0.0+
    Result LoadAlbumScreenShotThumbnailImageEx1(
        const AlbumFileId& album_file_id, const ScreenShotDecodeOption& screenshot_decoder_options,
        OutLargeData<LoadAlbumScreenShotImageOutput, BufferAttr_HipcMapAlias> out_image_output,
        OutArray<u8, BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_image,
        OutArray<u8, BufferAttr_HipcMapAlias> out_buffer); // 4.0.0+
    Result ForceAlbumUnmounted();
    Result ResetAlbumMountStatus();
    Result RefreshAlbumCache();
    Result GetAlbumCache();
    Result GetAlbumCacheEx();                             // 4.0.0+
    Result GetAlbumEntryFromApplicationAlbumEntryAruid(); // 2.0.0+
    Result Unknown8022();                                 // 19.0.0+
    Result SetInternalErrorConversionEnabled();
    Result LoadMakerNoteInfoForDebug();    // 6.0.0+
    Result Unknown50001();                 // 19.0.0+
    Result GetAlbumAccessResultForDebug(); // 19.0.0+
    Result SetAlbumAccessResultForDebug(); // 19.0.0+
    Result OpenAccessorSession();          // 4.0.0+

    Result TranslateResult(Result in_result);

    std::shared_ptr<AlbumManager> _manager;
};
} // namespace Service::Capture