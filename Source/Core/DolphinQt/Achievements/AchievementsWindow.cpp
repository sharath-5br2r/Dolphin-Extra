// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementsWindow.h"

#include <mutex>

#include <rcheevos/include/rc_error.h>

#include <QDialogButtonBox>
#include <QScrollArea>
#include <QScrollBar>

#include "Core/AchievementManager.h"

#include "DolphinQt/Achievements/AchievementHeaderWidget.h"
#include "DolphinQt/Achievements/AchievementLeaderboardWidget.h"
#include "DolphinQt/Achievements/AchievementProgressWidget.h"
#include "DolphinQt/Achievements/AchievementSettingsWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings.h"

#include "ui_AchievementsWindow.h"

AchievementsWindow::AchievementsWindow(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::AchievementsWindow>())
{
  m_ui->setupUi(this);

  const bool is_game_loaded = AchievementManager::GetInstance().IsGameLoaded();
  m_header_widget = new AchievementHeaderWidget(this);
  m_settings_widget = new AchievementSettingsWidget(m_ui->settingsTab);
  m_progress_widget = new AchievementProgressWidget(m_ui->progressTab);
  m_leaderboard_widget = new AchievementLeaderboardWidget(m_ui->leaderboardsTab);

  m_ui->headerLayout->addWidget(m_header_widget);
  m_ui->settingsLayout->addWidget(GetWrappedWidget(m_settings_widget));
  m_progress_scroll_area = static_cast<QScrollArea*>(GetWrappedWidget(m_progress_widget));
  m_leaderboard_scroll_area = static_cast<QScrollArea*>(GetWrappedWidget(m_leaderboard_widget));
  m_ui->progressLayout->addWidget(m_progress_scroll_area);
  m_ui->leaderboardsLayout->addWidget(m_leaderboard_scroll_area);
  m_ui->tabWidget->setTabVisible(1, is_game_loaded);
  m_ui->tabWidget->setTabVisible(2, is_game_loaded);
  adjustSize();

  ConnectWidgets();

  m_event_hook = AchievementManager::GetInstance().update_event.Register(
      [this](AchievementManager::UpdatedItems updated_items) {
        QueueOnObject(this, [this, updated_items = std::move(updated_items)] {
          AchievementsWindow::UpdateData(updated_items);
        });
      });
  UpdateData(AchievementManager::UpdatedItems{.all = true});

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this] { m_settings_widget->UpdateData(RC_OK); });
}

AchievementsWindow::~AchievementsWindow() = default;

void AchievementsWindow::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  update();
}

void AchievementsWindow::ConnectWidgets()
{
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AchievementsWindow::UpdateData(const AchievementManager::UpdatedItems& updated_items)
{
  m_settings_widget->UpdateData(updated_items.failed_login_code);
  if (updated_items.all)
  {
    m_header_widget->UpdateData();
    m_progress_widget->UpdateData(true);
    m_leaderboard_widget->UpdateData(true);
    m_progress_scroll_area->verticalScrollBar()->setValue(0);
    m_leaderboard_scroll_area->verticalScrollBar()->setValue(0);
  }
  else
  {
    if (updated_items.player_icon || updated_items.game_icon || updated_items.rich_presence ||
        updated_items.all_achievements || updated_items.achievements.size() > 0)
    {
      m_header_widget->UpdateData();
    }
    if (updated_items.all_achievements || updated_items.rich_presence)
      m_progress_widget->UpdateData(false);
    else if (updated_items.achievements.size() > 0)
      m_progress_widget->UpdateData(updated_items.achievements);
    if (updated_items.all_leaderboards)
      m_leaderboard_widget->UpdateData(false);
    else if (updated_items.leaderboards.size() > 0)
      m_leaderboard_widget->UpdateData(updated_items.leaderboards);
  }

  {
    auto& instance = AchievementManager::GetInstance();
    std::lock_guard lg{instance.GetLock()};
    const bool is_game_loaded = instance.IsGameLoaded();
    m_header_widget->setVisible(instance.HasAPIToken());
    m_ui->tabWidget->setTabVisible(1, is_game_loaded);
    m_ui->tabWidget->setTabVisible(2, is_game_loaded);
  }
  update();
}

void AchievementsWindow::ForceSettingsTab()
{
  m_ui->tabWidget->setCurrentIndex(0);
}

#endif  // USE_RETRO_ACHIEVEMENTS
