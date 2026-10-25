// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionSimulation.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/InputConfig.h"

#include "ui_WiimoteEmuExtensionMotionSimulation.h"

WiimoteEmuExtensionMotionSimulation::WiimoteEmuExtensionMotionSimulation(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
}

void WiimoteEmuExtensionMotionSimulation::CreateMainLayout()
{
  Ui::WiimoteEmuExtensionMotionSimulation ui;
  ui.setupUi(this);

  ui.nunchukLayout->addWidget(
      CreateGroupBox(tr("Shake"),
                     Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Shake)),
      0, 0);
  ui.nunchukLayout->addWidget(
      CreateGroupBox(tr("Tilt"),
                     Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Tilt)),
      0, 1);
  ui.nunchukLayout->addWidget(
      CreateGroupBox(tr("Swing"),
                     Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Swing)),
      0, 2);
}

void WiimoteEmuExtensionMotionSimulation::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuExtensionMotionSimulation::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuExtensionMotionSimulation::GetConfig()
{
  return Wiimote::GetConfig();
}
