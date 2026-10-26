// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Common/Settings/Descriptor.h"

namespace BigPicture
{
enum class SettingsOperation
{
  Back,
  Category,
  Global,
  Game,
  Edit,
  Controllers,
  Controller,
  Devices,
  Device,
  Profiles,
  Profile,
  ClearProfile,
  Refresh,
  PresetFolder,
  Count
};
struct SettingsRequest
{
  std::uint64_t revision = 0;
  SettingsOperation operation = SettingsOperation::Back;
  std::string value;
};
struct SettingsRow
{
  std::string id;
  std::string label;
  std::string detail;
  bool enabled = true;
  std::optional<SettingsRequest> navigation;
  std::optional<Common::Settings::Request> edit;
  bool active = false;
};
// Translated, value-only menu data. Profile/controller objects and config layers stay on the host.
struct SettingsMenuSnapshot
{
  std::uint64_t revision = 0;
  std::string id;
  std::string title;
  std::string description;
  std::vector<SettingsRow> rows;
  std::vector<SettingsRow> toolbar;
  std::optional<SettingsRequest> back;
  std::string default_focus;
  bool preferences = false;
};
}  // namespace BigPicture
