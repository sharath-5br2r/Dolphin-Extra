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
class GeneralPane;
}

class GeneralPane final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralPane(QWidget* parent = nullptr);
  ~GeneralPane() override;

private:
  void ConnectLayout();
  void BindSettings();
  void PopulateSpeedLimit();
  void AddDescriptions();

  void LoadConfig();
  void OnSaveConfig();
  void OnEmulationStateChanged(Core::State state);
  void UpdateDescriptionsUsingHardcoreStatus();

  std::unique_ptr<Ui::GeneralPane> m_ui;

// Analytics related
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  void GenerateNewIdentity();
#endif
};
