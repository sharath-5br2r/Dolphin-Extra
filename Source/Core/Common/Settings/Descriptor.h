// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Config/ConfigInfo.h"

namespace Common::Settings
{
using Value = std::variant<bool, int, u32, float, std::string, u64, s64>;
template <typename T>
using EnumStorage = std::conditional_t<std::is_signed_v<std::underlying_type_t<T>>,
                                       std::conditional_t<(sizeof(T) <= sizeof(int)), int, s64>,
                                       std::conditional_t<(sizeof(T) <= sizeof(u32)), u32, u64>>;
enum class Control
{
  Toggle,
  Number,
  Choice,
  Text,
  Path
};
enum class ApplyPolicy
{
  Live,
  IdleOnly,
  NextBoot
};
enum class EditTarget
{
  Effective,
  Base,
  Game
};
enum class Operation
{
  Set,
  SetDisplay,
  ResetDefault,
  ClearOverride
};
enum class Status
{
  Success,
  UnknownSetting,
  InvalidValue,
  UnsupportedContext,
  Unavailable,
  Stale,
  WrongThread
};

struct Choice
{
  Value value;
  std::string label;
  bool operator==(const Choice&) const = default;
};
struct Range
{
  double minimum = 0;
  double maximum = 0;
  double step = 1;
  // display = stored * scale + offset (e.g. an overclock fraction displayed as a percent).
  double scale = 1;
  double offset = 0;
  std::string unit;
  bool operator==(const Range&) const = default;
};
struct Descriptor
{
  std::string id;
  Config::Location location;
  std::string category;
  int order = 0;
  std::string label;
  std::string help;
  Control control = Control::Toggle;
  Value default_value = false;
  bool per_game = false;
  bool reverse = false;
  bool netplay_safe = true;
  bool hardcore_safe = true;
  ApplyPolicy policy = ApplyPolicy::Live;
  std::optional<Range> range;
  std::vector<Choice> choices;
  // Slider ticks are exact stored values, in display order; they need not be monotonic.
  std::vector<Value> ticks;
};
struct State
{
  Descriptor descriptor;
  Value value;
  Value inherited_value;
  Value display_value;
  bool overridden = false;
  bool value_available = true;
  std::optional<Config::LayerType> source_layer;
  bool enabled = true;
  std::string disabled_reason;
};
struct Snapshot
{
  u64 revision = 0;
  EditTarget target = EditTarget::Effective;
  std::vector<State> settings;
};
struct Request
{
  u64 revision = 0;
  std::string id;
  Operation operation = Operation::Set;
  Value value = false;
  double display_value = 0;
};
struct Result
{
  Status status = Status::Success;
  ApplyPolicy policy = ApplyPolicy::Live;
};
}  // namespace Common::Settings
