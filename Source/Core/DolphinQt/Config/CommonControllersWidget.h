// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Ui
{
class CommonControllersWidget;
}

class CommonControllersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit CommonControllersWidget(QWidget* parent);
  ~CommonControllersWidget() override;

private:
  void OnControllerInterfaceConfigure();
  void OnSDLHintConfigure();

  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  std::unique_ptr<Ui::CommonControllersWidget> m_ui;
};
