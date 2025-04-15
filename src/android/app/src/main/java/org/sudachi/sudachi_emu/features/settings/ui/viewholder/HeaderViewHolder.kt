// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.features.settings.ui.viewholder

import android.view.View
import org.sudachi.sudachi_emu.databinding.ListItemSettingsHeaderBinding
import org.sudachi.sudachi_emu.features.settings.model.view.SettingsItem
import org.sudachi.sudachi_emu.features.settings.ui.SettingsAdapter

class HeaderViewHolder(
    val binding: ListItemSettingsHeaderBinding,
    adapter: SettingsAdapter,
) : SettingViewHolder(binding.root, adapter) {
    init {
        itemView.setOnClickListener(null)
    }

    override fun bind(item: SettingsItem) {
        binding.textHeaderName.text = item.title
    }

    override fun onClick(clicked: View) {
        // no-op
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}
