// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyTAS.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingHorizontalPage.h"

HotkeyTAS::HotkeyTAS(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyTAS::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Frame Advance"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_FRAME_ADVANCE)));
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Movie"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_MOVIE)));
}

InputConfig* HotkeyTAS::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyTAS::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyTAS::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
