// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControlIMU.h"

#include <QFontMetrics>
#include <QPushButton>
#include <QString>
#include <QStyle>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"

#include "InputCommon/InputConfig.h"

#include "ui_WiimoteEmuMotionControlIMU.h"

WiimoteEmuMotionControlIMU::WiimoteEmuMotionControlIMU(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
}

void WiimoteEmuMotionControlIMU::CreateMainLayout()
{
  Ui::WiimoteEmuMotionControlIMU ui;
  ui.setupUi(this);
  const int icon_size = QFontMetrics(font()).height() * 5 / 4;
  ui.warningIconLabel->setPixmap(
      style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(icon_size, icon_size));
  connect(ui.alternateInputSourcesButton, &QPushButton::clicked, this, [this] {
    ControllerInterfaceWindow window{this};
    window.exec();
  });

  ui.groupsLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUPoint)));
  ui.groupsLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IRPassthrough)));
  ui.groupsLayout->addWidget(CreateGroupBox(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUAccelerometer)));
  ui.groupsLayout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUGyroscope)));
}

void WiimoteEmuMotionControlIMU::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuMotionControlIMU::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuMotionControlIMU::GetConfig()
{
  return Wiimote::GetConfig();
}
