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
class GameListDialog;
}

class GameListDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GameListDialog(const GameListModel& game_list_model, QWidget* parent);
  ~GameListDialog() override;

  int exec() override;
  const UICommon::GameFile& GetSelectedGame() const;

private:
  void ConnectWidgets();
  void PopulateGameList();

  const GameListModel& m_game_list_model;
  std::unique_ptr<Ui::GameListDialog> m_ui;
};
