// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QString>
#include <QWidget>

#include "Common/CommonTypes.h"

namespace Ui
{
class EmulationStatusWidget;
}

class QLabel;

struct EmulationStatus
{
  bool active = false;
  QString renderer;
  u32 width = 0;
  u32 height = 0;
  double fps = 0.0;
  double vps = 0.0;
  double speed = 0.0;
  int volume = 100;
  bool muted = false;
};

class EmulationStatusWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit EmulationStatusWidget(QWidget* parent = nullptr);
  ~EmulationStatusWidget() override;

  void SetStatus(const EmulationStatus& status);

private:
  std::unique_ptr<Ui::EmulationStatusWidget> m_ui;
  QLabel* m_renderer;
  QLabel* m_resolution;
  QLabel* m_fps;
  QLabel* m_vps;
  QLabel* m_speed;
  QLabel* m_volume;
};
