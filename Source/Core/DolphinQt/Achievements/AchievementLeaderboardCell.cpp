// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementLeaderboardCell.h"

#include "ui_AchievementLeaderboardCell.h"

AchievementLeaderboardCell::AchievementLeaderboardCell(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::AchievementLeaderboardCell>())
{
  m_ui->setupUi(this);
}

AchievementLeaderboardCell::~AchievementLeaderboardCell() = default;

void AchievementLeaderboardCell::SetLeaderboard(const QString& title, const QString& description)
{
  m_ui->primaryLabel->setText(title);
  m_ui->secondaryLabel->setText(description);
  m_ui->tertiaryLabel->setVisible(false);
}

void AchievementLeaderboardCell::SetScore(const QString& rank, const QString& username,
                                          const QString& score)
{
  m_ui->primaryLabel->setText(rank);
  m_ui->secondaryLabel->setText(username);
  m_ui->tertiaryLabel->setText(score);
  m_ui->tertiaryLabel->setVisible(true);
}

void AchievementLeaderboardCell::SetPlaceholder()
{
  const QString placeholder = QStringLiteral("---");
  SetScore(placeholder, placeholder, placeholder);
}

#endif  // USE_RETRO_ACHIEVEMENTS
