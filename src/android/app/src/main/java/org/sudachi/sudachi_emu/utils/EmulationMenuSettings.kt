// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.utils

import androidx.preference.PreferenceManager
import org.sudachi.sudachi_emu.SudachiApplication
import org.sudachi.sudachi_emu.features.settings.model.Settings

object EmulationMenuSettings {
    private val preferences =
        PreferenceManager.getDefaultSharedPreferences(SudachiApplication.appContext)

    var joystickRelCenter: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER, true)
        set(value) {
            preferences
                .edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER, value)
                .apply()
        }
    var dpadSlide: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_DPAD_SLIDE, true)
        set(value) {
            preferences
                .edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_DPAD_SLIDE, value)
                .apply()
        }
    var hapticFeedback: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_HAPTICS, false)
        set(value) {
            preferences
                .edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_HAPTICS, value)
                .apply()
        }

    var showFps: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_SHOW_FPS, false)
        set(value) {
            preferences
                .edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_SHOW_FPS, value)
                .apply()
        }
    var showOverlay: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_SHOW_OVERLAY, true)
        set(value) {
            preferences
                .edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_SHOW_OVERLAY, value)
                .apply()
        }
}
