// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Core
{
enum class State;
}

namespace Ui
{
class InterfacePane;
}

class InterfacePane final : public QWidget
{
  Q_OBJECT
public:
  explicit InterfacePane(QWidget* parent = nullptr);
  ~InterfacePane() override;

private:
  void BindSettings();
  void BindLanguageChoice();
  void BindThemeChoice();
  void PopulateStyleChoices();
  void AddDescriptions();
  void ConnectLayout();
  void UpdateShowDebuggingCheckbox();
  void LoadUserStyle();
  void OnUserStyleChanged();
  void OnLanguageChanged();

  void OnEmulationStateChanged(Core::State state);

  std::unique_ptr<Ui::InterfacePane> m_ui;
};
