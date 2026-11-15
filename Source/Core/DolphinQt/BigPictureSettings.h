// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "Common/BigPicture/Settings.h"
#include "Common/WorkQueueThread.h"
#include "DolphinQt/BigPictureFiles.h"
#include "UICommon/Settings/Service.h"

class InputConfig;
class QTimer;

namespace ciface::Core
{
class Device;
class DeviceContainer;
class InputDetector;
}  // namespace ciface::Core

namespace ControllerEmu
{
class Attachments;
class Control;
class EmulatedController;
class NumericSettingBase;
}  // namespace ControllerEmu

// Qt-owner settings workflow. The renderer receives translated rows and value requests only.
class BigPictureSettings final : public QObject
{
  Q_OBJECT
public:
  struct Game
  {
    std::string id;
    u16 revision = 0;
    std::string title;
    bool active = false;
    bool operator==(const Game&) const = default;
  };
  struct ControllerTarget
  {
    std::string id;
    std::string label;
    InputConfig* config = nullptr;
    int index = 0;
  };
  // Input detection reads detection_source; it defaults to the global controller interface.
  explicit BigPictureSettings(QObject* parent, std::function<void()> changed = {},
                              std::vector<ControllerTarget> targets = {},
                              std::function<std::vector<std::string>()> devices = {},
                              const ciface::Core::DeviceContainer* detection_source = nullptr);
  ~BigPictureSettings() override;
  bool Open(bool game = false);
  void SetGame(Game game);
  void SetEnvironment(UICommon::Settings::Environment environment);
  std::shared_ptr<const Common::Settings::Snapshot> ReadSnapshot();
  std::shared_ptr<const BigPicture::SettingsMenuSnapshot> ReadMenu();
  Common::Settings::Result Apply(const Common::Settings::Request& request);
  Common::Settings::Status Execute(const BigPicture::SettingsRequest& request);
  // Stops waiting for a binding input, e.g. once the settings screen is no longer shown.
  // Does not notify; the next menu read reflects it.
  void CancelCapture();

signals:
  void SettingApplied(const QString& id);

private:
  enum class Page
  {
    Home,
    Category,
    Edit,
    Controllers,
    Controller,
    Devices,
    Profiles,
    Presets,
    Text,
    Browse,
    Mapping,
    MapControl,
    MapDefaults,
    Capture,
    MapExtension,
    MapSetting,
    ProfileName
  };
  struct MappingControl
  {
    // "<group name>/<control name>": stable across translations and binding changes.
    std::string key;
    std::string group;
    std::string label;
    ControllerEmu::Control* control = nullptr;
  };
  struct MappingSetting
  {
    // "set:<group key>/<ini name>".
    std::string key;
    std::string group;
    std::string label;
    ControllerEmu::NumericSettingBase* setting = nullptr;
  };
  struct BrowseJob
  {
    u64 revision = 0;
    std::string path;
    bool folders_only = false;
  };
  struct Profile
  {
    std::string path;
    std::string name;
    std::string game_value;
  };
  UICommon::Settings::Context GameContext();
  std::vector<ControllerTarget> Targets() const;
  std::vector<std::string> Devices() const;
  const ControllerTarget* Target(const std::vector<ControllerTarget>& targets) const;
  std::vector<Profile> Profiles(const ControllerTarget& target);
  std::string ControllerUnavailable() const;
  void ScanPresets();
  void Browse(std::string directory);
  // Caller holds the controller's state lock.
  std::vector<MappingControl> MappingControls(ControllerEmu::EmulatedController& controller) const;
  std::vector<MappingSetting> MappingSettings(ControllerEmu::EmulatedController& controller) const;
  bool IsTextPage() const { return m_page == Page::Text || m_page == Page::ProfileName; }
  // Returns a translated error, or an empty string once the profile is written.
  std::string SaveProfile(const ControllerTarget& target, const std::string& name);
  bool StartCapture(const ControllerTarget& target, const std::string& key);
  void UpdateCapture();
  std::string BrowseStartDirectory(const Common::Settings::State& state) const;
  void Back();
  std::string MenuId(bool game) const;
  UICommon::Settings::Service m_service;
  UICommon::Settings::Environment m_environment;
  Game m_game;
  UICommon::Settings::Context m_game_context;
  Page m_page = Page::Home;
  std::string m_category = "Interface";
  std::string m_edit;
  std::string m_controller;
  std::string m_preset_folder;
  std::string m_draft;
  std::string m_text_error;
  u64 m_revision = 1;
  u64 m_settings_revision = 0;
  // The service reuses its snapshot until a revision change, so translate each one once.
  std::shared_ptr<const Common::Settings::Snapshot> m_source_snapshot;
  std::shared_ptr<const Common::Settings::Snapshot> m_translated_snapshot;
  std::vector<std::string> m_devices;
  std::vector<ControllerTarget> m_targets;
  std::function<std::vector<std::string>()> m_input_devices;
  std::function<void()> m_changed;
  std::vector<std::string> m_presets;
  bool m_presets_loading = false;
  std::string m_browse_directory;
  BigPictureFiles::Listing m_browse_listing;
  bool m_browse_loading = false;
  bool m_browse_folders = false;
  u64 m_browse_revision = 0;
  // Read by the listing thread to skip superseded folders and stop at destruction.
  std::atomic<u64> m_browse_latest = 0;
  std::atomic<bool> m_browse_cancelled = false;
  std::string m_map_control;
  std::string m_map_setting;
  std::string m_map_message;
  std::string m_controller_message;
  const ciface::Core::DeviceContainer* m_detection_source;
  // Set while a binding capture is open; detection starts once its inputs are released.
  std::shared_ptr<ciface::Core::Device> m_capture_device;
  std::chrono::steady_clock::time_point m_capture_release_deadline;
  std::unique_ptr<ciface::Core::InputDetector> m_capture;
  QTimer* m_capture_timer = nullptr;
  Common::WorkQueueThread<bool> m_worker;
  Common::WorkQueueThread<BrowseJob> m_browse_worker;
};
