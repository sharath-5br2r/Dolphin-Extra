// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>
#include <QString>

namespace ControllerEmu
{
class EmulatedController;
}

class InputConfig;
class MappingButton;

namespace Ui
{
class MappingWindow;
}

class QComboBox;
class QDialogButtonBox;
class QEvent;
class QPushButton;
class QTabWidget;
class QToolButton;
class QWidget;

class MappingWindow final : public QDialog
{
  Q_OBJECT
public:
  enum class Type
  {
    // GameCube
    MAPPING_GC_BONGOS,
    MAPPING_GC_DANCEMAT,
    MAPPING_GC_GBA,
    MAPPING_GC_KEYBOARD,
    MAPPING_GCPAD,
    MAPPING_GC_STEERINGWHEEL,
    MAPPING_GC_MICROPHONE,
    // Wii
    MAPPING_WIIMOTE_EMU,
    // Hotkeys
    MAPPING_HOTKEYS,
    MAPPING_BIG_PICTURE,
    // Freelook
    MAPPING_FREELOOK,
    // Triforce
    MAPPING_AM_BASEBOARD,
  };

  explicit MappingWindow(QWidget* parent, Type type, int port_num);
  ~MappingWindow() override;

  int GetPort() const;
  ControllerEmu::EmulatedController* GetController() const;
  bool IsCreateOtherDeviceMappingsEnabled() const;
  bool IsWaitForAlternateMappingsEnabled() const;
  bool IsIterativeMappingEnabled() const;
  void ShowExtensionMotionTabs(bool show);
  void ActivateExtensionTab();

signals:
  // Emitted when config has changed so widgets can update to reflect the change.
  void ConfigChanged();
  // Emitted at INDICATOR_UPDATE_FREQ Hz for real-time indicators to be updated.
  void Update();
  void Save();

  void UnQueueInputDetection(MappingButton*);
  void QueueInputDetection(MappingButton*);
  void CancelMapping();

private:
  void SetMappingType(Type type);
  void CreateWidgets();
  void ConnectWidgets();

  QWidget* AddWidget(const QString& name, QWidget* widget);

  void RefreshDevices();

  void OnSelectProfile(int index);
  void OnProfileTextChanged(const QString& text);
  void OnDeleteProfilePressed();
  void OnLoadProfilePressed();
  void OnSaveProfilePressed();
  void OnOpenProfileFolder();
  void UpdateProfileIndex();
  void UpdateProfileButtonState();
  void PopulateProfileSelection();
  void UpdateDeviceList();

  void OnDefaultFieldsPressed();
  void OnClearFieldsPressed();
  void OnSelectDevice(int index);

  ControllerEmu::EmulatedController* m_controller = nullptr;

  std::unique_ptr<Ui::MappingWindow> m_ui;
  QDialogButtonBox* m_button_box;

  // Devices
  QComboBox* m_devices_combo;
  QAction* m_other_device_mappings;
  QAction* m_wait_for_alternate_mappings;
  QAction* m_iterative_mapping;

  // Profiles
  QComboBox* m_profiles_combo;
  QPushButton* m_profiles_load;
  QPushButton* m_profiles_save;
  QToolButton* m_profile_other_actions;
  QAction* m_profiles_delete;
  QAction* m_profiles_open_folder;

  // Reset
  QPushButton* m_reset_default;
  QPushButton* m_reset_clear;

  QTabWidget* m_tab_widget;
  QWidget* m_extension_motion_input_tab;
  QWidget* m_extension_motion_simulation_tab;
  const QString EXTENSION_MOTION_INPUT_TAB_NAME = tr("Extension Motion Input");
  const QString EXTENSION_MOTION_SIMULATION_TAB_NAME = tr("Extension Motion Simulation");

  Type m_mapping_type;
  const int m_port;
  InputConfig* m_config;
};
