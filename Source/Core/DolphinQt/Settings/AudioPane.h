// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Ui
{
class AudioPane;
}

class AudioPane final : public QWidget
{
  Q_OBJECT
public:
  explicit AudioPane(QWidget* parent = nullptr);
  ~AudioPane() override;

private:
  void BindSettings();
  void ConfigureLayout();
  void ConnectWidgets();
  void AddDescriptions();

  void OnEmulationStateChanged(bool running);
  void OnBackendChanged();
  void OnDspChanged();

  void UpdateWiimoteRoutingEnabled();
  void UpdateGBARoutingEnabled();

  void CheckNeedForLatencyControl();
  bool m_latency_control_supported = false;

  std::unique_ptr<Ui::AudioPane> m_ui;
};
