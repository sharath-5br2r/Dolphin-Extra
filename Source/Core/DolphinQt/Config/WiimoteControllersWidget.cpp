// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/WiimoteControllersWidget.h"

#include <memory>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>

#include "Common/Config/Config.h"
#include "Common/WorkQueueThread.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/LibUSBBluetoothAdapter.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"
#include "Core/USBUtils.h"
#include "Core/WiiUtils.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/USBDevicePicker.h"

#include "ui_WiimoteControllersWidget.h"

#if defined(_WIN32)
#include "Core/HW//WiimoteReal/IOWin.h"
#endif

WiimoteControllersWidget::WiimoteControllersWidget(QWidget* parent)
    : QWidget(parent), m_ui{std::make_unique<Ui::WiimoteControllersWidget>()}
{
  m_ui->setupUi(this);
  InitializeControls();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          [this] { LoadSettings(Core::GetState(Core::System::GetInstance())); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this](Core::State state) { LoadSettings(state); });
  LoadSettings(Core::GetState(Core::System::GetInstance()));

  m_bluetooth_adapter_refresh_thread.Reset("Bluetooth Adapter Refresh Thread");
  StartBluetoothAdapterRefresh();
}

WiimoteControllersWidget::~WiimoteControllersWidget()
{
  m_bluetooth_adapter_refresh_thread.WaitForCompletion();
}

void WiimoteControllersWidget::StartBluetoothAdapterRefresh()
{
#ifdef __LIBUSB__
  if (m_bluetooth_adapter_scan_in_progress)
    return;

  m_ui->bluetoothAdaptersComboBox->clear();
  m_ui->bluetoothAdaptersComboBox->setDisabled(true);
  m_ui->bluetoothAdaptersComboBox->addItem(tr("Scanning for adapters..."));

  m_bluetooth_adapter_scan_in_progress = true;

  const auto scan_func = [this]() {
    INFO_LOG_FMT(COMMON, "Refreshing Bluetooth adapter list...");
    auto device_list = USBUtils::ListDevices(LibUSBBluetoothAdapter::IsBluetoothDevice);
    INFO_LOG_FMT(COMMON, "{} Bluetooth adapters available.", device_list.size());
    const auto refresh_complete_func = [this, devices = std::move(device_list)]() {
      OnBluetoothAdapterRefreshComplete(devices);
    };
    QueueOnObject(this, refresh_complete_func);
  };

  m_bluetooth_adapter_refresh_thread.Push(scan_func);
#endif
}

void WiimoteControllersWidget::OnBluetoothAdapterRefreshComplete(
    std::span<const USBUtils::DeviceInfo> devices)
{
  const int configured_vid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
  const int configured_pid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
  bool found_configured_device = configured_vid == -1 || configured_pid == -1;

  m_ui->bluetoothAdaptersComboBox->clear();
  m_bluetooth_adapter_scan_in_progress = false;

  const auto state = Core::GetState(Core::System::GetInstance());
  UpdateBluetoothAdapterWidgetsEnabled(state);

  m_ui->bluetoothAdaptersComboBox->addItem(tr("Automatic"));

  for (auto& device : devices)
  {
    m_ui->bluetoothAdaptersComboBox->addItem(QString::fromStdString(device.ToDisplayString()),
                                             QVariant::fromValue(device));

    if (!found_configured_device &&
        LibUSBBluetoothAdapter::IsConfiguredBluetoothDevice(device.vid, device.pid))
    {
      found_configured_device = true;
      m_ui->bluetoothAdaptersComboBox->setCurrentIndex(m_ui->bluetoothAdaptersComboBox->count() -
                                                       1);
    }
  }

  if (!found_configured_device)
  {
    const QString name = QLatin1Char{'['} + tr("disconnected") + QLatin1Char(']');
    const std::string name_str = name.toStdString();

    USBUtils::DeviceInfo disconnected_device;
    disconnected_device.vid = configured_vid;
    disconnected_device.pid = configured_pid;

    const QString device_info =
        QString::fromStdString(disconnected_device.ToDisplayString(name_str));

    m_ui->bluetoothAdaptersComboBox->insertSeparator(m_ui->bluetoothAdaptersComboBox->count());
    m_ui->bluetoothAdaptersComboBox->addItem(device_info, QVariant::fromValue(disconnected_device));
    m_ui->bluetoothAdaptersComboBox->setCurrentIndex(m_ui->bluetoothAdaptersComboBox->count() - 1);
  }

  m_ui->bluetoothAdaptersComboBox->insertSeparator(m_ui->bluetoothAdaptersComboBox->count());
  m_ui->bluetoothAdaptersComboBox->addItem(tr("More Options..."));
}

void WiimoteControllersWidget::InitializeControls()
{
  m_wiimote_labels = {m_ui->wiimote1Label, m_ui->wiimote2Label, m_ui->wiimote3Label,
                      m_ui->wiimote4Label};
  m_wiimote_boxes = {m_ui->wiimote1ComboBox, m_ui->wiimote2ComboBox, m_ui->wiimote3ComboBox,
                     m_ui->wiimote4ComboBox};
  m_wiimote_buttons = {m_ui->wiimote1ConfigureButton, m_ui->wiimote2ConfigureButton,
                       m_ui->wiimote3ConfigureButton, m_ui->wiimote4ConfigureButton};
  m_wiimote_pt_labels = {m_ui->passthroughSyncLabel, m_ui->passthroughResetLabel};

  for (size_t i = 0; i < m_wiimote_labels.size(); ++i)
    m_wiimote_labels[i]->setText(tr("Wii Remote %1").arg(i + 1));

  auto* const wiimote_refresh_action = new QAction(tr("Refresh"), m_ui->wiimoteRefreshButton);
  m_ui->wiimoteRefreshButton->setDefaultAction(wiimote_refresh_action);
  connect(wiimote_refresh_action, &QAction::triggered, this,
          &WiimoteControllersWidget::OnWiimoteRefreshPressed);

#if defined(_WIN32)
  m_ui->refreshIndicatorLabel->setPixmap(
      style()->standardIcon(QStyle::SP_BrowserReload).pixmap(16, 16));

  auto* const wiimote_sync_action = new QAction(tr("Sync"), m_ui->wiimoteRefreshButton);
  m_ui->wiimoteRefreshButton->addAction(wiimote_sync_action);
  connect(wiimote_sync_action, &QAction::triggered, this,
          &WiimoteControllersWidget::TriggerHostWiimoteSync);

  auto* const wiimote_reset_action = new QAction(tr("Reset"), m_ui->wiimoteRefreshButton);
  m_ui->wiimoteRefreshButton->addAction(wiimote_reset_action);
  connect(wiimote_reset_action, &QAction::triggered, this,
          &WiimoteControllersWidget::TriggerHostWiimoteReset);
#endif
}

void WiimoteControllersWidget::ConnectWidgets()
{
  connect(m_ui->passthroughRadioButton, &QRadioButton::toggled, this, [this] {
    SaveSettings();
    LoadSettings(Core::GetState(Core::System::GetInstance()));
  });
  connect(m_ui->controllerInterfaceCheckBox, &QCheckBox::toggled, this, [this] {
    SaveSettings();
    LoadSettings(Core::GetState(Core::System::GetInstance()));
    WiimoteReal::HandleWiimotesInControllerInterfaceSettingChange();
  });
  connect(m_ui->continuousScanningCheckBox, &QCheckBox::toggled, this, [this] {
    SaveSettings();
    LoadSettings(Core::GetState(Core::System::GetInstance()));
  });

  connect(m_ui->realBalanceBoardCheckBox, &QCheckBox::toggled, this,
          &WiimoteControllersWidget::SaveSettings);
  connect(m_ui->speakerDataCheckBox, &QCheckBox::toggled, this,
          &WiimoteControllersWidget::SaveSettings);
  connect(m_ui->bluetoothAdaptersComboBox, &QComboBox::activated, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughDeviceChanged);
  connect(m_ui->bluetoothAdaptersRefreshButton, &QPushButton::clicked, this,
          &WiimoteControllersWidget::StartBluetoothAdapterRefresh);
  connect(m_ui->passthroughSyncButton, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughSyncPressed);
  connect(m_ui->passthroughResetButton, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughResetPressed);

  for (size_t i = 0; i < m_wiimote_boxes.size(); ++i)
  {
    connect(m_wiimote_boxes[i], &QComboBox::currentIndexChanged, this, [this] {
      SaveSettings();
      LoadSettings(Core::GetState(Core::System::GetInstance()));
    });
    connect(m_wiimote_buttons[i], &QPushButton::clicked, this,
            [this, i] { OnWiimoteConfigure(i); });
  }
}

void WiimoteControllersWidget::OnBluetoothPassthroughDeviceChanged(int index)
{
  std::optional<USBUtils::DeviceInfo> device_info;
  bool needs_refresh = false;
  // "Automatic" selection
  if (index == 0)
  {
    Config::DeleteKey(Config::GetActiveLayerForConfig(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID),
                      Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
    Config::DeleteKey(Config::GetActiveLayerForConfig(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID),
                      Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
    return;
  }
  // "More Options..." selection
  else if (index == m_ui->bluetoothAdaptersComboBox->count() - 1)
  {
    device_info = USBDevicePicker::Run(this, tr("Select a Bluetooth Device"));
    needs_refresh = true;
  }
  else
  {
    const QVariant item_data = m_ui->bluetoothAdaptersComboBox->itemData(index);

    if (!item_data.isValid() || !item_data.canConvert<USBUtils::DeviceInfo>())
    {
      ERROR_LOG_FMT(COMMON, "Invalid Bluetooth device info selected in WiimoteControllersWidget");
      return;
    }
    device_info = item_data.value<USBUtils::DeviceInfo>();
  }

  if (device_info.has_value())
  {
    Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID, device_info->pid);
    Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID, device_info->vid);
  }
  if (needs_refresh)
    StartBluetoothAdapterRefresh();
}

void WiimoteControllersWidget::OnBluetoothPassthroughResetPressed()
{
  const auto ios = Core::System::GetInstance().GetIOS();

  if (!ios)
  {
    ModalMessageBox::warning(
        this, tr("Warning"),
        tr("Saved Wii Remote pairings can only be reset when a Wii game is running."));
    return;
  }

  auto device = WiiUtils::GetBluetoothRealDevice();
  if (device != nullptr)
    device->TriggerSyncButtonHeldEvent();
}

void WiimoteControllersWidget::OnBluetoothPassthroughSyncPressed()
{
  const auto ios = Core::System::GetInstance().GetIOS();

  if (!ios)
  {
    ModalMessageBox::warning(this, tr("Warning"),
                             tr("A sync can only be triggered when a Wii game is running."));
    return;
  }

  auto device = WiiUtils::GetBluetoothRealDevice();
  if (device != nullptr)
    device->TriggerSyncButtonPressedEvent();
}

void WiimoteControllersWidget::OnWiimoteRefreshPressed()
{
  WiimoteReal::Refresh();
}

void WiimoteControllersWidget::OnWiimoteConfigure(size_t index)
{
  MappingWindow::Type type;
  switch (m_wiimote_boxes[index]->currentIndex())
  {
  case 0:  // None
  case 2:  // Real Wii Remote
    return;
  case 1:  // Emulated Wii Remote
    type = MappingWindow::Type::MAPPING_WIIMOTE_EMU;
    break;
  default:
    return;
  }

  MappingWindow* window = new MappingWindow(this, type, static_cast<int>(index));
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void WiimoteControllersWidget::UpdateBluetoothAdapterWidgetsEnabled(const Core::State state)
{
  const bool running = state != Core::State::Uninitialized;
  const bool running_wii = running && Core::System::GetInstance().IsWii();
  const bool enable_adapter_refresh = m_ui->passthroughRadioButton->isChecked() && !running_wii;
  const bool enable_adapter_selection =
      enable_adapter_refresh && !m_bluetooth_adapter_scan_in_progress;

  m_ui->bluetoothAdaptersLabel->setEnabled(enable_adapter_selection);
  m_ui->bluetoothAdaptersComboBox->setEnabled(enable_adapter_selection);
  m_ui->bluetoothAdaptersRefreshButton->setEnabled(enable_adapter_refresh);
}

void WiimoteControllersWidget::LoadSettings(Core::State state)
{
  for (size_t i = 0; i < m_wiimote_boxes.size(); ++i)
  {
    SignalBlocking(m_wiimote_boxes[i])
        ->setCurrentIndex(int(Config::Get(Config::GetInfoForWiimoteSource(int(i)))));
  }
  SignalBlocking(m_ui->realBalanceBoardCheckBox)
      ->setChecked(Config::Get(Config::WIIMOTE_BB_SOURCE) == WiimoteSource::Real);
  SignalBlocking(m_ui->speakerDataCheckBox)
      ->setChecked(Config::Get(Config::MAIN_WIIMOTE_ENABLE_SPEAKER));
  SignalBlocking(m_ui->controllerInterfaceCheckBox)
      ->setChecked(Config::Get(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE));
  SignalBlocking(m_ui->continuousScanningCheckBox)
      ->setChecked(Config::Get(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING));

  if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
    SignalBlocking(m_ui->passthroughRadioButton)->setChecked(true);
  else
    SignalBlocking(m_ui->emulatedRadioButton)->setChecked(true);

  // Make sure continuous scanning setting is applied.
  WiimoteReal::Initialize(::Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);

  const bool running = state != Core::State::Uninitialized;

  m_ui->emulatedRadioButton->setEnabled(!running);
  m_ui->passthroughRadioButton->setEnabled(!running);

  const bool running_gc = running && !Core::System::GetInstance().IsWii();
  const bool enable_passthrough = m_ui->passthroughRadioButton->isChecked() && !running_gc;
  const bool enable_emu_bt = !m_ui->passthroughRadioButton->isChecked() && !running_gc;
  const bool is_netplay = NetPlay::IsNetPlayRunning();
  const bool running_netplay = running && is_netplay;

  UpdateBluetoothAdapterWidgetsEnabled(state);

  m_ui->passthroughSyncButton->setEnabled(enable_passthrough);
  m_ui->passthroughResetButton->setEnabled(enable_passthrough);

  for (auto* pt_label : m_wiimote_pt_labels)
    pt_label->setEnabled(enable_passthrough);

  const int num_local_wiimotes = is_netplay ? NetPlay::NumLocalWiimotes() : 4;
  for (size_t i = 0; i < m_wiimote_boxes.size(); ++i)
  {
    m_wiimote_labels[i]->setEnabled(enable_emu_bt);
    m_wiimote_boxes[i]->setEnabled(enable_emu_bt && !running_netplay);

    const bool is_emu_wiimote = m_wiimote_boxes[i]->currentIndex() == 1;
    m_wiimote_buttons[i]->setEnabled(enable_emu_bt && is_emu_wiimote &&
                                     static_cast<int>(i) < num_local_wiimotes);
  }

  m_ui->realBalanceBoardCheckBox->setEnabled(enable_emu_bt && !running_netplay);
  m_ui->speakerDataCheckBox->setEnabled(enable_emu_bt && !running_netplay);

  const bool ciface_wiimotes = m_ui->controllerInterfaceCheckBox->isChecked();

  m_ui->wiimoteRefreshButton->setEnabled((enable_emu_bt || ciface_wiimotes) &&
                                         !m_ui->continuousScanningCheckBox->isChecked());
  m_ui->continuousScanningCheckBox->setEnabled(enable_emu_bt || ciface_wiimotes);
}

void WiimoteControllersWidget::SaveSettings()
{
  {
    Config::ConfigChangeCallbackGuard config_guard;
    Config::SetBaseOrCurrent(Config::MAIN_WIIMOTE_ENABLE_SPEAKER,
                             m_ui->speakerDataCheckBox->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE,
                             m_ui->controllerInterfaceCheckBox->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING,
                             m_ui->continuousScanningCheckBox->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED,
                             m_ui->passthroughRadioButton->isChecked());

    const WiimoteSource bb_source =
        m_ui->realBalanceBoardCheckBox->isChecked() ? WiimoteSource::Real : WiimoteSource::None;
    Config::SetBaseOrCurrent(Config::WIIMOTE_BB_SOURCE, bb_source);

    for (size_t i = 0; i < m_wiimote_boxes.size(); ++i)
    {
      const int index = m_wiimote_boxes[i]->currentIndex();
      Config::SetBaseOrCurrent(Config::GetInfoForWiimoteSource(int(i)), WiimoteSource(index));
    }
  }

  SConfig::GetInstance().SaveSettings();
}

#if defined(_WIN32)
void WiimoteControllersWidget::AsyncRefreshActionHelper(std::invocable<> auto func)
{
  m_ui->wiimoteRefreshButton->setEnabled(false);
  m_ui->refreshIndicatorLabel->show();

  auto result = std::async(std::launch::async, std::move(func));

  auto* const animation = new QTimer{this};
  connect(animation, &QTimer::timeout, this, [this, animation, result = std::move(result)] {
    // Spin the refresh indicator.
    m_ui->refreshIndicatorLabel->setPixmap(
        m_ui->refreshIndicatorLabel->pixmap().transformed(QTransform().rotate(90)));

    if (result.wait_for(std::chrono::seconds{}) != std::future_status::ready)
      return;

    // When the async task is done, re-enable the button and hide the indicator.
    animation->deleteLater();
    m_ui->refreshIndicatorLabel->hide();
    m_ui->wiimoteRefreshButton->setEnabled(true);
  });

  animation->start(250);
}

void WiimoteControllersWidget::TriggerHostWiimoteSync()
{
  AsyncRefreshActionHelper(WiimoteReal::WiimoteScannerWindows::FindAndAuthenticateWiimotes);
}

void WiimoteControllersWidget::TriggerHostWiimoteReset()
{
  AsyncRefreshActionHelper(WiimoteReal::WiimoteScannerWindows::RemoveRememberedWiimotes);
}
#endif
