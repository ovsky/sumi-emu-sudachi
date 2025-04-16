// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sumi.sumi_emu.features.settings.ui.viewholder

import android.view.View
import org.sumi.sumi_emu.R
import org.sumi.sumi_emu.databinding.ListItemSettingBinding
import org.sumi.sumi_emu.features.settings.model.view.SettingsItem
import org.sumi.sumi_emu.features.settings.model.view.SliderSetting
import org.sumi.sumi_emu.features.settings.ui.SettingsAdapter
import org.sumi.sumi_emu.utils.ViewUtils.setVisible

class SliderViewHolder(
    val binding: ListItemSettingBinding,
    adapter: SettingsAdapter,
) : SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SliderSetting

    override fun bind(item: SettingsItem) {
        setting = item as SliderSetting
        binding.textSettingName.text = setting.title
        binding.textSettingDescription.setVisible(item.description.isNotEmpty())
        binding.textSettingDescription.text = setting.description
        binding.textSettingValue.setVisible(true)
        binding.textSettingValue.text =
            String.format(
                binding.textSettingValue.context.getString(R.string.value_with_units),
                if (setting.getSelectedValue() is Int) {
                    setting.getSelectedValue().toString()
                } else {
                    setting.getSelectedValue()
                },
                setting.units,
            )

        binding.buttonClear.setVisible(setting.clearable)
        binding.buttonClear.setOnClickListener {
            adapter.onClearClick(setting, bindingAdapterPosition)
        }

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (setting.isEditable) {
            adapter.onSliderClick(setting, bindingAdapterPosition)
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting, bindingAdapterPosition)
        }
        return false
    }
}
