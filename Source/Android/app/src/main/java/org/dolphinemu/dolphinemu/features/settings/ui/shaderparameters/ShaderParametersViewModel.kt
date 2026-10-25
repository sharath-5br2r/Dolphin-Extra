// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.shaderparameters

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.features.settings.model.ShaderParameterEditor

/** One parameter as the screen draws it; a snapshot of the native editor's row. */
data class ShaderParameterRow(
    val index: Int,
    /** The shader's description, or the parameter id when it declares none. */
    val title: String,
    /** The parameter id, shown under the title when it differs from it. */
    val name: String,
    val valueText: String,
    val minimum: Float,
    val maximum: Float,
    /** The last slider position; 0 means the parameter gets no slider. */
    val sliderPositions: Int,
    val sliderPosition: Int,
    val isDefault: Boolean,
)

sealed interface ShaderParametersUiState {
    data object Loading : ShaderParametersUiState
    data class LoadFailed(val reason: String) : ShaderParametersUiState
    data object NoParameters : ShaderParametersUiState
    data class Loaded(val rows: List<ShaderParameterRow>) : ShaderParametersUiState {
        val canResetAll: Boolean get() = rows.any { !it.isDefault }
    }
}

/**
 * Holds the native editor for one preset. Opening it parses the preset, so that happens off the
 * main thread; every edit afterwards is an in-memory config write and runs on the main thread,
 * which is also the only thread that touches the editor once it is open.
 */
class ShaderParametersViewModel(presetSpec: String) : ViewModel() {
    private var editor: ShaderParameterEditor? = null

    private val _state = MutableStateFlow<ShaderParametersUiState>(ShaderParametersUiState.Loading)
    val state: StateFlow<ShaderParametersUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            val opened = withContext(Dispatchers.IO) { ShaderParameterEditor(presetSpec) }
            editor = opened
            _state.value = when (opened.status) {
                ShaderParameterEditor.STATUS_OK ->
                    ShaderParametersUiState.Loaded(List(opened.parameterCount) { opened.readRow(it) })
                ShaderParameterEditor.STATUS_NO_PARAMETERS -> ShaderParametersUiState.NoParameters
                else -> ShaderParametersUiState.LoadFailed(opened.error)
            }
        }
    }

    fun onSliderPositionChanged(index: Int, position: Int) =
        edit(index) { it.setSliderPosition(index, position) }

    fun onValueEntered(index: Int, value: Float) = edit(index) { it.setValue(index, value) }

    fun onResetClicked(index: Int) = edit(index) { it.reset(index) }

    fun onResetAllClicked() {
        val editor = editor ?: return
        val loaded = _state.value as? ShaderParametersUiState.Loaded ?: return
        if (editor.resetAll()) {
            _state.value = loaded.copy(rows = loaded.rows.map { editor.readRow(it.index) })
        }
    }

    private inline fun edit(index: Int, change: (ShaderParameterEditor) -> Boolean) {
        val editor = editor ?: return
        val loaded = _state.value as? ShaderParametersUiState.Loaded ?: return
        if (!change(editor)) return
        _state.value = loaded.copy(
            rows = loaded.rows.map { if (it.index == index) editor.readRow(index) else it }
        )
    }

    class Factory(private val presetSpec: String) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return ShaderParametersViewModel(presetSpec) as T
        }
    }
}

private fun ShaderParameterEditor.readRow(index: Int): ShaderParameterRow {
    val name = getName(index)
    return ShaderParameterRow(
        index = index,
        title = getDescription(index).ifEmpty { name },
        name = name,
        valueText = getValueText(index),
        minimum = getMinimum(index),
        maximum = getMaximum(index),
        sliderPositions = getSliderPositions(index),
        sliderPosition = getSliderPosition(index),
        isDefault = isDefault(index),
    )
}
