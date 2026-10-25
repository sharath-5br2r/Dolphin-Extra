// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/FreeLookRotation.h"

#include <QPushButton>

#include "Core/FreeLookManager.h"
#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "InputCommon/InputConfig.h"

#include "ui_FreeLookRotation.h"

FreeLookRotation::FreeLookRotation(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void FreeLookRotation::CreateMainLayout()
{
  Ui::FreeLookRotation ui;
  ui.setupUi(this);
  connect(ui.alternateInputSourcesButton, &QPushButton::clicked, this, [this] {
    ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->setWindowModality(Qt::WindowModality::WindowModal);
    window->show();
  });

  ui.mainLayout->addWidget(
      CreateGroupBox(tr("Incremental Rotation (rad/sec)"),
                     FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Rotation)),
      1, 0);
}

InputConfig* FreeLookRotation::GetConfig()
{
  return FreeLook::GetInputConfig();
}

void FreeLookRotation::LoadSettings()
{
  FreeLook::LoadInputConfig();
}

void FreeLookRotation::SaveSettings()
{
  FreeLook::GetInputConfig()->SaveConfig();
}
