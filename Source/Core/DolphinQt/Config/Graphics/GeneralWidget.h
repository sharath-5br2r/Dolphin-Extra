// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

class GraphicsPane;

namespace Config
{
class Layer;
}  // namespace Config

namespace Ui
{
class GeneralWidget;
}

class GeneralWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralWidget(GraphicsPane* gfx_pane);
  ~GeneralWidget() override;

signals:
  void BackendChanged(const QString& backend);

private:
  void BackendWarning();

  void BindSettings();
  void ToggleCustomAspectRatio(int index);
  void ConnectWidgets();
  void AddDescriptions();

  void OnBackendChanged(const QString& backend_name);
  void OnEmulationStateChanged(bool running);

  std::unique_ptr<Ui::GeneralWidget> m_ui;
  int m_previous_backend = 0;
  Config::Layer* m_game_layer = nullptr;
};
