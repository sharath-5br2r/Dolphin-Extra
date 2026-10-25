// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <array>
#include <memory>

class QComboBox;
class QPushButton;

namespace Core
{
enum class State;
}

namespace Ui
{
class GamecubeControllersWidget;
}

class GamecubeControllersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit GamecubeControllersWidget(QWidget* parent);
  ~GamecubeControllersWidget() override;

private:
  void LoadSettings(Core::State state);
  void SaveSettings();

  void OnGCTypeChanged(size_t index);
  void OnGCPadConfigure(size_t index);

  void InitializeControls();
  void ConnectWidgets();

  std::unique_ptr<Ui::GamecubeControllersWidget> m_ui;
  std::array<QComboBox*, 4> m_gc_controller_boxes;
  std::array<QPushButton*, 4> m_gc_buttons;
};
