// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControl.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/InputConfig.h"

#include "ui_MappingHorizontalPage.h"

WiimoteEmuMotionControl::WiimoteEmuMotionControl(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void WiimoteEmuMotionControl::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Shake)));
  ui.groupLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Point)));
  ui.groupLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Tilt)));
  ui.groupLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Swing)));
}

void WiimoteEmuMotionControl::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuMotionControl::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuMotionControl::GetConfig()
{
  return Wiimote::GetConfig();
}
