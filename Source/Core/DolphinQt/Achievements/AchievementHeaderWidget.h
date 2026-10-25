// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <memory>

#include <QWidget>

namespace Ui
{
class AchievementHeaderWidget;
}

class AchievementHeaderWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementHeaderWidget(QWidget* parent);
  ~AchievementHeaderWidget() override;

  void UpdateData();

private:
  std::unique_ptr<Ui::AchievementHeaderWidget> m_ui;
};

#endif  // USE_RETRO_ACHIEVEMENTS
