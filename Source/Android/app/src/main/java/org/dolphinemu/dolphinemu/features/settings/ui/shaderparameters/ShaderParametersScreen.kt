// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.shaderparameters

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.RestartAlt
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.ui.theme.DolphinScaffold
import kotlin.math.roundToInt

@Composable
fun ShaderParametersScreen(
    presetName: String,
    openedFromGameSettings: Boolean,
    state: ShaderParametersUiState,
    onBackClicked: () -> Unit,
    onSliderPositionChanged: (index: Int, position: Int) -> Unit,
    onValueEntered: (index: Int, value: Float) -> Unit,
    onResetClicked: (index: Int) -> Unit,
    onResetAllClicked: () -> Unit,
) {
    DolphinScaffold(
        title = { Text(stringResource(R.string.shader_parameters_title, presetName)) },
        navigationIcon = {
            IconButton(onClick = onBackClicked) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
            }
        },
    ) { padding ->
        when (state) {
            ShaderParametersUiState.Loading -> CenteredBox(padding) { CircularProgressIndicator() }
            ShaderParametersUiState.NoParameters ->
                CenteredMessage(padding, stringResource(R.string.shader_parameters_none))
            is ShaderParametersUiState.LoadFailed -> CenteredMessage(
                padding,
                stringResource(R.string.shader_parameters_load_error, state.reason),
            )
            is ShaderParametersUiState.Loaded -> ParameterList(
                padding = padding,
                rows = state.rows,
                canResetAll = state.canResetAll,
                openedFromGameSettings = openedFromGameSettings,
                onSliderPositionChanged = onSliderPositionChanged,
                onValueEntered = onValueEntered,
                onResetClicked = onResetClicked,
                onResetAllClicked = onResetAllClicked,
            )
        }
    }
}

@Composable
private fun ParameterList(
    padding: PaddingValues,
    rows: List<ShaderParameterRow>,
    canResetAll: Boolean,
    openedFromGameSettings: Boolean,
    onSliderPositionChanged: (Int, Int) -> Unit,
    onValueEntered: (Int, Float) -> Unit,
    onResetClicked: (Int) -> Unit,
    onResetAllClicked: () -> Unit,
) {
    // The index of the row whose value is being typed, if any.
    var editingIndex by rememberSaveable { mutableStateOf<Int?>(null) }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = padding,
    ) {
        // Reset All heads the list rather than floating over it: a floating button sits on the
        // right-hand column, which is exactly where every row keeps its value and Reset controls.
        item {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth().padding(start = 16.dp, end = 4.dp),
            ) {
                // Above the parameters rather than in a tooltip: everything else on the page that
                // led here is per-game, so this is where global values are least expected.
                Text(
                    text = if (openedFromGameSettings) {
                        stringResource(R.string.shader_parameters_global_note)
                    } else {
                        ""
                    },
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = onResetAllClicked, enabled = canResetAll) {
                    Icon(Icons.Filled.RestartAlt, contentDescription = null)
                    Text(
                        text = stringResource(R.string.shader_parameters_reset_all),
                        modifier = Modifier.padding(start = 8.dp),
                    )
                }
            }
            HorizontalDivider()
        }
        items(rows, key = { it.index }) { row ->
            ParameterRow(
                row = row,
                onSliderPositionChanged = { onSliderPositionChanged(row.index, it) },
                onValueClicked = { editingIndex = row.index },
                onResetClicked = { onResetClicked(row.index) },
            )
            HorizontalDivider()
        }
    }

    val editing = editingIndex?.let { index -> rows.firstOrNull { it.index == index } }
    if (editing != null) {
        ValueEntryDialog(
            row = editing,
            onConfirm = { value ->
                onValueEntered(editing.index, value)
                editingIndex = null
            },
            onDismiss = { editingIndex = null },
        )
    }
}

@Composable
private fun ParameterRow(
    row: ShaderParameterRow,
    onSliderPositionChanged: (Int) -> Unit,
    onValueClicked: () -> Unit,
    onResetClicked: () -> Unit,
) {
    Column(modifier = Modifier.fillMaxWidth().padding(start = 16.dp, end = 4.dp, top = 8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(modifier = Modifier.weight(1f)) {
                Text(text = row.title, style = MaterialTheme.typography.bodyLarge)
                if (row.name != row.title) {
                    Text(
                        text = row.name,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            TextButton(onClick = onValueClicked) { Text(row.valueText) }
            IconButton(onClick = onResetClicked, enabled = !row.isDefault) {
                Icon(
                    Icons.Filled.RestartAlt,
                    contentDescription = stringResource(R.string.shader_parameters_reset),
                )
            }
        }
        if (row.sliderPositions > 0) {
            // Integer positions on a continuous slider: the editor owns what each position means
            // (a preset's range need not be a multiple of its step), and tick marks for thousands
            // of positions would be unusable.
            Slider(
                value = row.sliderPosition.toFloat(),
                onValueChange = { onSliderPositionChanged(it.roundToInt()) },
                valueRange = 0f..row.sliderPositions.toFloat(),
                modifier = Modifier.padding(end = 12.dp),
            )
        } else {
            Box(modifier = Modifier.padding(bottom = 8.dp))
        }
    }
}

@Composable
private fun ValueEntryDialog(
    row: ShaderParameterRow,
    onConfirm: (Float) -> Unit,
    onDismiss: () -> Unit,
) {
    var text by rememberSaveable(row.index) { mutableStateOf(row.valueText) }
    var invalid by remember(row.index) { mutableStateOf(false) }
    val hasRange = row.maximum > row.minimum

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(row.title) },
        text = {
            OutlinedTextField(
                value = text,
                onValueChange = {
                    text = it
                    invalid = false
                },
                singleLine = true,
                isError = invalid,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                supportingText = {
                    when {
                        invalid -> Text(stringResource(R.string.shader_parameters_invalid_value))
                        hasRange -> Text(
                            stringResource(
                                R.string.shader_parameters_range,
                                row.minimum.toString(),
                                row.maximum.toString(),
                            )
                        )
                    }
                },
            )
        },
        confirmButton = {
            TextButton(onClick = {
                val value = ShaderParameterInput.parse(text)
                if (value == null) invalid = true else onConfirm(value)
            }) { Text(stringResource(R.string.ok)) }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text(stringResource(R.string.cancel)) }
        },
    )
}

@Composable
private fun CenteredBox(padding: PaddingValues, content: @Composable () -> Unit) {
    Box(
        modifier = Modifier.fillMaxSize().padding(padding),
        contentAlignment = Alignment.Center,
    ) { content() }
}

@Composable
private fun CenteredMessage(padding: PaddingValues, message: String) {
    CenteredBox(padding) {
        Column(verticalArrangement = Arrangement.Center) {
            Text(
                text = message,
                textAlign = TextAlign.Center,
                style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.padding(horizontal = 24.dp),
            )
        }
    }
}
