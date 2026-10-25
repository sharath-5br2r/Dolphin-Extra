// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

class GraphicsPane;
class QString;

namespace Config
{
class Layer;
}  // namespace Config

namespace Ui
{
class HacksWidget;
}

class HacksWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit HacksWidget(GraphicsPane* gfx_pane);
  ~HacksWidget() override;

private:
  void BindSettings();
  void ConnectWidgets();
  void AddDescriptions();

  void UpdateGPUTextureDecodingEnabled(const QString& backend_name);
  void UpdateBoundingBoxEnabled(const QString& backend_name);
  void UpdateDeferEFBCopiesEnabled();
  void UpdateSkipPresentingDuplicateFramesEnabled();

  void OnBackendChanged(const QString& backend_name);

  std::unique_ptr<Ui::HacksWidget> m_ui;
  Config::Layer* m_game_layer = nullptr;
};
