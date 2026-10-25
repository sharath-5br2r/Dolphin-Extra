// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCMicrophone.h"

#include "InputCommon/InputConfig.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "ui_MappingHorizontalPage.h"

GCMicrophone::GCMicrophone(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCMicrophone::CreateMainLayout()
{
  Ui::MappingHorizontalPage ui;
  ui.setupUi(this);
  ui.groupLayout->addWidget(
      CreateGroupBox(tr("Microphone"), Pad::GetGroup(GetPort(), PadGroup::Mic)));
}

void GCMicrophone::LoadSettings()
{
  Pad::LoadConfig();
}

void GCMicrophone::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCMicrophone::GetConfig()
{
  return Pad::GetConfig();
}
