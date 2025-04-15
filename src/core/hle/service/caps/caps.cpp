// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/caps/a/caps_a.h"
#include "core/hle/service/caps/caps.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/ss/caps_ss.h"
#include "core/hle/service/caps/su/caps_su.h"
#include "core/hle/service/caps/u/caps_u.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::Capture {

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    auto album_manager = std::make_shared<AlbumManager>(system);

    server_manager->RegisterNamedService(
        "caps:a", std::make_shared<IAlbumAccessorService>(system, album_manager));
    server_manager->RegisterNamedService(
        "caps:ss", std::make_shared<IScreenShotService>(system, album_manager));
    server_manager->RegisterNamedService(
        "caps:su", std::make_shared<IScreenShotApplicationService>(system, album_manager));
    server_manager->RegisterNamedService(
        "caps:u", std::make_shared<IAlbumApplicationService>(system, album_manager));

    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Capture
