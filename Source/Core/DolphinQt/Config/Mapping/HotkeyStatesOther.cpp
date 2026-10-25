// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyStatesOther.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingHorizontalPage.h"

HotkeyStatesOther::HotkeyStatesOther(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyStatesOther::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Select Last State"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_SELECT_STATE)));
  ui.groupLayout->addWidget(CreateGroupBox(tr("Load Last State"),
                                           HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_LAST_STATE)));
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Other State Hotkeys"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_STATE_MISC)));
}

InputConfig* HotkeyStatesOther::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyStatesOther::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyStatesOther::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
