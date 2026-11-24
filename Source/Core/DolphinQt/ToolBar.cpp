// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ToolBar.h"

#include <algorithm>
#include <vector>

#include <QAction>
#include <QIcon>
#include <QToolBar>

#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

#include "ui_MainWindow.h"

ToolBar::ToolBar(Ui::MainWindow& ui, QObject* parent)
    : QObject(parent), m_toolbar(ui.toolbar), m_open_action(ui.actionToolbarOpen),
      m_refresh_action(ui.actionToolbarRefresh), m_pause_play_action(ui.actionToolbarPlayPause),
      m_stop_action(ui.actionToolbarStop), m_fullscreen_action(ui.actionToolbarFullScreen),
      m_screenshot_action(ui.actionToolbarScreenShot), m_config_action(ui.actionToolbarSettings),
      m_controllers_action(ui.actionToolbarControllers),
      m_graphics_action(ui.actionToolbarGraphics), m_step_action(ui.actionToolbarStep),
      m_step_over_action(ui.actionToolbarStepOver), m_step_out_action(ui.actionToolbarStepOut),
      m_skip_action(ui.actionToolbarSkip), m_show_pc_action(ui.actionToolbarShowPC),
      m_set_pc_action(ui.actionToolbarSetPC)
{
  m_netplay_setup_action = m_toolbar->addAction(tr("Netplay"));
  m_cloud_saves_action = m_toolbar->addAction(tr("Cloud Saves"));
  m_updater_action = m_toolbar->addAction(tr("Update"));

  m_toolbar->setMovable(!Settings::Instance().AreWidgetsLocked());
  m_toolbar->setVisible(Settings::Instance().IsToolBarVisible());

  ConnectActions();
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &ToolBar::UpdateIcons);
  UpdateIcons();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this](Core::State state) { OnEmulationStateChanged(state); });

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          [this] { OnEmulationStateChanged(Core::GetState(Core::System::GetInstance())); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, &ToolBar::OnDebugModeToggled);

  connect(&Settings::Instance(), &Settings::ToolBarVisibilityChanged, m_toolbar,
          &QToolBar::setVisible);
  connect(m_toolbar, &QToolBar::visibilityChanged, &Settings::Instance(),
          &Settings::SetToolBarVisible);

  connect(&Settings::Instance(), &Settings::WidgetLockChanged, this,
          [this](bool locked) { m_toolbar->setMovable(!locked); });

  connect(&Settings::Instance(), &Settings::GameListRefreshRequested, this,
          [this] { m_refresh_action->setEnabled(false); });
  connect(&Settings::Instance(), &Settings::GameListRefreshStarted, this,
          [this] { m_refresh_action->setEnabled(true); });

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
  OnDebugModeToggled(Config::IsDebuggingEnabled());
}

void ToolBar::OnEmulationStateChanged(Core::State state)
{
  bool running = state != Core::State::Uninitialized;
  m_stop_action->setEnabled(running);
  m_fullscreen_action->setEnabled(running);
  m_screenshot_action->setEnabled(running);

  bool playing = running && state != Core::State::Paused;
  UpdatePausePlayButtonState(playing);

  const bool paused = Core::GetState(Core::System::GetInstance()) == Core::State::Paused;
  m_step_action->setEnabled(paused);
  m_step_over_action->setEnabled(paused);
  m_step_out_action->setEnabled(paused);
  m_skip_action->setEnabled(paused);
  m_set_pc_action->setEnabled(paused);
}

void ToolBar::OnDebugModeToggled(bool enabled)
{
  m_step_action->setVisible(enabled);
  m_step_over_action->setVisible(enabled);
  m_step_out_action->setVisible(enabled);
  m_skip_action->setVisible(enabled);
  m_show_pc_action->setVisible(enabled);
  m_set_pc_action->setVisible(enabled);

  const bool paused = Core::GetState(Core::System::GetInstance()) == Core::State::Paused;
  m_step_action->setEnabled(paused);
  m_step_over_action->setEnabled(paused);
  m_step_out_action->setEnabled(paused);
  m_skip_action->setEnabled(paused);
  m_set_pc_action->setEnabled(paused);
}

void ToolBar::ConnectActions()
{
  connect(m_step_action, &QAction::triggered, this, &ToolBar::StepPressed);
  connect(m_step_over_action, &QAction::triggered, this, &ToolBar::StepOverPressed);
  connect(m_step_out_action, &QAction::triggered, this, &ToolBar::StepOutPressed);
  connect(m_skip_action, &QAction::triggered, this, &ToolBar::SkipPressed);
  connect(m_show_pc_action, &QAction::triggered, this, &ToolBar::ShowPCPressed);
  connect(m_set_pc_action, &QAction::triggered, this, &ToolBar::SetPCPressed);

  connect(m_open_action, &QAction::triggered, this, &ToolBar::OpenPressed);
  connect(m_refresh_action, &QAction::triggered, this, &ToolBar::RefreshPressed);
  m_refresh_action->setEnabled(false);

  connect(m_pause_play_action, &QAction::triggered, this, &ToolBar::PlayPressed);
  connect(m_stop_action, &QAction::triggered, this, &ToolBar::StopPressed);
  connect(m_fullscreen_action, &QAction::triggered, this, &ToolBar::FullScreenPressed);
  connect(m_screenshot_action, &QAction::triggered, this, &ToolBar::ScreenShotPressed);

  connect(m_config_action, &QAction::triggered, this, &ToolBar::SettingsPressed);
  connect(m_graphics_action, &QAction::triggered, this, &ToolBar::GraphicsPressed);
  connect(m_controllers_action, &QAction::triggered, this, &ToolBar::ControllersPressed);
  connect(m_cloud_saves_action, &QAction::triggered, this, &ToolBar::CloudSavesPressed);
  connect(m_netplay_setup_action, &QAction::triggered, this, &ToolBar::NetPlaySetupDialogPressed);
  connect(m_updater_action, &QAction::triggered, this, &ToolBar::InstallUpdateManuallyPressed);

  // Ensure every button has about the same width
  std::vector<QWidget*> items;
  for (const auto& action :
       {m_open_action, m_pause_play_action, m_stop_action, m_stop_action, m_fullscreen_action,
        m_screenshot_action, m_config_action, m_graphics_action, m_controllers_action,
        m_cloud_saves_action, m_step_action, m_step_over_action, m_step_out_action, m_skip_action,
        m_show_pc_action, m_set_pc_action, m_netplay_setup_action, m_updater_action})
  {
    items.emplace_back(m_toolbar->widgetForAction(action));
  }

  std::vector<int> widths;
  std::ranges::transform(items, std::back_inserter(widths),
                         [](QWidget* item) { return item->sizeHint().width(); });

  const int min_width = *std::ranges::max_element(widths) * 0.85;
  for (QWidget* widget : items)
    widget->setMinimumWidth(min_width);
}

void ToolBar::UpdatePausePlayButtonState(const bool playing_state)
{
  if (playing_state)
  {
    disconnect(m_pause_play_action, nullptr, nullptr, nullptr);
    m_pause_play_action->setText(tr("Pause"));
    m_pause_play_action->setIcon(Resources::GetThemeIcon("pause"));
    connect(m_pause_play_action, &QAction::triggered, this, &ToolBar::PausePressed);
  }
  else
  {
    disconnect(m_pause_play_action, nullptr, nullptr, nullptr);
    m_pause_play_action->setText(tr("Play"));
    m_pause_play_action->setIcon(Resources::GetThemeIcon("play"));
    connect(m_pause_play_action, &QAction::triggered, this, &ToolBar::PlayPressed);
  }
}

void ToolBar::UpdateIcons()
{
  m_step_action->setIcon(Resources::GetThemeIcon("debugger_step_in"));
  m_step_over_action->setIcon(Resources::GetThemeIcon("debugger_step_over"));
  m_step_out_action->setIcon(Resources::GetThemeIcon("debugger_step_out"));
  m_skip_action->setIcon(Resources::GetThemeIcon("debugger_skip"));
  m_show_pc_action->setIcon(Resources::GetThemeIcon("debugger_show_pc"));
  m_set_pc_action->setIcon(Resources::GetThemeIcon("debugger_set_pc"));

  m_open_action->setIcon(Resources::GetThemeIcon("open"));
  m_refresh_action->setIcon(Resources::GetThemeIcon("refresh"));

  const Core::State state = Core::GetState(Core::System::GetInstance());
  const bool playing = state != Core::State::Uninitialized && state != Core::State::Paused;
  if (!playing)
    m_pause_play_action->setIcon(Resources::GetThemeIcon("play"));
  else
    m_pause_play_action->setIcon(Resources::GetThemeIcon("pause"));

  m_stop_action->setIcon(Resources::GetThemeIcon("stop"));
  m_fullscreen_action->setIcon(Resources::GetThemeIcon("fullscreen"));
  m_screenshot_action->setIcon(Resources::GetThemeIcon("screenshot"));
  m_netplay_setup_action->setIcon(Resources::GetThemeIcon("netplay"));
  m_config_action->setIcon(Resources::GetThemeIcon("config"));
  m_controllers_action->setIcon(Resources::GetThemeIcon("gcpad"));
  m_graphics_action->setIcon(Resources::GetThemeIcon("graphics"));
  m_cloud_saves_action->setIcon(Resources::GetThemeIcon("cloud"));
  m_updater_action->setIcon(Resources::GetThemeIcon("update"));
}
