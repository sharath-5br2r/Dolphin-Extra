// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyDebugging.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingGridPage.h"

HotkeyDebugging::HotkeyDebugging(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyDebugging::CreateMainLayout()
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Stepping"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_STEPPING)), 0, 0, -1, 1);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Program Counter"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_PC)), 0, 1);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Breakpoint"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_BREAKPOINT)), 1, 1);
}

InputConfig* HotkeyDebugging::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyDebugging::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyDebugging::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
