// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

class WiimoteControllersWidget;

namespace Ui
{
class ControllersPane;
}

class ControllersPane final : public QWidget
{
  Q_OBJECT
public:
  ControllersPane();
  ~ControllersPane() override;

private:
  void CreateSections();

  std::unique_ptr<Ui::ControllersPane> m_ui;
  WiimoteControllersWidget* m_wiimote_controllers;
};
