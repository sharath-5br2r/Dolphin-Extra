// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/GameListDialog.h"

#include <memory>

#include <QDialogButtonBox>
#include <QListWidget>

#include "UICommon/GameFile.h"

#include "ui_GameListDialog.h"

GameListDialog::GameListDialog(const GameListModel& game_list_model, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model),
      m_ui(std::make_unique<Ui::GameListDialog>())
{
  m_ui->setupUi(this);
  ConnectWidgets();
}

GameListDialog::~GameListDialog() = default;

void GameListDialog::ConnectWidgets()
{
  connect(m_ui->gameList, &QListWidget::itemSelectionChanged,
          [this] { m_ui->buttonBox->setEnabled(m_ui->gameList->currentRow() != -1); });

  connect(m_ui->gameList, &QListWidget::itemDoubleClicked, this, &GameListDialog::accept);
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &GameListDialog::accept);
}

void GameListDialog::PopulateGameList()
{
  m_ui->gameList->clear();

  for (int i = 0; i < m_game_list_model.rowCount(QModelIndex()); i++)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list_model.GetGameFile(i);

    auto* item =
        new QListWidgetItem(QString::fromStdString(m_game_list_model.GetNetPlayName(*game)));
    item->setData(Qt::UserRole, QVariant::fromValue(std::move(game)));
    m_ui->gameList->addItem(item);
  }

  m_ui->gameList->sortItems();
}

const UICommon::GameFile& GameListDialog::GetSelectedGame() const
{
  auto items = m_ui->gameList->selectedItems();
  return *items[0]->data(Qt::UserRole).value<std::shared_ptr<const UICommon::GameFile>>();
}

int GameListDialog::exec()
{
  PopulateGameList();
  return QDialog::exec();
}
