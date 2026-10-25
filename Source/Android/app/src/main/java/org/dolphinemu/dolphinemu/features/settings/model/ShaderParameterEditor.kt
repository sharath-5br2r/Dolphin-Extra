// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import androidx.annotation.Keep

/**
 * The parameters of a post-processing preset, backed by VideoCommon::ShaderParameterEditor -- the
 * same model the desktop Shader Parameters dialog draws. Constructing one parses the preset, which
 * can take a while for large presets, so do it off the UI thread; everything else is cheap.
 *
 * Every mutator stores the change in the global (base) config layer in memory and returns whether
 * anything changed. A running game picks the change up on its next frame. The values reach GFX.ini
 * only when the base layer is saved.
 *
 * @param presetSpec the raw GFX_ENHANCE_POST_SHADER value; a legacy chain counts as its first entry.
 */
class ShaderParameterEditor(presetSpec: String) {
    @Keep
    private val pointer: Long = initialize(presetSpec)

    external fun finalize()

    /** One of [STATUS_OK], [STATUS_NO_PARAMETERS] or [STATUS_ERROR]. */
    val status: Int
        external get

    /** Why the preset could not be loaded; empty unless [status] is [STATUS_ERROR]. */
    val error: String
        external get

    val parameterCount: Int
        external get

    external fun getName(index: Int): String

    /** The shader's own description of the parameter; may be empty. */
    external fun getDescription(index: Int): String

    external fun getValue(index: Int): Float

    /** The value with the precision its step calls for, always with a '.' decimal separator. */
    external fun getValueText(index: Int): String

    external fun getMinimum(index: Int): Float

    external fun getMaximum(index: Int): Float

    /** The last slider position; 0 when the parameter has no usable range and gets no slider. */
    external fun getSliderPositions(index: Int): Int

    external fun getSliderPosition(index: Int): Int

    external fun isDefault(index: Int): Boolean

    /** Clamps to the declared range when there is one. */
    external fun setValue(index: Int, value: Float): Boolean

    external fun setSliderPosition(index: Int, position: Int): Boolean

    external fun reset(index: Int): Boolean

    external fun resetAll(): Boolean

    companion object {
        // VideoCommon::ShaderParameterEditor::Status, by ordinal.
        const val STATUS_OK = 0
        const val STATUS_NO_PARAMETERS = 1
        const val STATUS_ERROR = 2

        @JvmStatic
        private external fun initialize(presetSpec: String): Long
    }
}
