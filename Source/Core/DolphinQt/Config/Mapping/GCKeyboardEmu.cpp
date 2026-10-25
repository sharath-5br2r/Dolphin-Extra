// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCKeyboardEmu.h"

#include <QFontMetrics>
#include <QStyle>

#include "InputCommon/InputConfig.h"

#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCKeyboardEmu.h"

#include "ui_GCKeyboardEmu.h"

GCKeyboardEmu::GCKeyboardEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCKeyboardEmu::CreateMainLayout()
{
  Ui::GCKeyboardEmu ui;
  ui.setupUi(this);
  const int icon_size = QFontMetrics(font()).height() * 5 / 4;
  ui.warningIconLabel->setPixmap(
      style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(icon_size, icon_size));

  using KG = KeyboardGroup;
  for (auto kbg : {KG::Kb0x, KG::Kb1x, KG::Kb2x, KG::Kb3x, KG::Kb4x, KG::Kb5x})
    ui.groupLayout->addWidget(CreateGroupBox(QString{}, Keyboard::GetGroup(GetPort(), kbg)));
}

void GCKeyboardEmu::LoadSettings()
{
  Keyboard::LoadConfig();
}

void GCKeyboardEmu::SaveSettings()
{
  Keyboard::GetConfig()->SaveConfig();
}

InputConfig* GCKeyboardEmu::GetConfig()
{
  return Keyboard::GetConfig();
}
