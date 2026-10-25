// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyGBA.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingHorizontalPage.h"

HotkeyGBA::HotkeyGBA(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGBA::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Core"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GBA_CORE)));
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Volume"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GBA_VOLUME)));
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Window Size"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GBA_SIZE)));
}

InputConfig* HotkeyGBA::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyGBA::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyGBA::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
