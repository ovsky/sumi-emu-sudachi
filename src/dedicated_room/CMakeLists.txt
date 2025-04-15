# SPDX-FileCopyrightText: 2017 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(sudachi-room
    precompiled_headers.h
    sudachi_room.cpp
    sudachi_room.rc
)

target_link_libraries(sudachi-room PRIVATE common network)
if (ENABLE_WEB_SERVICE)
    target_compile_definitions(sudachi-room PRIVATE -DENABLE_WEB_SERVICE)
    target_link_libraries(sudachi-room PRIVATE web_service)
endif()

target_link_libraries(sudachi-room PRIVATE mbedtls mbedcrypto)
if (MSVC)
    target_link_libraries(sudachi-room PRIVATE getopt)
endif()
target_link_libraries(sudachi-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS sudachi-room)
endif()

if (SUDACHI_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(sudachi-room PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(sudachi-room)
