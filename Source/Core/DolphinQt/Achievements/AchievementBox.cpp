// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementBox.h"

#include <QByteArray>
#include <QDateTime>
#include <QSizePolicy>
#include <QWidget>

#include "Core/AchievementManager.h"

#include "DolphinQt/QtUtils/FromStdString.h"

#include "ui_AchievementBox.h"

static constexpr size_t PROGRESS_LENGTH = 24;

AchievementBox::AchievementBox(QWidget* parent, const rc_client_achievement_t* achievement)
    : QGroupBox(parent), m_ui(std::make_unique<Ui::AchievementBox>()), m_achievement(achievement)
{
  m_ui->setupUi(this);

  const auto& instance = AchievementManager::GetInstance();
  if (!instance.IsGameLoaded())
    return;

  m_ui->titleLabel->setText(QString::fromUtf8(achievement->title, strlen(achievement->title)));
  m_ui->descriptionLabel->setText(
      QString::fromUtf8(achievement->description, strlen(achievement->description)));
  m_ui->pointsLabel->setText(tr("%1 points").arg(achievement->points));

  QSizePolicy sp_retain = m_ui->progressBar->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_ui->progressBar->setSizePolicy(sp_retain);

  UpdateData();
}

AchievementBox::~AchievementBox() = default;

void AchievementBox::UpdateData()
{
  {
    std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
    // rc_client guarantees m_achievement will be valid as long as the game is loaded
    if (!AchievementManager::GetInstance().IsGameLoaded())
      return;

    const auto& badge = AchievementManager::GetInstance().GetAchievementBadge(
        m_achievement->id, !m_achievement->unlocked);
    std::string_view color = AchievementManager::GRAY;
    if (m_achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE)
      color = AchievementManager::GOLD;
    else if (m_achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE)
      color = AchievementManager::BLUE;
    QImage i_badge(badge.data.data(), badge.width, badge.height, QImage::Format_RGBA8888);
    m_ui->badgeLabel->setPixmap(
        QPixmap::fromImage(i_badge).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_ui->badgeLabel->adjustSize();
    m_ui->badgeLabel->setStyleSheet(
        QStringLiteral("border: 4px solid %1").arg(QtUtils::FromStdString(color)));

    if (m_achievement->unlocked)
    {
      if (m_achievement->unlock_time != 0)
      {
        m_ui->statusLabel->setText(
            // i18n: %1 is a date/time.
            tr("Unlocked at %1")
                .arg(QDateTime::fromSecsSinceEpoch(m_achievement->unlock_time).toString()));
      }
      else
      {
        m_ui->statusLabel->setText(tr("Unlocked"));
      }
    }
    else
    {
      m_ui->statusLabel->setText(tr("Locked"));
    }
  }

  UpdateProgress();
}

void AchievementBox::UpdateProgress()
{
  std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
  // rc_client guarantees m_achievement will be valid as long as the game is loaded
  if (!AchievementManager::GetInstance().IsGameLoaded())
    return;

  if (m_achievement->measured_percent > 0.000)
  {
    m_ui->progressBar->setRange(0, 100);
    m_ui->progressBar->setValue(m_achievement->unlocked ? 100 : m_achievement->measured_percent);
    m_ui->progressLabel->setText(
        QString::fromUtf8(m_achievement->measured_progress,
                          qstrnlen(m_achievement->measured_progress, PROGRESS_LENGTH)));
    m_ui->progressLabel->setVisible(!m_achievement->unlocked);
    m_ui->progressBar->setVisible(true);
  }
  else
  {
    m_ui->progressBar->setVisible(false);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
