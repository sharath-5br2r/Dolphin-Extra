// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

object PostProcessing {
    @JvmStatic
    val shaderList: Array<String>
        external get

    /**
     * Downloads the shader pack with the given registry id (e.g. "libretro", "satpixie",
     * "retrocrisis") and installs it into the user Shaders directory, auto-fetching any missing
     * dependency packs first. [profile] selects a display profile for packs that ship several (only
     * "retrocrisis" uses it; pass "" for the others). Blocking; call off the UI thread. Returns the
     * number of installed presets, or -1 on failure.
     */
    @JvmStatic
    external fun downloadShaderPack(packId: String, profile: String): Int

    // VideoCommon::ParameterEditingAvailability, by ordinal.
    const val PARAMETERS_AVAILABLE = 0
    const val PARAMETERS_NO_PRESET = 1
    const val PARAMETERS_LIBRARY_UNAVAILABLE = 2
    const val PARAMETERS_UNSUPPORTED_BACKEND = 3

    /**
     * Whether the shader parameter editor can be offered, as one of the PARAMETERS_* constants: the
     * first unmet requirement among a selected preset ([presetSpec], the raw
     * GFX_ENHANCE_POST_SHADER value), the librashader library, and a backend whose librashader
     * runtime applies stored values ([videoBackend], the MAIN_GFX_BACKEND config name -- Vulkan on
     * Android; OpenGL ES runs the built-in engine, which ignores them).
     */
    @JvmStatic
    external fun getParameterEditingAvailability(videoBackend: String, presetSpec: String): Int
}
