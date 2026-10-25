// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllersPane.h"

#include <memory>

#include "DolphinQt/Config/CommonControllersWidget.h"
#include "DolphinQt/Config/GamecubeControllersWidget.h"
#include "DolphinQt/Config/WiimoteControllersWidget.h"

#include "ui_ControllersPane.h"

ControllersPane::ControllersPane() : m_ui{std::make_unique<Ui::ControllersPane>()}
{
  m_ui->setupUi(this);
  CreateSections();
}

ControllersPane::~ControllersPane() = default;

void ControllersPane::CreateSections()
{
  auto* const gamecube_controllers = new GamecubeControllersWidget(this);
  m_wiimote_controllers = new WiimoteControllersWidget(this);
  auto* const common = new CommonControllersWidget(this);

  m_ui->gamecubeLayout->addWidget(gamecube_controllers);
  m_ui->wiimoteLayout->addWidget(m_wiimote_controllers);
  m_ui->commonLayout->addWidget(common);
}
