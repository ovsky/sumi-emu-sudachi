// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sumi.sumi_emu.features.settings.utils

import android.net.Uri
import org.sumi.sumi_emu.model.Game
import org.sumi.sumi_emu.utils.DirectoryInitialization
import org.sumi.sumi_emu.utils.FileUtil
import org.sumi.sumi_emu.utils.NativeConfig
import java.io.*

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config.ini"

    fun getSettingsFile(fileName: String): File = File(DirectoryInitialization.userDirectory + "/config/" + fileName)

    fun getCustomSettingsFile(game: Game): File =
        File(DirectoryInitialization.userDirectory + "/config/custom/" + game.settingsName + ".ini")

    fun loadCustomConfig(game: Game) {
        val fileName = FileUtil.getFilename(Uri.parse(game.path))
        NativeConfig.initializePerGameConfig(game.programId, fileName)
    }
}
