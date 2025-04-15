// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "sudachi_cmd/emu_window/emu_window_sdl3_vk.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

EmuWindow_SDL3_VK::EmuWindow_SDL3_VK(InputCommon::InputSubsystem* input_subsystem_,
                                     Core::System& system_, bool fullscreen)
    : EmuWindow_SDL3{input_subsystem_, system_} {
    const std::string window_title =
        fmt::format("sudachi {} | {}-{} (Vulkan)", Common::g_build_name, Common::g_scm_branch,
                    Common::g_scm_desc);
    render_window = SDL_CreateWindow(window_title.c_str(), Layout::ScreenUndocked::Width,
                                     Layout::ScreenUndocked::Height,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
        ShowCursor(false);
    }

#if defined(SDL_PLATFORM_WIN32)
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(render_window),
                                             SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    window_info.type = Core::Frontend::WindowSystemType::Windows;
    window_info.render_surface = reinterpret_cast<void*>(hwnd);
#elif defined(SDL_PLATFORM_LINUX)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        Display* xdisplay = (Display*)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Window xwindow = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                                                       SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        window_info.type = Core::Frontend::WindowSystemType::X11;
        window_info.display_connection = xdisplay;
        window_info.render_surface = reinterpret_cast<void*>(xwindow);
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        struct wl_display* display = (struct wl_display*)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        struct wl_surface* surface = (struct wl_surface*)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
        window_info.type = Core::Frontend::WindowSystemType::Wayland;
        window_info.display_connection = display;
        window_info.render_surface = reinterpret_cast<void*>(surface);
    }
#elif defined(SDL_PLATFORM_MACOS)
    window_info.type = Core::Frontend::WindowSystemType::MacOS;
    window_info.render_surface = SDL_Metal_GetLayer(SDL_Metal_CreateView(render_window));
#elif defined(SDL_PLATFORM_ANDROID)
    EGLSurface surface = (EGLSurface)SDL_GetPointerProperty(
        SDL_GetWindowProperties(render_window), SDL_PROP_WINDOW_ANDROID_SURFACE_POINTER, NULL);
    window_info.type = Core::Frontend::WindowSystemType::Android;
    window_info.render_surface = reinterpret_cast<void*>(surface);
#else
    LOG_CRITICAL(Frontend, "Window manager subsystem {} not implemented", wm.subsystem);
    std::exit(EXIT_FAILURE);
    break;
#endif

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "sudachi Version: {} | {}-{} (Vulkan)", Common::g_build_name,
             Common::g_scm_branch, Common::g_scm_desc);
}

EmuWindow_SDL3_VK::~EmuWindow_SDL3_VK() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL3_VK::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
