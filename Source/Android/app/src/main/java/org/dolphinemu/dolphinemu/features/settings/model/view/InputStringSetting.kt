// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

/**
 * A text setting edited in a dialog. With [secretSummary] it holds a secret: the row shows that
 * summary ("Saved", say) instead of the value, and the dialog masks the input, with a button to
 * show it.
 */
class InputStringSetting(
    context: Context,
    setting: AbstractStringSetting,
    titleId: Int,
    descriptionId: Int,
    val secretSummary: (() -> CharSequence)? = null,
) : SettingsItem(context, titleId, descriptionId) {
    val isSecret: Boolean
        get() = secretSummary != null

    override val type: Int = TYPE_STRING

    private var stringSetting: AbstractStringSetting = setting

    override val setting: AbstractSetting
        get() = stringSetting

    val selectedValue: String
        get() = stringSetting.string

    fun setSelectedValue(settings: Settings, selection: String) {
        stringSetting.setString(settings, selection)
    }
}
