// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"

#include <memory>

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"
#endif

#include "ui_ControllerInterfaceWindow.h"

ControllerInterfaceWindow::ControllerInterfaceWindow(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::ControllerInterfaceWindow>())
{
  m_ui->setupUi(this);
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
  m_dsuclient_widget = new DualShockUDPClientWidget();
  m_ui->tabWidget->addTab(m_dsuclient_widget, tr("DSU Client"));  // TODO: use GetWrappedWidget()?
#endif

  const bool has_configuration = m_ui->tabWidget->count() > 0;
  m_ui->tabWidget->setVisible(has_configuration);
  m_ui->nothingToConfigureLabel->setVisible(!has_configuration);
}

ControllerInterfaceWindow::~ControllerInterfaceWindow() = default;
