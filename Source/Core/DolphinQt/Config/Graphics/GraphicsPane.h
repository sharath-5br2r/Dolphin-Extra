// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

class MainWindow;

namespace Config
{
class Layer;
}  // namespace Config

namespace Ui
{
class GraphicsPane;
}

class GraphicsPane final : public QWidget
{
  Q_OBJECT
public:
  explicit GraphicsPane(MainWindow* main_window, Config::Layer* config_layer);
  ~GraphicsPane() override;

  Config::Layer* GetConfigLayer();

signals:
  void BackendChanged(const QString& backend);
  void UseFastTextureSamplingChanged();
  void UpdateGPUTextureDecoding();

private:
  void CreatePages();
  void OnBackendChanged(const QString& backend);

  std::unique_ptr<Ui::GraphicsPane> m_ui;
  MainWindow* const m_main_window;
  Config::Layer* const m_config_layer;
};
