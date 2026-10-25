// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"

#include <memory>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTimer>

#include "Core/Config/MainSettings.h"

#include "InputCommon/GCAdapter.h"

#include "ui_GCPadWiiUConfigDialog.h"

GCPadWiiUConfigDialog::GCPadWiiUConfigDialog(int port, QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::GCPadWiiUConfigDialog>()), m_port{port}
{
  CreateLayout();

  LoadSettings();
  ConnectWidgets();
}

GCPadWiiUConfigDialog::~GCPadWiiUConfigDialog() = default;

void GCPadWiiUConfigDialog::CreateLayout()
{
  m_ui->setupUi(this);
  setWindowTitle(tr("GameCube Controller Adapter at Port %1").arg(m_port + 1));

  UpdateAdapterStatus();

  auto* const timer = new QTimer{this};
  connect(timer, &QTimer::timeout, this, &GCPadWiiUConfigDialog::UpdateAdapterStatus);
  timer->start(std::chrono::milliseconds{500});
}

void GCPadWiiUConfigDialog::ConnectWidgets()
{
  connect(m_ui->rumbleCheckBox, &QCheckBox::toggled, this, &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_ui->simulateBongosCheckBox, &QCheckBox::toggled, this,
          &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &GCPadWiiUConfigDialog::accept);
}

void GCPadWiiUConfigDialog::UpdateAdapterStatus()
{
  const char* error_message = nullptr;
  const bool detected = GCAdapter::IsDetected(&error_message);
  QString status_text;

  if (detected)
  {
    status_text = tr("Adapter Detected");
  }
  else if (error_message)
  {
    status_text = tr("Error Opening Adapter: %1").arg(QString::fromUtf8(error_message));
  }
  else
  {
    status_text = tr("No Adapter Detected");
  }

  m_ui->statusLabel->setText(status_text);

  const auto poll_rate = GCAdapter::GetCurrentPollRate();
  if (poll_rate != 0)
    m_ui->pollRateLabel->setText(tr("Poll Rate: %1 Hz").arg(poll_rate, 0, 'f', 2));
  else
    m_ui->pollRateLabel->clear();

  m_ui->rumbleCheckBox->setEnabled(detected);
  m_ui->simulateBongosCheckBox->setEnabled(detected);
}

void GCPadWiiUConfigDialog::LoadSettings()
{
  m_ui->rumbleCheckBox->setChecked(Config::Get(Config::GetInfoForAdapterRumble(m_port)));
  m_ui->simulateBongosCheckBox->setChecked(Config::Get(Config::GetInfoForSimulateKonga(m_port)));
}

void GCPadWiiUConfigDialog::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::GetInfoForAdapterRumble(m_port),
                           m_ui->rumbleCheckBox->isChecked());
  Config::SetBaseOrCurrent(Config::GetInfoForSimulateKonga(m_port),
                           m_ui->simulateBongosCheckBox->isChecked());
}
