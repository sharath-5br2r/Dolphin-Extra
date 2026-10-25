// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <array>
#include <memory>

#include <QWidget>

#include "Core/AchievementManager.h"

class AchievementLeaderboardCell;

namespace Ui
{
class AchievementLeaderboardWidget;
}

class AchievementLeaderboardWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementLeaderboardWidget(QWidget* parent);
  ~AchievementLeaderboardWidget() override;

  void UpdateData(bool clean_all);
  void UpdateData(const std::set<AchievementManager::AchievementId>& update_ids);
  void UpdateRow(AchievementManager::AchievementId leaderboard_id);

private:
  struct LeaderboardRow
  {
    std::array<AchievementLeaderboardCell*, 4> entries;
  };

  std::unique_ptr<Ui::AchievementLeaderboardWidget> m_ui;
  std::map<AchievementManager::AchievementId, LeaderboardRow> m_leaderboard_order;
};

#endif  // USE_RETRO_ACHIEVEMENTS
