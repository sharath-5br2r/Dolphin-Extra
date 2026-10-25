// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QStringList>
#include <QWidget>

namespace Ui
{
class DualShockUDPClientWidget;
}

class DualShockUDPClientWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit DualShockUDPClientWidget();
  ~DualShockUDPClientWidget() override;

signals:
  // Emitted when config has changed so widgets can update to reflect the change.
  void ConfigChanged();

private:
  void ConnectWidgets();

  void SetButtonEnableStates();
  void RefreshServerList();

  void OnServerAdded();
  void OnServerEdited();
  void OnServerRemoved();
  void OnServerSelection();
  void OnServersToggled();

  std::unique_ptr<Ui::DualShockUDPClientWidget> m_ui;
};
