// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientEditServerDialog.h"

#include <memory>

#include <fmt/format.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include "DolphinQt/Config/ControllerInterface/DualShockUDPSettings.h"
#include "DolphinQt/Config/ControllerInterface/ServerStringValidator.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

#include "ui_DualShockUDPClientEditServerDialog.h"

DualShockUDPClientEditServerDialog::DualShockUDPClientEditServerDialog(
    QWidget* parent, std::optional<size_t> existing_index)
    : QDialog(parent), m_ui(std::make_unique<Ui::DualShockUDPClientEditServerDialog>()),
      m_existing_index(std::move(existing_index))
{
  m_ui->setupUi(this);
  CreateWidgets();
}

DualShockUDPClientEditServerDialog::~DualShockUDPClientEditServerDialog() = default;

void DualShockUDPClientEditServerDialog::CreateWidgets()
{
  setWindowTitle(tr(m_existing_index.has_value() ? "Edit DSU Server" : "Add New DSU Server"));

  m_ui->descriptionLineEdit->setValidator(new ServerStringValidator(m_ui->descriptionLineEdit));

  m_ui->serverAddressLineEdit->setText(
      QString::fromStdString(ciface::DualShockUDPClient::DEFAULT_SERVER_ADDRESS));
  m_ui->serverAddressLineEdit->setValidator(new ServerStringValidator(m_ui->serverAddressLineEdit));

  m_ui->serverPortSpinBox->setValue(ciface::DualShockUDPClient::DEFAULT_SERVER_PORT);

  if (m_existing_index.has_value())
  {
    const auto server = DualShockUDPSettings::GetServers()[*m_existing_index];
    m_ui->descriptionLineEdit->setText(QString::fromStdString(server.description));
    m_ui->serverAddressLineEdit->setText(QString::fromStdString(server.server_address));
    m_ui->serverPortSpinBox->setValue(server.server_port);
  }

  auto* const finish_button = m_ui->buttonBox->button(QDialogButtonBox::Ok);
  finish_button->setText(tr(m_existing_index ? "Apply" : "Add"));
  finish_button->setDefault(true);
  m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this,
          &DualShockUDPClientEditServerDialog::OnServerFinished);
}

void DualShockUDPClientEditServerDialog::OnServerFinished()
{
  const auto server = DualShockUDPServer(m_ui->descriptionLineEdit->text().toStdString(),
                                         m_ui->serverAddressLineEdit->text().toStdString(),
                                         m_ui->serverPortSpinBox->value());
  if (m_existing_index.has_value())
  {
    DualShockUDPSettings::ReplaceServer(*m_existing_index, server);
  }
  else
  {
    DualShockUDPSettings::AddServer(server);
  }
  accept();
}
