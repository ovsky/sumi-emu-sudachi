// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.features.settings.ui.viewholder

import android.view.View
import androidx.core.content.res.ResourcesCompat
import org.sudachi.sudachi_emu.databinding.ListItemSettingBinding
import org.sudachi.sudachi_emu.features.settings.model.view.RunnableSetting
import org.sudachi.sudachi_emu.features.settings.model.view.SettingsItem
import org.sudachi.sudachi_emu.features.settings.ui.SettingsAdapter
import org.sudachi.sudachi_emu.utils.ViewUtils.setVisible

class RunnableViewHolder(
    val binding: ListItemSettingBinding,
    adapter: SettingsAdapter,
) : SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: RunnableSetting

    override fun bind(item: SettingsItem) {
        setting = item as RunnableSetting
        binding.icon.setVisible(setting.iconId != 0)
        if (setting.iconId != 0) {
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.resources,
                    setting.iconId,
                    binding.icon.context.theme,
                ),
            )
        }

        binding.textSettingName.text = setting.title
        binding.textSettingDescription.setVisible(setting.description.isNotEmpty())
        binding.textSettingDescription.text = item.description
        binding.textSettingValue.setVisible(false)
        binding.buttonClear.setVisible(false)

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (setting.isRunnable) {
            setting.runnable.invoke()
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}
