// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.features.input

import android.content.Context
import android.os.Build
import android.os.CombinedVibration
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.InputDevice
import androidx.annotation.Keep
import androidx.annotation.RequiresApi
import org.sudachi.sudachi_emu.SudachiApplication

@Keep
@Suppress("DEPRECATION")
interface SudachiVibrator {
    fun supportsVibration(): Boolean

    fun vibrate(intensity: Float)

    companion object {
        fun getControllerVibrator(device: InputDevice): SudachiVibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                SudachiVibratorManager(device.vibratorManager)
            } else {
                SudachiVibratorManagerCompat(device.vibrator)
            }

        fun getSystemVibrator(): SudachiVibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager =
                    SudachiApplication.appContext
                        .getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
                SudachiVibratorManager(vibratorManager)
            } else {
                val vibrator =
                    SudachiApplication.appContext
                        .getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
                SudachiVibratorManagerCompat(vibrator)
            }

        fun getVibrationEffect(intensity: Float): VibrationEffect? {
            if (intensity > 0f) {
                return VibrationEffect.createOneShot(
                    50,
                    (255.0 * intensity).toInt().coerceIn(1, 255),
                )
            }
            return null
        }
    }
}

@RequiresApi(Build.VERSION_CODES.S)
class SudachiVibratorManager(
    private val vibratorManager: VibratorManager,
) : SudachiVibrator {
    override fun supportsVibration(): Boolean = vibratorManager.vibratorIds.isNotEmpty()

    override fun vibrate(intensity: Float) {
        val vibration = SudachiVibrator.getVibrationEffect(intensity) ?: return
        vibratorManager.vibrate(CombinedVibration.createParallel(vibration))
    }
}

class SudachiVibratorManagerCompat(
    private val vibrator: Vibrator,
) : SudachiVibrator {
    override fun supportsVibration(): Boolean = vibrator.hasVibrator()

    override fun vibrate(intensity: Float) {
        val vibration = SudachiVibrator.getVibrationEffect(intensity) ?: return
        vibrator.vibrate(vibration)
    }
}
