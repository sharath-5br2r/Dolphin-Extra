// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/GCTASInputWindow.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QSpinBox>
#include <QStyle>
#include <QWidget>

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "DolphinQt/TAS/TASCheckBox.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/InputConfig.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"

#include "ui_GCTASInputWindow.h"

GCTASInputWindow::GCTASInputWindow(QWidget* parent, int controller_id)
    : TASInputWindow(parent), m_controller_id(controller_id)
{
  setWindowTitle(tr("GameCube TAS Input %1").arg(controller_id + 1));

  auto* const content = new QWidget(m_scroll_widget);
  Ui::GCTASInputWindow ui;
  ui.setupUi(content);

  auto* const main_stick_box =
      CreateStickInputs(tr("Main Stick"), GCPad::MAIN_STICK_GROUP, &m_overrider, 1, 1, 255, 255,
                        Qt::Key_F, Qt::Key_G);
  auto* const c_stick_box = CreateStickInputs(tr("C Stick"), GCPad::C_STICK_GROUP, &m_overrider, 1,
                                              1, 255, 255, Qt::Key_H, Qt::Key_J);

  ui.stickLayout->addWidget(new AspectRatioWidget(main_stick_box, 1, 1.02f));
  ui.stickLayout->addWidget(new AspectRatioWidget(c_stick_box, 1, 1.02f));

  auto* l_trigger_layout =
      CreateSliderValuePairLayout(tr("Left"), GCPad::TRIGGERS_GROUP, GCPad::L_ANALOG, &m_overrider,
                                  0, 0, 0, 255, Qt::Key_N, ui.triggersBox);

  auto* r_trigger_layout =
      CreateSliderValuePairLayout(tr("Right"), GCPad::TRIGGERS_GROUP, GCPad::R_ANALOG, &m_overrider,
                                  0, 0, 0, 255, Qt::Key_M, ui.triggersBox);

  ui.triggersLayout->addLayout(l_trigger_layout);
  ui.triggersLayout->addLayout(r_trigger_layout);

  m_a_button =
      CreateButton(QStringLiteral("&A"), GCPad::BUTTONS_GROUP, GCPad::A_BUTTON, &m_overrider);
  m_b_button =
      CreateButton(QStringLiteral("&B"), GCPad::BUTTONS_GROUP, GCPad::B_BUTTON, &m_overrider);
  m_x_button =
      CreateButton(QStringLiteral("&X"), GCPad::BUTTONS_GROUP, GCPad::X_BUTTON, &m_overrider);
  m_y_button =
      CreateButton(QStringLiteral("&Y"), GCPad::BUTTONS_GROUP, GCPad::Y_BUTTON, &m_overrider);
  m_z_button =
      CreateButton(QStringLiteral("&Z"), GCPad::BUTTONS_GROUP, GCPad::Z_BUTTON, &m_overrider);
  m_start_button = CreateButton(QStringLiteral("&START"), GCPad::BUTTONS_GROUP, GCPad::START_BUTTON,
                                &m_overrider);

  m_l_button =
      CreateButton(QStringLiteral("&L"), GCPad::TRIGGERS_GROUP, GCPad::L_DIGITAL, &m_overrider);
  m_r_button =
      CreateButton(QStringLiteral("&R"), GCPad::TRIGGERS_GROUP, GCPad::R_DIGITAL, &m_overrider);

  m_left_button =
      CreateButton(QStringLiteral("L&eft"), GCPad::DPAD_GROUP, DIRECTION_LEFT, &m_overrider);
  m_up_button = CreateButton(QStringLiteral("&Up"), GCPad::DPAD_GROUP, DIRECTION_UP, &m_overrider);
  m_down_button =
      CreateButton(QStringLiteral("&Down"), GCPad::DPAD_GROUP, DIRECTION_DOWN, &m_overrider);
  m_right_button =
      CreateButton(QStringLiteral("R&ight"), GCPad::DPAD_GROUP, DIRECTION_RIGHT, &m_overrider);

  ui.buttonsLayout->addWidget(m_a_button, 0, 0);
  ui.buttonsLayout->addWidget(m_b_button, 0, 1);
  ui.buttonsLayout->addWidget(m_x_button, 0, 2);
  ui.buttonsLayout->addWidget(m_y_button, 0, 3);
  ui.buttonsLayout->addWidget(m_z_button, 0, 4);
  ui.buttonsLayout->addWidget(m_l_button, 0, 5);
  ui.buttonsLayout->addWidget(m_r_button, 0, 6);

  ui.buttonsLayout->addWidget(m_start_button, 1, 0);
  ui.buttonsLayout->addWidget(m_left_button, 1, 1);
  ui.buttonsLayout->addWidget(m_up_button, 1, 2);
  ui.buttonsLayout->addWidget(m_down_button, 1, 3);
  ui.buttonsLayout->addWidget(m_right_button, 1, 4);

  AddContentWidget(content);
  const QSize hint = m_scroll_widget->sizeHint();
  const int scrollbar_buffer = style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 10;
  resize(hint.width() + scrollbar_buffer, hint.height() + scrollbar_buffer);
}

void GCTASInputWindow::hideEvent(QHideEvent* event)
{
  Pad::GetConfig()->GetController(m_controller_id)->ClearInputOverrideFunction();
}

void GCTASInputWindow::showEvent(QShowEvent* event)
{
  Pad::GetConfig()
      ->GetController(m_controller_id)
      ->SetInputOverrideFunction(m_overrider.GetInputOverrideFunction());
}
