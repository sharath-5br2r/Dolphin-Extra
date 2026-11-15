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
  TextStart,
  TextKey,
  TextErase,
  TextClear,
  TextDone,
  BrowseStart,
  BrowseEnter,
  BrowseChoose,
  Mapping,
  MapBind,
  MapOptions,
  MapClear,
  MapDefaults,
  MapDefaultsConfirm,
  MapExtensions,
  MapExtension,
  MapSettingEdit,
  MapSettingStep,
  MapSettingToggle,
  MapSettingReset,
  SaveProfile,
  MapSettingSet,
  Count
};
struct SettingsRequest
{
  std::uint64_t revision = 0;
  SettingsOperation operation = SettingsOperation::Back;
  std::string value;
};
// One slider change: a catalog value edit, or a host operation such as controller tuning.
struct SettingsAction
{
  std::optional<SettingsRequest> navigation;
  std::optional<Common::Settings::Request> edit;
};
struct SettingsSlider
{
  double minimum = 0;
  double maximum = 0;
  double step = 1;
  double value = 0;
  std::string minimum_label;
  std::string maximum_label;
  // Each is absent where it would not change the value (at the ends of the range).
  std::optional<SettingsAction> decrease;
  std::optional<SettingsAction> increase;
  std::optional<SettingsAction> decrease_large;
  std::optional<SettingsAction> increase_large;
  // Template for an arbitrary value, such as a pointer position; the menu fills in the value.
  SettingsAction set;
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
  std::string value;
  std::string help;
  std::string section;
  std::optional<bool> toggle;
  bool value_enabled = true;
  std::string category;
  // Secondary request for the row's options (DPad Right / F2), alongside its navigation.
  std::optional<SettingsRequest> options;
  std::optional<SettingsSlider> slider;
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
  bool category_page = false;
  bool text_input = false;
  std::string draft;
  // Input detection owns the controller: the menu must not act on any input until it ends.
  bool capturing = false;
};
}  // namespace BigPicture
