// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionInput.h"

#include <QFontMetrics>
#include <QPushButton>
#include <QStyle>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"

#include "InputCommon/InputConfig.h"

#include "ui_WiimoteEmuExtensionMotionInput.h"

WiimoteEmuExtensionMotionInput::WiimoteEmuExtensionMotionInput(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
}

void WiimoteEmuExtensionMotionInput::CreateMainLayout()
{
  Ui::WiimoteEmuExtensionMotionInput ui;
  ui.setupUi(this);
  const int icon_size = QFontMetrics(font()).height() * 5 / 4;
  ui.warningIconLabel->setPixmap(
      style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(icon_size, icon_size));
  connect(ui.alternateInputSourcesButton, &QPushButton::clicked, this, [this] {
    ControllerInterfaceWindow window{this};
    window.exec();
  });

  ui.nunchukLayout->addWidget(
      CreateGroupBox(
          tr("Accelerometer"),
          Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::IMUAccelerometer)),
      1, 0);
}

void WiimoteEmuExtensionMotionInput::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuExtensionMotionInput::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuExtensionMotionInput::GetConfig()
{
  return Wiimote::GetConfig();
}
