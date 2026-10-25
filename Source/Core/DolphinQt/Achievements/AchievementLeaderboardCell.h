// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <memory>

#include <QWidget>

class QString;

namespace Ui
{
class AchievementLeaderboardCell;
}

class AchievementLeaderboardCell final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementLeaderboardCell(QWidget* parent);
  ~AchievementLeaderboardCell() override;

  void SetLeaderboard(const QString& title, const QString& description);
  void SetScore(const QString& rank, const QString& username, const QString& score);
  void SetPlaceholder();

private:
  std::unique_ptr<Ui::AchievementLeaderboardCell> m_ui;
};

#endif  // USE_RETRO_ACHIEVEMENTS
