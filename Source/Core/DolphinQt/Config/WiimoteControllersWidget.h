// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>

#include <QWidget>

#include "Common/WorkQueueThread.h"
#include "Core/USBUtils.h"

class QAction;
class QComboBox;
class QLabel;
class QPushButton;

namespace Core
{
enum class State;
}

namespace Ui
{
class WiimoteControllersWidget;
}

class WiimoteControllersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit WiimoteControllersWidget(QWidget* parent);
  ~WiimoteControllersWidget() override;

private:
  void SaveSettings();
  void OnBluetoothPassthroughDeviceChanged(int index);
  void OnBluetoothPassthroughSyncPressed();
  void OnBluetoothPassthroughResetPressed();
  void OnBluetoothAdapterRefreshComplete(std::span<const USBUtils::DeviceInfo> devices);
  void OnWiimoteRefreshPressed();
  void OnWiimoteConfigure(size_t index);
  void StartBluetoothAdapterRefresh();
  void UpdateBluetoothAdapterWidgetsEnabled(Core::State state);

  void InitializeControls();
  void ConnectWidgets();
  void LoadSettings(Core::State state);

#if defined(_WIN32)
  void AsyncRefreshActionHelper(std::invocable<> auto);
  void TriggerHostWiimoteSync();
  void TriggerHostWiimoteReset();
#endif

  std::unique_ptr<Ui::WiimoteControllersWidget> m_ui;
  std::array<QLabel*, 4> m_wiimote_labels;
  std::array<QComboBox*, 4> m_wiimote_boxes;
  std::array<QPushButton*, 4> m_wiimote_buttons;
  std::array<QLabel*, 2> m_wiimote_pt_labels;

  Common::AsyncWorkThreadSP m_bluetooth_adapter_refresh_thread;
  bool m_bluetooth_adapter_scan_in_progress = false;
};
