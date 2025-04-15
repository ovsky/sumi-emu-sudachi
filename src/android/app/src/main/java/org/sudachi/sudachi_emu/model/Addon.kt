// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.model

data class Addon(
    var enabled: Boolean,
    val title: String,
    val version: String,
)
