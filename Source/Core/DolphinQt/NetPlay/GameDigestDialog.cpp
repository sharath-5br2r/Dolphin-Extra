// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/GameDigestDialog.h"

#include <algorithm>
#include <functional>

#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/Settings.h"

#include "ui_GameDigestDialog.h"

static QString GetPlayerNameFromPID(int pid)
{
  QString player_name = QObject::tr("Invalid Player ID");
  const auto client = Settings::Instance().GetNetPlayClient();
  if (!client)
    return player_name;

  for (const auto* player : client->GetPlayers())
  {
    if (player->pid == pid)
    {
      player_name = QString::fromStdString(player->name);
      break;
    }
  }
  return player_name;
}

GameDigestDialog::GameDigestDialog(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::GameDigestDialog>())
{
  m_ui->setupUi(this);
  ConnectWidgets();
  setWindowFlags(Qt::Sheet | Qt::Dialog);
  setWindowModality(Qt::WindowModal);
}

GameDigestDialog::~GameDigestDialog() = default;

void GameDigestDialog::ConnectWidgets()
{
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &GameDigestDialog::reject);
}

void GameDigestDialog::show(const QString& title)
{
  m_ui->progressBox->setTitle(title);

  for (const auto& pair : m_progress_bars)
  {
    m_ui->progressLayout->removeWidget(pair.second);
    pair.second->deleteLater();
  }

  for (const auto& pair : m_status_labels)
  {
    m_ui->progressLayout->removeWidget(pair.second);
    pair.second->deleteLater();
  }

  m_progress_bars.clear();
  m_status_labels.clear();
  m_results.clear();
  m_ui->checkLabel->clear();

  const auto client = Settings::Instance().GetNetPlayClient();
  if (!client)
    return;

  if (Settings::Instance().GetNetPlayServer())
  {
    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
    QPushButton* cancel_button = m_ui->buttonBox->button(QDialogButtonBox::Cancel);
    cancel_button->setAutoDefault(false);
    cancel_button->setDefault(false);
  }
  else
  {
    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Close);
    QPushButton* close_button = m_ui->buttonBox->button(QDialogButtonBox::Close);
    close_button->setAutoDefault(false);
    close_button->setDefault(false);
  }

  for (const auto* player : client->GetPlayers())
  {
    m_progress_bars[player->pid] = new QProgressBar;
    m_status_labels[player->pid] = new QLabel;

    m_ui->progressLayout->addWidget(m_progress_bars[player->pid]);
    m_ui->progressLayout->addWidget(m_status_labels[player->pid]);
  }

  QDialog::show();
}

void GameDigestDialog::SetProgress(int pid, int progress)
{
  QString player_name = GetPlayerNameFromPID(pid);

  if (!m_status_labels.contains(pid))
    return;

  m_status_labels[pid]->setText(
      tr("%1[%2]: %3 %").arg(player_name, QString::number(pid), QString::number(progress)));
  m_progress_bars[pid]->setValue(progress);
}

void GameDigestDialog::SetResult(int pid, const std::string& result)
{
  QString player_name = GetPlayerNameFromPID(pid);

  if (!m_status_labels.contains(pid))
    return;

  m_status_labels[pid]->setText(
      tr("%1[%2]: %3").arg(player_name, QString::number(pid), QString::fromStdString(result)));

  m_results.push_back(result);

  const auto client = Settings::Instance().GetNetPlayClient();
  if (client && m_results.size() >= client->GetPlayers().size())
  {
    if (std::ranges::adjacent_find(m_results, std::ranges::not_equal_to{}) == m_results.end())
    {
      m_ui->checkLabel->setText(tr("The hashes match!"));
    }
    else
    {
      m_ui->checkLabel->setText(tr("The hashes do not match!"));
    }

    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Close);
    QPushButton* close_button = m_ui->buttonBox->button(QDialogButtonBox::Close);
    close_button->setAutoDefault(false);
    close_button->setDefault(false);
  }
}

void GameDigestDialog::reject()
{
  const auto server = Settings::Instance().GetNetPlayServer();

  if (server)
    server->AbortGameDigest();

  QDialog::reject();
}
