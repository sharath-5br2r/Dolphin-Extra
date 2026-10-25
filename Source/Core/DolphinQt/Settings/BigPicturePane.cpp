// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/BigPicturePane.h"

#include <array>
#include <memory>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QPushButton>
#include <QSignalBlocker>

#include "Core/Config/UISettings.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "InputCommon/BigPictureInput.h"

#include "ui_BigPicturePane.h"

BigPicturePane::BigPicturePane(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::BigPicturePane>())
{
  m_ui->setupUi(this);
  ConfigWidget::Bind(m_ui->startBigPictureCheckBox, Config::MAIN_START_BIG_PICTURE);
  const std::array buttons = {m_ui->configureController1Button, m_ui->configureController2Button,
                              m_ui->configureController3Button, m_ui->configureController4Button};
  for (int controller = 0; controller < static_cast<int>(buttons.size()); ++controller)
    connect(buttons[controller], &QPushButton::clicked, this,
            [this, controller] { emit ConfigureInput(controller); });

  LoadSettings();
  connect(m_ui->swapConfirmBackCheckBox, &QCheckBox::toggled, this, &BigPicturePane::SaveSettings);
  connect(m_ui->interfaceScaleSpinBox, &QDoubleSpinBox::valueChanged, this,
          &BigPicturePane::SaveSettings);
  connect(m_ui->safeAreaSpinBox, &QDoubleSpinBox::valueChanged, this,
          &BigPicturePane::SaveSettings);
}

BigPicturePane::~BigPicturePane() = default;

void BigPicturePane::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  window()->installEventFilter(this);
  LoadSettings();
}

bool BigPicturePane::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::WindowActivate && isVisible())
    LoadSettings();
  return QWidget::eventFilter(object, event);
}

void BigPicturePane::LoadSettings()
{
  const auto lock = BigPictureInput::MenuController::GetStateLock();
  const auto options = BigPictureInput::GetController(0)->GetOptions();
  const QSignalBlocker swap_blocker(m_ui->swapConfirmBackCheckBox);
  const QSignalBlocker scale_blocker(m_ui->interfaceScaleSpinBox);
  const QSignalBlocker safe_area_blocker(m_ui->safeAreaSpinBox);
  m_ui->swapConfirmBackCheckBox->setChecked(options.swap_confirm_back);
  m_ui->interfaceScaleSpinBox->setValue(options.scale * 100.0);
  m_ui->safeAreaSpinBox->setValue(options.safe_area * 100.0);
}

void BigPicturePane::SaveSettings()
{
  BigPictureInput::SetOptions({m_ui->swapConfirmBackCheckBox->isChecked(),
                               static_cast<float>(m_ui->interfaceScaleSpinBox->value() / 100.0),
                               static_cast<float>(m_ui->safeAreaSpinBox->value() / 100.0)});
}
