// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GBAPadEmu.h"

#include "Core/HW/GBAPad.h"
#include "Core/HW/GBAPadEmu.h"
#include "InputCommon/InputConfig.h"

#include "ui_MappingGridPage.h"

GBAPadEmu::GBAPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GBAPadEmu::CreateMainLayout()
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateControlsBox(tr("D-Pad"), Pad::GetGBAGroup(GetPort(), GBAPadGroup::DPad), 2), 0, 0, -1,
      1);
  ui.groupLayout->addWidget(
      CreateControlsBox(tr("Buttons"), Pad::GetGBAGroup(GetPort(), GBAPadGroup::Buttons), 2), 0, 1,
      -1, 1);
}

void GBAPadEmu::LoadSettings()
{
  Pad::LoadGBAConfig();
}

void GBAPadEmu::SaveSettings()
{
  Pad::GetGBAConfig()->SaveConfig();
}

InputConfig* GBAPadEmu::GetConfig()
{
  return Pad::GetGBAConfig();
}
