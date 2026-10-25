// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadEmu.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "InputCommon/InputConfig.h"

#include "ui_MappingGridPage.h"

GCPadEmu::GCPadEmu(MappingWindow* window, SubType sub_type) : MappingWidget(window)
{
  CreateMainLayout(sub_type);
}

void GCPadEmu::CreateMainLayout(SubType sub_type)
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons)), 0, 0);
  ui.groupLayout->addWidget(CreateGroupBox(tr("D-Pad"), Pad::GetGroup(GetPort(), PadGroup::DPad)),
                            1, 0);

  if (sub_type == SubType::AMBaseboard)
  {
    ui.groupLayout->addWidget(
        CreateGroupBox(tr("Triforce"), Pad::GetGroup(GetPort(), PadGroup::Triforce)), 2, 0);
  }

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)), 0, 1, -1,
      1);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("C Stick"), Pad::GetGroup(GetPort(), PadGroup::CStick)), 0, 2, -1, 1);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Triggers"), Pad::GetGroup(GetPort(), PadGroup::Triggers)), 0, 4);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Rumble"), Pad::GetGroup(GetPort(), PadGroup::Rumble)), 1, 4);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Options"), Pad::GetGroup(GetPort(), PadGroup::Options)), 2, 4);
}

void GCPadEmu::LoadSettings()
{
  Pad::LoadConfig();
}

void GCPadEmu::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCPadEmu::GetConfig()
{
  return Pad::GetConfig();
}
