// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementProgressWidget.h"

#include <QLabel>
#include <QString>

#include "Core/AchievementManager.h"

#include "DolphinQt/Achievements/AchievementBox.h"
#include "DolphinQt/QtUtils/ClearLayoutRecursively.h"

#include "ui_AchievementProgressWidget.h"

AchievementProgressWidget::AchievementProgressWidget(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::AchievementProgressWidget>())
{
  m_ui->setupUi(this);
}

AchievementProgressWidget::~AchievementProgressWidget() = default;

void AchievementProgressWidget::UpdateData(bool clean_all)
{
  if (clean_all)
  {
    ClearLayoutRecursively(m_ui->commonLayout);
    m_achievement_boxes.clear();
  }
  else
  {
    while (auto* item = m_ui->commonLayout->takeAt(0))
    {
      auto* widget = item->widget();
      m_ui->commonLayout->removeWidget(widget);
      if (std::strcmp(widget->metaObject()->className(), "QLabel") == 0)
        widget->deleteLater();
      delete item;
    }
  }

  auto& instance = AchievementManager::GetInstance();
  if (!instance.IsGameLoaded())
    return;
  auto* client = instance.GetClient();
  auto* achievement_list =
      rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  if (!achievement_list)
    return;
  for (u32 ix = 0; ix < achievement_list->num_buckets; ix++)
  {
    m_ui->commonLayout->addWidget(new QLabel(tr(achievement_list->buckets[ix].label)));
    for (u32 jx = 0; jx < achievement_list->buckets[ix].num_achievements; jx++)
    {
      auto* achievement = achievement_list->buckets[ix].achievements[jx];
      auto box_itr = m_achievement_boxes.lower_bound(achievement->id);
      if (box_itr != m_achievement_boxes.end() && box_itr->first == achievement->id)
      {
        box_itr->second->UpdateProgress();
        m_ui->commonLayout->addWidget(box_itr->second.get());
      }
      else
      {
        const auto new_box_itr = m_achievement_boxes.try_emplace(
            box_itr, achievement->id, std::make_shared<AchievementBox>(this, achievement));
        m_ui->commonLayout->addWidget(new_box_itr->second.get());
      }
    }
  }
  rc_client_destroy_achievement_list(achievement_list);
}

void AchievementProgressWidget::UpdateData(
    const std::set<AchievementManager::AchievementId>& update_ids)
{
  for (auto& [id, box] : m_achievement_boxes)
  {
    if (update_ids.contains(id))
    {
      box->UpdateData();
    }
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
