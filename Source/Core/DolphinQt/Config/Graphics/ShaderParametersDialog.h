// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <QDialog>

#include "VideoCommon/PostProcessing/ShaderParameterEditor.h"

class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QSlider;

// Editor for the selected preset's #pragma parameters, ported from PCSX2's ShaderParametersDialog.
// One row per parameter: its description, a slider, a spin box and a Reset. The values, the slider
// mapping and persistence are VideoCommon::ShaderParameterEditor's, shared with the Android editor;
// this class only draws the rows. Every edit is stored in the global config layer and bumps the
// generation counter a running chain polls, so a preset can be tuned while a game is on screen
// without reloading anything.
class ShaderParametersDialog final : public QDialog
{
  Q_OBJECT
public:
  // `preset` is the relative preset id the picker stores in GFX_ENHANCE_POST_SHADER ('/'
  // separators, no ".slangp"), already resolved through VideoCommon::ResolveConfiguredPreset.
  //
  // `opened_from_game_properties` is true when the caller's other controls write a per-game layer.
  // The values edited here are global whatever the caller is (see ShaderParameterEditor::Save), and a page where
  // everything else is per-game is exactly where that is least expected, so the dialog says so up
  // front instead of leaving the user to discover it.
  ShaderParametersDialog(QWidget* parent, const QString& preset, bool opened_from_game_properties);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  struct RowWidgets
  {
    QSlider* slider = nullptr;  // Null when the parameter has no slider positions.
    QDoubleSpinBox* spin = nullptr;
    QPushButton* reset = nullptr;
  };

  void CreateWidgets(bool opened_from_game_properties);
  bool ShowLoadStatus();
  void BuildRows();
  void RefreshRow(size_t index);
  void OnResetAllClicked();

  std::unique_ptr<VideoCommon::ShaderParameterEditor> m_editor;

  QLabel* m_status;
  QScrollArea* m_scroll;
  QGridLayout* m_grid;
  QPushButton* m_reset_all;
  std::vector<RowWidgets> m_rows;

  // Set while widgets are being refreshed programmatically. The slider and the spin box each drive
  // the other, so without it an edit to either oscillates between them.
  bool m_updating = false;
};
