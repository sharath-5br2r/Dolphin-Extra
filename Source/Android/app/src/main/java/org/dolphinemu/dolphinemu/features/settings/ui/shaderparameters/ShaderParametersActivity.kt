// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.shaderparameters

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.utils.ThemeHelper

/**
 * Edits the parameters of the configured post-processing preset; the Android counterpart of the
 * desktop Shader Parameters dialog. Values are stored globally whichever settings page opened it,
 * and a running game applies them on its next frame.
 */
class ShaderParametersActivity : AppCompatActivity(), ThemeProvider {
    override var themeId: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        val presetSpec = intent.getStringExtra(EXTRA_PRESET).orEmpty()
        val openedFromGameSettings = intent.getBooleanExtra(EXTRA_FROM_GAME_SETTINGS, false)

        val viewModel = ViewModelProvider(
            this,
            ShaderParametersViewModel.Factory(presetSpec)
        )[ShaderParametersViewModel::class.java]

        setContent {
            DolphinTheme {
                val state by viewModel.state.collectAsState()
                ShaderParametersScreen(
                    presetName = presetLeafName(presetSpec),
                    openedFromGameSettings = openedFromGameSettings,
                    state = state,
                    onBackClicked = { finish() },
                    onSliderPositionChanged = viewModel::onSliderPositionChanged,
                    onValueEntered = viewModel::onValueEntered,
                    onResetClicked = viewModel::onResetClicked,
                    onResetAllClicked = viewModel::onResetAllClicked,
                )
            }
        }
    }

    override fun onStop() {
        super.onStop()
        // Edits go to the base layer in memory. The settings screen that opened this one saves
        // only its own layer -- the game's when it is per-game settings -- so without this the
        // values would never reach GFX.ini.
        NativeConfig.save(NativeConfig.LAYER_BASE)
    }

    override fun setTheme(themeId: Int) {
        super.setTheme(themeId)
        this.themeId = themeId
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)
        super.onResume()
    }

    companion object {
        private const val EXTRA_PRESET = "preset"
        private const val EXTRA_FROM_GAME_SETTINGS = "from_game_settings"

        /**
         * @param presetSpec the raw GFX_ENHANCE_POST_SHADER value.
         * @param fromGameSettings whether the caller is a game's own settings, which shows a note
         *   that parameter values are global.
         */
        @JvmStatic
        fun launch(context: Context, presetSpec: String, fromGameSettings: Boolean) {
            val intent = Intent(context, ShaderParametersActivity::class.java)
                .putExtra(EXTRA_PRESET, presetSpec)
                .putExtra(EXTRA_FROM_GAME_SETTINGS, fromGameSettings)
            context.startActivity(intent)
        }

        // The leaf of the preset that loads: the first entry of a legacy chain, without its folder
        // -- the same name the desktop dialog's title shows.
        private fun presetLeafName(presetSpec: String): String =
            presetSpec.substringBefore(';').trim().substringAfterLast('/')
    }
}
