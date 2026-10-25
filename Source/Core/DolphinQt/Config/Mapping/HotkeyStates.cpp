// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyStates.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingHorizontalPage.h"

HotkeyStates::HotkeyStates(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyStates::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Save"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_SAVE_STATE)));
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Load"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_STATE)));
}

InputConfig* HotkeyStates::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyStates::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyStates::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
