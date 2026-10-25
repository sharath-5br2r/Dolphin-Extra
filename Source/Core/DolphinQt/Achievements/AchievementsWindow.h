// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <memory>

#include <QDialog>

#include "Common/HookableEvent.h"
#include "Core/AchievementManager.h"

class AchievementHeaderWidget;
class AchievementLeaderboardWidget;
class AchievementSettingsWidget;
class AchievementProgressWidget;
class QScrollArea;
class UpdateCallback;

namespace Ui
{
class AchievementsWindow;
}

class AchievementsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit AchievementsWindow(QWidget* parent);
  ~AchievementsWindow() override;

  void UpdateData(const AchievementManager::UpdatedItems& updated_items);
  void ForceSettingsTab();

private:
  void showEvent(QShowEvent* event) override;
  void ConnectWidgets();

  std::unique_ptr<Ui::AchievementsWindow> m_ui;
  AchievementHeaderWidget* m_header_widget;
  AchievementSettingsWidget* m_settings_widget;
  AchievementProgressWidget* m_progress_widget;
  AchievementLeaderboardWidget* m_leaderboard_widget;
  QScrollArea* m_progress_scroll_area;
  QScrollArea* m_leaderboard_scroll_area;

  Common::EventHook m_event_hook;
};

#endif  // USE_RETRO_ACHIEVEMENTS
