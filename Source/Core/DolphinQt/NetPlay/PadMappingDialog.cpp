// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/PadMappingDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSignalBlocker>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/Settings.h"

#include "ui_PadMappingDialog.h"

PadMappingDialog::PadMappingDialog(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::PadMappingDialog>())
{
  m_ui->setupUi(this);
  m_gc_boxes = {m_ui->gcBox1, m_ui->gcBox2, m_ui->gcBox3, m_ui->gcBox4};
  m_gba_boxes = {m_ui->gbaBox1, m_ui->gbaBox2, m_ui->gbaBox3, m_ui->gbaBox4};
  m_wii_boxes = {m_ui->wiiBox1, m_ui->wiiBox2, m_ui->wiiBox3, m_ui->wiiBox4};

  const std::array gc_labels = {m_ui->gcPort1Label, m_ui->gcPort2Label, m_ui->gcPort3Label,
                                m_ui->gcPort4Label};
  const std::array wii_labels = {m_ui->wiiRemote1Label, m_ui->wiiRemote2Label,
                                 m_ui->wiiRemote3Label, m_ui->wiiRemote4Label};
  for (size_t i = 0; i < m_gc_boxes.size(); ++i)
  {
    gc_labels[i]->setText(tr("GC Port %1").arg(i + 1));
    m_gba_boxes[i]->setText(tr("GBA Port %1").arg(i + 1));
    wii_labels[i]->setText(tr("Wii Remote %1").arg(i + 1));
  }

#ifndef HAS_LIBMGBA
  for (QCheckBox* checkbox : m_gba_boxes)
    checkbox->hide();
#endif

  ConnectWidgets();
}

PadMappingDialog::~PadMappingDialog() = default;

void PadMappingDialog::ConnectWidgets()
{
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  for (const auto& combo_group : {m_gc_boxes, m_wii_boxes})
  {
    for (const auto& combo : combo_group)
    {
      connect(combo, &QComboBox::currentIndexChanged, this, &PadMappingDialog::OnMappingChanged);
    }
  }
  for (const auto& checkbox : m_gba_boxes)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(checkbox, &QCheckBox::checkStateChanged, this, &PadMappingDialog::OnMappingChanged);
#else
    connect(checkbox, &QCheckBox::stateChanged, this, &PadMappingDialog::OnMappingChanged);
#endif
  }
}

int PadMappingDialog::exec()
{
  const auto client = Settings::Instance().GetNetPlayClient();
  const auto server = Settings::Instance().GetNetPlayServer();
  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
  m_gba_config = server->GetGBAConfig();
  m_wii_mapping = server->GetWiimoteMapping();

  QStringList players;

  players.append(tr("None"));

  for (const auto& player : m_players)
  {
    players.append(
        QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid));
  }

  for (auto& combo_group : {m_gc_boxes, m_wii_boxes})
  {
    const bool gc = combo_group == m_gc_boxes;
    for (size_t i = 0; i < combo_group.size(); i++)
    {
      auto& combo = combo_group[i];
      const QSignalBlocker blocker(combo);

      combo->clear();
      combo->addItems(players);

      const auto index = gc ? m_pad_mapping[i] : m_wii_mapping[i];

      combo->setCurrentIndex(index);
    }
  }

  for (size_t i = 0; i < m_gba_boxes.size(); i++)
  {
    const QSignalBlocker blocker(m_gba_boxes[i]);

    m_gba_boxes[i]->setChecked(m_gba_config[i].enabled);
  }

  return QDialog::exec();
}

NetPlay::PadMappingArray PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

NetPlay::GBAConfigArray PadMappingDialog::GetGBAArray()
{
  return m_gba_config;
}

NetPlay::PadMappingArray PadMappingDialog::GetWiimoteArray()
{
  return m_wii_mapping;
}

void PadMappingDialog::OnMappingChanged()
{
  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    const int gc_id = m_gc_boxes[i]->currentIndex();
    const int wii_id = m_wii_boxes[i]->currentIndex();

    m_pad_mapping[i] = gc_id > 0 ? m_players[gc_id - 1]->pid : 0;
    m_gba_config[i].enabled = m_gba_boxes[i]->isChecked();
    m_wii_mapping[i] = wii_id > 0 ? m_players[wii_id - 1]->pid : 0;
  }
}
