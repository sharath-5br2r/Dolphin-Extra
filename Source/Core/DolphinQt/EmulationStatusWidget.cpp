// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulationStatusWidget.h"

#include <cmath>

#include <QLabel>

#include "ui_EmulationStatusWidget.h"

namespace
{
void SetMetric(QLabel* label, const QString& text, bool available)
{
  label->setText(text);
  label->setVisible(available);
}
}  // namespace

EmulationStatusWidget::EmulationStatusWidget(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::EmulationStatusWidget>())
{
  m_ui->setupUi(this);
  m_renderer = m_ui->statusRenderer;
  m_resolution = m_ui->statusResolution;
  m_fps = m_ui->statusFps;
  m_vps = m_ui->statusVps;
  m_speed = m_ui->statusSpeed;
  m_volume = m_ui->statusVolume;

  for (QLabel* label : {m_renderer, m_resolution, m_fps, m_vps, m_speed, m_volume})
    label->setContentsMargins(10, 0, 10, 0);
}

EmulationStatusWidget::~EmulationStatusWidget() = default;

void EmulationStatusWidget::SetStatus(const EmulationStatus& status)
{
  if (!status.active)
  {
    hide();
    return;
  }

  SetMetric(m_renderer, tr("Renderer: %1").arg(status.renderer), !status.renderer.isEmpty());
  SetMetric(m_resolution, tr("Resolution: %1x%2").arg(status.width).arg(status.height),
            status.width != 0 && status.height != 0);
  SetMetric(m_fps, tr("FPS: %1").arg(status.fps, 0, 'f', 1), status.fps > 0.0);
  SetMetric(m_vps, tr("VPS: %1").arg(status.vps, 0, 'f', 1), status.vps > 0.0);
  SetMetric(m_speed, tr("Speed: %1%").arg(std::lround(status.speed * 100.0)), status.speed > 0.0);
  m_volume->setText(status.muted ? tr("Volume: Muted") : tr("Volume: %1%").arg(status.volume));
  m_volume->show();
  show();
}
