// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsPane.h"

#include <memory>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/Graphics/AdvancedWidget.h"
#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"
#include "DolphinQt/Config/Graphics/GeneralWidget.h"
#include "DolphinQt/Config/Graphics/HacksWidget.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "VideoCommon/VideoBackendBase.h"

#include "ui_GraphicsPane.h"

GraphicsPane::GraphicsPane(MainWindow* main_window, Config::Layer* config_layer)
    : m_ui{std::make_unique<Ui::GraphicsPane>()}, m_main_window(main_window),
      m_config_layer{config_layer}
{
  m_ui->setupUi(this);
  CreatePages();

  OnBackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
}

GraphicsPane::~GraphicsPane() = default;

Config::Layer* GraphicsPane::GetConfigLayer()
{
  return m_config_layer;
}

void GraphicsPane::CreatePages()
{
  auto* const general_widget = new GeneralWidget(this);
  auto* const enhancements_widget = new EnhancementsWidget(this);
  auto* const hacks_widget = new HacksWidget(this);
  auto* const advanced_widget = new AdvancedWidget(this);

  connect(general_widget, &GeneralWidget::BackendChanged, this, &GraphicsPane::OnBackendChanged);

  QWidget* const wrapped_general = GetWrappedWidget(general_widget);
  QWidget* const wrapped_enhancements = GetWrappedWidget(enhancements_widget);
  QWidget* const wrapped_hacks = GetWrappedWidget(hacks_widget);
  QWidget* const wrapped_advanced = GetWrappedWidget(advanced_widget);

  m_ui->generalLayout->addWidget(wrapped_general);
  m_ui->enhancementsLayout->addWidget(wrapped_enhancements);
  m_ui->hacksLayout->addWidget(wrapped_hacks);
  m_ui->advancedLayout->addWidget(wrapped_advanced);
}

void GraphicsPane::OnBackendChanged(const QString& backend_name)
{
  // TODO: Game properties graphics config does not properly handle backend info.
  if (m_main_window != nullptr)
    VideoBackendBase::PopulateBackendInfo(m_main_window->GetWindowSystemInfo());

  emit BackendChanged(backend_name);
}
