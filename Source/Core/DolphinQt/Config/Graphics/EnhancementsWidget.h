// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include <QWidget>

class GraphicsPane;

namespace Config
{
template <typename T>
class Info;
class Layer;
}  // namespace Config

namespace ConfigWidget
{
class ComplexBinding;
}

namespace Ui
{
class EnhancementsWidget;
}

class EnhancementsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit EnhancementsWidget(GraphicsPane* gfx_pane);
  ~EnhancementsWidget() override;

private:
  void MigrateRemovedStereoModes();
  void BindSettings();
  void ConnectWidgets();
  void AddDescriptions();

  void OnBackendChanged();
  void UpdateAntialiasingOptions();
  // The preset the post-processing row is showing, which is the one the picker and the parameters
  // dialog both act on. Empty means no post-processing.
  std::string CurrentShaderPreset() const;
  // Replaces the field's text with the preset that is actually in use when the stored value is a
  // legacy ';'-separated chain, so the row does not advertise passes nothing runs.
  void ShowResolvedPreset();
  void SetShaderPreset(const QString& preset);
  void BrowseForShaderPreset();
  void ClearShaderPreset();
  void EditShaderParameters();
  void UpdateParametersButtonState();
  void ShaderChanged();

  void DownloadShaderPack(const std::string& pack_id, const std::string& profile);

  std::unique_ptr<Ui::EnhancementsWidget> m_ui;
  ConfigWidget::ComplexBinding* m_antialiasing_binding = nullptr;
  ConfigWidget::ComplexBinding* m_texture_filtering_binding = nullptr;
  Config::Layer* m_game_layer = nullptr;
};
