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
class AdvancedWidget;
}

class AdvancedWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AdvancedWidget(GraphicsPane* gfx_pane);
  ~AdvancedWidget() override;

private:
  void BindSettings();
  void ConnectWidgets();
  void AddDescriptions();
  void OnBackendChanged();
  void OnEmulationStateChanged(bool running);

  std::unique_ptr<Ui::AdvancedWidget> m_ui;
  Config::Layer* m_game_layer = nullptr;
};
