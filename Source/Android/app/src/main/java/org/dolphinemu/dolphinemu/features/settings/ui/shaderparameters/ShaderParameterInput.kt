// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.shaderparameters

object ShaderParameterInput {
    /**
     * Parses a typed parameter value. Accepts '.' or ',' as the decimal separator (but not both,
     * and only once), since the decimal keypad of a ','-decimal locale types a comma. Returns null
     * for anything that is not a finite number, including the "NaN"/"Infinity" spellings and
     * values that overflow a float.
     */
    fun parse(text: String): Float? {
        val trimmed = text.trim()
        if (trimmed.count { it == '.' || it == ',' } > 1) return null
        val value = trimmed.replace(',', '.').toFloatOrNull() ?: return null
        return if (value.isFinite()) value else null
    }
}
