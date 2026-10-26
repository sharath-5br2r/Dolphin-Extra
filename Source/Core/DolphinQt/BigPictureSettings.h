// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QObject>

#include "Common/BigPicture/Settings.h"
#include "Common/WorkQueueThread.h"
#include "UICommon/Settings/Service.h"

class InputConfig;

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
  explicit BigPictureSettings(QObject* parent, std::function<void()> changed = {},
                              std::vector<ControllerTarget> targets = {},
                              std::function<std::vector<std::string>()> devices = {});
  ~BigPictureSettings() override;
  void Open();
  void SetGame(Game game);
  void SetEnvironment(UICommon::Settings::Environment environment);
  std::shared_ptr<const Common::Settings::Snapshot> ReadSnapshot();
  std::shared_ptr<const BigPicture::SettingsMenuSnapshot> ReadMenu();
  Common::Settings::Result Apply(const Common::Settings::Request& request);
  Common::Settings::Status Execute(const BigPicture::SettingsRequest& request);

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
    Presets
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
  void Back();
  UICommon::Settings::Service m_service;
  UICommon::Settings::Environment m_environment;
  Game m_game;
  UICommon::Settings::Context m_game_context;
  Page m_page = Page::Home;
  std::string m_category;
  std::string m_edit;
  std::string m_controller;
  std::string m_preset_folder;
  u64 m_revision = 1;
  u64 m_settings_revision = 0;
  std::vector<std::string> m_devices;
  std::vector<ControllerTarget> m_targets;
  std::function<std::vector<std::string>()> m_input_devices;
  std::function<void()> m_changed;
  std::vector<std::string> m_presets;
  bool m_presets_loading = false;
  Common::WorkQueueThread<bool> m_worker;
};
