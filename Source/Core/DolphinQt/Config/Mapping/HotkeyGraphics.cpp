// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyGraphics.h"

#include "Core/HotkeyManager.h"

#include "ui_MappingGridPage.h"

HotkeyGraphics::HotkeyGraphics(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGraphics::CreateMainLayout()
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);

  ui.groupLayout->addWidget(CreateGroupBox(tr("Graphics Toggles"),
                                           HotkeyManagerEmu::GetHotkeyGroup(HKGP_GRAPHICS_TOGGLES)),
                            0, 0, -1, 1);

  ui.groupLayout->addWidget(
      CreateGroupBox(tr("FreeLook"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_FREELOOK)), 0, 1);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Internal Resolution"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_IR)), 1, 1);
}

InputConfig* HotkeyGraphics::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyGraphics::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyGraphics::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
