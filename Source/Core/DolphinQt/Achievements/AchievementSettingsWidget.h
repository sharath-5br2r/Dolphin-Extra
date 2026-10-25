// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <memory>

#include <QWidget>

namespace Ui
{
class AchievementSettingsWidget;
}

class AchievementSettingsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementSettingsWidget(QWidget* parent);
  ~AchievementSettingsWidget() override;

  void UpdateData(int login_failed_code);

private:
  void OnControllerInterfaceConfigure();

  void SetDescriptions();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void ToggleRAIntegration();
  void Login();
  void Logout();
  void ToggleHardcore();
  void ToggleUnofficial();
  void ToggleEncore();
  void ToggleSpectator();
  void ToggleLeaderboardTracker();
  void ToggleChallengeIndicators();
  void ToggleDiscordPresence();
  void ToggleProgress();

  std::unique_ptr<Ui::AchievementSettingsWidget> m_ui;
};

#endif  // USE_RETRO_ACHIEVEMENTS
