// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/FreeLookGeneral.h"

#include "Core/FreeLookManager.h"
#include "InputCommon/InputConfig.h"

#include "ui_MappingGridPage.h"

FreeLookGeneral::FreeLookGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void FreeLookGeneral::CreateMainLayout()
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Move"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Move)), 0, 0);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Speed"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Speed)), 0, 1);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Field of View"),
                     FreeLook::GetInputGroup(GetPort(), FreeLookGroup::FieldOfView)),
      0, 2);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Other"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Other)), 0, 3);
}

void FreeLookGeneral::LoadSettings()
{
  FreeLook::LoadInputConfig();
}

void FreeLookGeneral::SaveSettings()
{
  FreeLook::GetInputConfig()->SaveConfig();
}

InputConfig* FreeLookGeneral::GetConfig()
{
  return FreeLook::GetInputConfig();
}
