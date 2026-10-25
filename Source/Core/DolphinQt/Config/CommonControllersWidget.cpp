// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/CommonControllersWidget.h"

#include <memory>

#include <QCheckBox>
#include <QPushButton>

#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/Config/SDLHints/SDLHintsWindow.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "ui_CommonControllersWidget.h"

CommonControllersWidget::CommonControllersWidget(QWidget* parent)
    : QWidget(parent), m_ui{std::make_unique<Ui::CommonControllersWidget>()}
{
  m_ui->setupUi(this);
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &CommonControllersWidget::LoadSettings);
}

CommonControllersWidget::~CommonControllersWidget() = default;

void CommonControllersWidget::ConnectWidgets()
{
  connect(m_ui->backgroundInputCheckBox, &QCheckBox::toggled, this,
          &CommonControllersWidget::SaveSettings);
  connect(m_ui->alternateInputSourcesButton, &QPushButton::clicked, this,
          &CommonControllersWidget::OnControllerInterfaceConfigure);
  connect(m_ui->sdlControllerSettingsButton, &QPushButton::clicked, this,
          &CommonControllersWidget::OnSDLHintConfigure);
}

void CommonControllersWidget::OnControllerInterfaceConfigure()
{
  ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void CommonControllersWidget::OnSDLHintConfigure()
{
  SDLHintsWindow* window = new SDLHintsWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void CommonControllersWidget::LoadSettings()
{
  SignalBlocking(m_ui->backgroundInputCheckBox)
      ->setChecked(Config::Get(Config::MAIN_INPUT_BACKGROUND_INPUT));
}

void CommonControllersWidget::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::MAIN_INPUT_BACKGROUND_INPUT,
                           m_ui->backgroundInputCheckBox->isChecked());
  Config::Save();
}
