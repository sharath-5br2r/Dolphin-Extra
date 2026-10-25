// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyGeneral.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingGridPage.h"

HotkeyGeneral::HotkeyGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGeneral::CreateMainLayout()
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("General"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GENERAL)), 0, 0, -1, 1);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Volume"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_VOLUME)), 0, 1);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Emulation Speed"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_SPEED)), 1, 1);
}

InputConfig* HotkeyGeneral::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyGeneral::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyGeneral::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
