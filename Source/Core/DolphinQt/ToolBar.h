// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

class QAction;
class QToolBar;

namespace Core
{
enum class State;
}

namespace Ui
{
class MainWindow;
}

class ToolBar final : public QObject
{
  Q_OBJECT

public:
  explicit ToolBar(Ui::MainWindow& ui, QObject* parent = nullptr);

signals:
  void OpenPressed();
  void RefreshPressed();
  void PlayPressed();
  void PausePressed();
  void StopPressed();
  void FullScreenPressed();
  void ScreenShotPressed();
  void NetPlaySetupDialogPressed();

  void SettingsPressed();
  void ControllersPressed();
  void GraphicsPressed();
  void InstallUpdateManuallyPressed();
  void CloudSavesPressed();

  void StepPressed();
  void StepOverPressed();
  void StepOutPressed();
  void SkipPressed();
  void ShowPCPressed();
  void SetPCPressed();

private:
  void OnEmulationStateChanged(Core::State state);
  void OnDebugModeToggled(bool enabled);

  void ConnectActions();
  void UpdateIcons();
  void UpdatePausePlayButtonState(bool playing_state);

  QToolBar* m_toolbar;
  QAction* m_open_action;
  QAction* m_refresh_action;
  QAction* m_pause_play_action;
  QAction* m_stop_action;
  QAction* m_fullscreen_action;
  QAction* m_screenshot_action;
  QAction* m_config_action;
  QAction* m_controllers_action;
  QAction* m_graphics_action;
  QAction* m_netplay_setup_action;
  QAction* m_updater_action;
  QAction* m_cloud_saves_action;

  QAction* m_step_action;
  QAction* m_step_over_action;
  QAction* m_step_out_action;
  QAction* m_skip_action;
  QAction* m_show_pc_action;
  QAction* m_set_pc_action;
};
