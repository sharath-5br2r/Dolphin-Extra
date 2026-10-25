// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

#include "DolphinQt/GameList/GameListModel.h"

namespace UICommon
{
class GameFile;
}

namespace Ui
{
class NetPlaySetupDialog;
}

class NetPlaySetupDialog : public QDialog
{
  Q_OBJECT
public:
  explicit NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent);
  ~NetPlaySetupDialog() override;

  void accept() override;
  void show();

signals:
  bool Join();
  bool Host(const UICommon::GameFile& game);

private:
  void ConnectWidgets();
  void PopulateGameList();
  void ResetTraversalHost();

  void SaveSettings();

  void OnConnectionTypeChanged(int index);

  const GameListModel& m_game_list_model;
  std::unique_ptr<Ui::NetPlaySetupDialog> m_ui;
};
