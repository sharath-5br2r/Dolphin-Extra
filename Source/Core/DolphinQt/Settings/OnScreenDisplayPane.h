// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Ui
{
class OnScreenDisplayPane;
}

class OnScreenDisplayPane final : public QWidget
{
public:
  explicit OnScreenDisplayPane(QWidget* parent = nullptr);
  ~OnScreenDisplayPane() override;

private:
  void BindSettings();
  void ConnectLayout();
  void AddDescriptions();

  std::unique_ptr<Ui::OnScreenDisplayPane> m_ui;
};
