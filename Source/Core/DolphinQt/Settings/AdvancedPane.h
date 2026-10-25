// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Ui
{
class AdvancedPane;
}

class AdvancedPane final : public QWidget
{
  Q_OBJECT
public:
  explicit AdvancedPane(QWidget* parent = nullptr);
  ~AdvancedPane() override;

private:
  void ConfigureWidgets();
  void BindSettings();
  void ConnectWidgets();
  void AddDescriptions();
  void UpdateCpuClockLabel();
  void UpdateVbiLabel();
  void Update();

  void OnResetButtonClicked();

  std::unique_ptr<Ui::AdvancedPane> m_ui;
};
