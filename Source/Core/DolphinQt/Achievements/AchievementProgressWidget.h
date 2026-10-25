// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <memory>

#include <QWidget>

#include "Common/CommonTypes.h"
#include "Core/AchievementManager.h"

class AchievementBox;

struct rc_api_achievement_definition_t;

namespace Ui
{
class AchievementProgressWidget;
}

class AchievementProgressWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementProgressWidget(QWidget* parent);
  ~AchievementProgressWidget() override;

  void UpdateData(bool clean_all);
  void UpdateData(const std::set<AchievementManager::AchievementId>& update_ids);

private:
  std::unique_ptr<Ui::AchievementProgressWidget> m_ui;
  std::map<AchievementManager::AchievementId, std::shared_ptr<AchievementBox>> m_achievement_boxes;
};

#endif  // USE_RETRO_ACHIEVEMENTS
