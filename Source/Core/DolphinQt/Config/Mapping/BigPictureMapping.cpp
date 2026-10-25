// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/BigPictureMapping.h"

#include "InputCommon/BigPictureInput.h"
#include "InputCommon/InputConfig.h"
#include "ui_MappingGridPage.h"

BigPictureMapping::BigPictureMapping(MappingWindow* window) : MappingWidget(window)
{
  Ui::MappingGridPage ui;
  ui.setupUi(this);
  auto* controller = BigPictureInput::GetController(GetPort());
  ui.groupLayout->addWidget(CreateGroupBox(tr("Navigation"), controller->GetButtons()), 0, 0);
}

InputConfig* BigPictureMapping::GetConfig()
{
  return BigPictureInput::GetConfig();
}
void BigPictureMapping::LoadSettings()
{
  BigPictureInput::LoadConfig();
}
void BigPictureMapping::SaveSettings()
{
  GetConfig()->SaveConfig();
}
