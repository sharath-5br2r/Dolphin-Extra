// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyUSBEmu.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingHorizontalPage.h"

HotkeyUSBEmu::HotkeyUSBEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyUSBEmu::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("USB Device Emulation"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_USB_EMU)));
}

InputConfig* HotkeyUSBEmu::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyUSBEmu::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyUSBEmu::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
