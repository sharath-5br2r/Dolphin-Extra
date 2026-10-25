// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"

#include <memory>

#include <fmt/format.h>

#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientEditServerDialog.h"
#include "DolphinQt/Config/ControllerInterface/DualShockUDPSettings.h"

#include "ui_DualShockUDPClientWidget.h"

DualShockUDPClientWidget::DualShockUDPClientWidget()
    : m_ui(std::make_unique<Ui::DualShockUDPClientWidget>())
{
  m_ui->setupUi(this);
  m_ui->serversEnabledCheckBox->setChecked(DualShockUDPSettings::IsEnabled());
  ConnectWidgets();
  RefreshServerList();
}

DualShockUDPClientWidget::~DualShockUDPClientWidget() = default;

void DualShockUDPClientWidget::ConnectWidgets()
{
  connect(m_ui->addServerButton, &QPushButton::clicked, this,
          &DualShockUDPClientWidget::OnServerAdded);
  connect(m_ui->editServerButton, &QPushButton::clicked, this,
          &DualShockUDPClientWidget::OnServerEdited);
  connect(m_ui->removeServerButton, &QPushButton::clicked, this,
          &DualShockUDPClientWidget::OnServerRemoved);
  connect(m_ui->serverListWidget, &QListWidget::currentRowChanged, this,
          &DualShockUDPClientWidget::OnServerSelection);
  connect(m_ui->serversEnabledCheckBox, &QCheckBox::clicked, this,
          &DualShockUDPClientWidget::OnServersToggled);
}

void DualShockUDPClientWidget::SetButtonEnableStates()
{
  const bool has_selection = m_ui->serverListWidget->currentRow() != -1;
  m_ui->editServerButton->setEnabled(has_selection);
  m_ui->removeServerButton->setEnabled(has_selection);
}

void DualShockUDPClientWidget::RefreshServerList()
{
  m_ui->serverListWidget->clear();

  for (const auto& server : DualShockUDPSettings::GetServers())
  {
    QListWidgetItem* list_item = new QListWidgetItem(QString::fromStdString(
        fmt::format("{}:{} - {}", server.description, server.server_address, server.server_port)));
    m_ui->serverListWidget->addItem(list_item);
  }

  SetButtonEnableStates();

  emit ConfigChanged();
}

void DualShockUDPClientWidget::OnServerAdded()
{
  DualShockUDPClientEditServerDialog add_server_dialog(this, std::nullopt);
  connect(&add_server_dialog, &DualShockUDPClientEditServerDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
  add_server_dialog.exec();
}

void DualShockUDPClientWidget::OnServerEdited()
{
  DualShockUDPClientEditServerDialog edit_server_dialog(this, m_ui->serverListWidget->currentRow());
  connect(&edit_server_dialog, &DualShockUDPClientEditServerDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
  edit_server_dialog.exec();
}

void DualShockUDPClientWidget::OnServerRemoved()
{
  const int row_to_remove = m_ui->serverListWidget->currentRow();

  DualShockUDPSettings::RemoveServer(row_to_remove);

  RefreshServerList();
}

void DualShockUDPClientWidget::OnServerSelection()
{
  SetButtonEnableStates();
}

void DualShockUDPClientWidget::OnServersToggled()
{
  DualShockUDPSettings::SetEnabled(m_ui->serversEnabledCheckBox->isChecked());
}
