// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <memory>

#include <QGroupBox>

#include "rcheevos/include/rc_client.h"

class QWidget;

struct rc_api_achievement_definition_t;

namespace Ui
{
class AchievementBox;
}

class AchievementBox final : public QGroupBox
{
  Q_OBJECT
public:
  explicit AchievementBox(QWidget* parent, const rc_client_achievement_t* achievement);
  ~AchievementBox() override;

  void UpdateData();
  void UpdateProgress();

private:
  std::unique_ptr<Ui::AchievementBox> m_ui;
  const rc_client_achievement_t* m_achievement;
};

#endif  // USE_RETRO_ACHIEVEMENTS
