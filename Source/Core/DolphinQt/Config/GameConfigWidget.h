// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include <QString>
#include <QWidget>

namespace UICommon
{
class GameFile;
}

namespace Config
{
class Layer;
}  // namespace Config

namespace Ui
{
class GameConfigWidget;
}

class GameConfigWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GameConfigWidget(const UICommon::GameFile& game);
  ~GameConfigWidget() override;

private:
  void BindSettings();
  void ConnectWidgets();
  void AddDescriptions();
  void PopulateEditorTabs();
  void RefreshLocalEditor();

  QString m_gameini_local_path;
  std::unique_ptr<Ui::GameConfigWidget> m_ui;
  const UICommon::GameFile& m_game;
  std::string m_game_id;
  std::unique_ptr<Config::Layer> m_layer;
  std::unique_ptr<Config::Layer> m_global_layer;
  int m_prev_tab_index = 0;
};
