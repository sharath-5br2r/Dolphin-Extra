// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"
#include "Core/HW/WiimoteEmu/Extension/Drums.h"
#include "Core/HW/WiimoteEmu/Extension/Guitar.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Extension/Shinkansen.h"
#include "Core/HW/WiimoteEmu/Extension/TaTaCon.h"
#include "Core/HW/WiimoteEmu/Extension/Turntable.h"
#include "Core/HW/WiimoteEmu/Extension/UDrawTablet.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/InputConfig.h"

#include "ui_WiimoteEmuExtension.h"

WiimoteEmuExtension::WiimoteEmuExtension(MappingWindow* window)
    : MappingWidget(window), m_ui(std::make_unique<Ui::WiimoteEmuExtension>())
{
  m_ui->setupUi(this);
  CreateClassicLayout();
  CreateDrumsLayout();
  CreateGuitarLayout();
  CreateNunchukLayout();
  CreateTurntableLayout();
  CreateUDrawTabletLayout();
  CreateDrawsomeTabletLayout();
  CreateTaTaConLayout();
  CreateShinkansenLayout();

  ChangeExtensionType(WiimoteEmu::ExtensionNumber::NONE);
}

WiimoteEmuExtension::~WiimoteEmuExtension() = default;

void WiimoteEmuExtension::CreateClassicLayout()
{
  m_ui->classicLayout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Buttons)),
      0, 0);
  m_ui->classicLayout->addWidget(
      CreateGroupBox(tr("D-Pad"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::DPad)),
      1, 0);
  m_ui->classicLayout->addWidget(
      CreateGroupBox(tr("Left Stick"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::LeftStick)),
      0, 1, -1, 1);
  m_ui->classicLayout->addWidget(
      CreateGroupBox(tr("Right Stick"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::RightStick)),
      0, 2, -1, 1);
  m_ui->classicLayout->addWidget(
      CreateGroupBox(tr("Triggers"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Triggers)),
      0, 3, -1, 1);
}

void WiimoteEmuExtension::CreateDrumsLayout()
{
  m_ui->drumsLayout->addWidget(
      CreateGroupBox(tr("Stick"), Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Stick)),
      0, 0, -1, 1);

  m_ui->drumsLayout->addWidget(
      CreateGroupBox(tr("Pads"), Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Pads)),
      0, 1);
  m_ui->drumsLayout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Buttons)),
      1, 1);
}

void WiimoteEmuExtension::CreateNunchukLayout()
{
  m_ui->nunchukLayout->addWidget(
      CreateGroupBox(tr("Stick"),
                     Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Stick)),
      0, 0);
  m_ui->nunchukLayout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Buttons)),
      0, 1);
}

void WiimoteEmuExtension::CreateGuitarLayout()
{
  m_ui->guitarStickLayout->addWidget(CreateGroupBox(
      tr("Stick"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Stick)));

  m_ui->guitarFretsLayout->addWidget(CreateGroupBox(
      tr("Strum"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Strum)));
  m_ui->guitarFretsLayout->addWidget(CreateGroupBox(
      tr("Frets"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Frets)));

  m_ui->guitarButtonsLayout->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Buttons)));
  m_ui->guitarButtonsLayout->addWidget(CreateGroupBox(
      tr("Whammy"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Whammy)));
  m_ui->guitarButtonsLayout->addWidget(CreateGroupBox(
      tr("Slider Bar"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::SliderBar)));
}

void WiimoteEmuExtension::CreateTurntableLayout()
{
  m_ui->turntableLayout->addWidget(
      CreateGroupBox(tr("Stick"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Stick)),
      0, 0, -1, 1);

  m_ui->turntableLayout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Buttons)),
      0, 1);
  m_ui->turntableLayout->addWidget(
      CreateGroupBox(tr("Effect"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::EffectDial)),
      1, 1, -1, 1);

  m_ui->turntableLayout->addWidget(
      // i18n: "Table" refers to a turntable
      CreateGroupBox(tr("Left Table"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::LeftTable)),
      0, 2);
  m_ui->turntableLayout->addWidget(
      CreateGroupBox(
          // i18n: "Table" refers to a turntable
          tr("Right Table"),
          Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::RightTable)),
      1, 2);
  m_ui->turntableLayout->addWidget(
      CreateGroupBox(tr("Crossfade"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Crossfade)),
      2, 2);
}

void WiimoteEmuExtension::CreateUDrawTabletLayout()
{
  m_ui->uDrawTabletLayout->addWidget(CreateGroupBox(
      tr("Buttons"),
      Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Buttons)));

  m_ui->uDrawTabletLayout->addWidget(CreateGroupBox(
      tr("Stylus"), Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Stylus)));

  m_ui->uDrawTabletLayout->addWidget(CreateGroupBox(
      tr("Touch"), Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Touch)));
}

void WiimoteEmuExtension::CreateDrawsomeTabletLayout()
{
  m_ui->drawsomeTabletLayout->addWidget(CreateGroupBox(
      tr("Stylus"),
      Wiimote::GetDrawsomeTabletGroup(GetPort(), WiimoteEmu::DrawsomeTabletGroup::Stylus)));

  m_ui->drawsomeTabletLayout->addWidget(CreateGroupBox(
      tr("Touch"),
      Wiimote::GetDrawsomeTabletGroup(GetPort(), WiimoteEmu::DrawsomeTabletGroup::Touch)));
}

void WiimoteEmuExtension::CreateTaTaConLayout()
{
  m_ui->taTaConLayout->addWidget(CreateGroupBox(
      tr("Center"), Wiimote::GetTaTaConGroup(GetPort(), WiimoteEmu::TaTaConGroup::Center)));
  m_ui->taTaConLayout->addWidget(CreateGroupBox(
      tr("Rim"), Wiimote::GetTaTaConGroup(GetPort(), WiimoteEmu::TaTaConGroup::Rim)));
}

void WiimoteEmuExtension::CreateShinkansenLayout()
{
  m_ui->shinkansenLayout->addWidget(CreateGroupBox(
      tr("Levers"), Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Levers)));
  m_ui->shinkansenLayout->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Buttons)));
  m_ui->shinkansenLayout->addWidget(CreateGroupBox(
      tr("Light"), Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Light)));
}

void WiimoteEmuExtension::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuExtension::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuExtension::GetConfig()
{
  return Wiimote::GetConfig();
}

void WiimoteEmuExtension::ChangeExtensionType(u32 type)
{
  using WiimoteEmu::ExtensionNumber;

  m_ui->noneBox->setHidden(type != ExtensionNumber::NONE);
  m_ui->nunchukBox->setHidden(type != ExtensionNumber::NUNCHUK);
  m_ui->classicBox->setHidden(type != ExtensionNumber::CLASSIC);
  m_ui->guitarBox->setHidden(type != ExtensionNumber::GUITAR);
  m_ui->drumsBox->setHidden(type != ExtensionNumber::DRUMS);
  m_ui->turntableBox->setHidden(type != ExtensionNumber::TURNTABLE);
  m_ui->uDrawTabletBox->setHidden(type != ExtensionNumber::UDRAW_TABLET);
  m_ui->drawsomeTabletBox->setHidden(type != ExtensionNumber::DRAWSOME_TABLET);
  m_ui->taTaConBox->setHidden(type != ExtensionNumber::TATACON);
  m_ui->shinkansenBox->setHidden(type != ExtensionNumber::SHINKANSEN);
}
