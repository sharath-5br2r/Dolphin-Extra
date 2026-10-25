// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementHeaderWidget.h"

#include <QSizePolicy>
#include <QString>

#include <rcheevos/include/rc_client.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"

#include "DolphinQt/QtUtils/FromStdString.h"

#include "ui_AchievementHeaderWidget.h"

AchievementHeaderWidget::AchievementHeaderWidget(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::AchievementHeaderWidget>())
{
  m_ui->setupUi(this);

  QSizePolicy sp_retain = m_ui->gameProgress->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_ui->gameProgress->setSizePolicy(sp_retain);
}

AchievementHeaderWidget::~AchievementHeaderWidget() = default;

void AchievementHeaderWidget::UpdateData()
{
  std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
  auto& instance = AchievementManager::GetInstance();
  if (!Config::Get(Config::RA_ENABLED) || !instance.HasAPIToken())
  {
    m_ui->headerBox->setVisible(false);
    return;
  }
  m_ui->headerBox->setVisible(true);

  QString user_name = QtUtils::FromStdString(instance.GetPlayerDisplayName());
  QString game_name = QtUtils::FromStdString(instance.GetGameDisplayName());
  const AchievementManager::Badge& player_badge = instance.GetPlayerBadge();
  const AchievementManager::Badge& game_badge = instance.GetGameBadge();

  m_ui->userIcon->setVisible(false);
  m_ui->userIcon->clear();
  m_ui->userIcon->setText({});
  if (!player_badge.data.empty())
  {
    QImage i_user_icon(player_badge.data.data(), player_badge.width, player_badge.height,
                       QImage::Format_RGBA8888);
    m_ui->userIcon->setPixmap(QPixmap::fromImage(i_user_icon)
                                  .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  m_ui->userIcon->adjustSize();
  m_ui->userIcon->setStyleSheet(QStringLiteral("border: 4px solid transparent"));
  m_ui->userIcon->setVisible(true);

  m_ui->gameIcon->setVisible(false);
  m_ui->gameIcon->clear();
  m_ui->gameIcon->setText({});

  if (instance.IsGameLoaded())
  {
    rc_client_user_game_summary_t game_summary;
    rc_client_get_user_game_summary(instance.GetClient(), &game_summary);
    if (!game_badge.data.empty())
    {
      QImage i_game_icon(game_badge.data.data(), game_badge.width, game_badge.height,
                         QImage::Format_RGBA8888);
      m_ui->gameIcon->setPixmap(QPixmap::fromImage(i_game_icon)
                                    .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_ui->gameIcon->adjustSize();
    std::string_view color = AchievementManager::GRAY;
    if (game_summary.num_core_achievements == game_summary.num_unlocked_achievements)
    {
      color = instance.IsHardcoreModeActive() ? AchievementManager::GOLD : AchievementManager::BLUE;
    }
    m_ui->gameIcon->setStyleSheet(
        QStringLiteral("border: 4px solid %1").arg(QtUtils::FromStdString(color)));
    m_ui->gameIcon->setVisible(true);

    m_ui->nameLabel->setText(tr("%1 is playing %2").arg(user_name).arg(game_name));
    m_ui->pointsLabel->setText(tr("%1 has unlocked %2/%3 achievements worth %4/%5 points")
                                   .arg(user_name)
                                   .arg(game_summary.num_unlocked_achievements)
                                   .arg(game_summary.num_core_achievements)
                                   .arg(game_summary.points_unlocked)
                                   .arg(game_summary.points_core));

    // This ensures that 0/0 renders as empty instead of full
    m_ui->gameProgress->setRange(
        0, (game_summary.num_core_achievements == 0) ? 1 : game_summary.num_core_achievements);
    m_ui->gameProgress->setVisible(true);
    m_ui->gameProgress->setValue(game_summary.num_unlocked_achievements);
    m_ui->progressLabel->setVisible(true);
    m_ui->progressLabel->setText(tr("%1/%2")
                                     .arg(game_summary.num_unlocked_achievements)
                                     .arg(game_summary.num_core_achievements));
    m_ui->richPresenceLabel->setText(QString::fromUtf8(instance.GetRichPresence().data()));
    m_ui->richPresenceLabel->setVisible(true);
  }
  else
  {
    m_ui->nameLabel->setText(user_name);
    m_ui->pointsLabel->setText(tr("%1 points").arg(instance.GetPlayerScore()));

    m_ui->gameProgress->setVisible(false);
    m_ui->progressLabel->setVisible(false);
    m_ui->richPresenceLabel->setVisible(false);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
