// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <span>
#include <vector>

#include <QString>

#include "Common/Config/ConfigInfo.h"

namespace ConfigWidget
{
enum class SettingKind
{
  Bool,
  Int,
  U32,
  Float,
  String,
  Path,
  Choice,
  Complex,
};

// What Bind() was handed, or could read off the widget it was handed. Nothing here is a new
// argument callers must supply.
struct SettingEntry
{
  Config::Location location;
  SettingKind kind = SettingKind::Bool;
  // True if any binding for this setting was given a Config::Layer, i.e. it is editable per game.
  bool per_game = false;
  QString title;
  QString description;
  // Choice kinds only. Empty for Complex, which is populated after binding.
  std::vector<QString> choices;
  // Numeric kinds only. Doubles so one field covers int, u32 and float ranges.
  std::optional<double> minimum;
  std::optional<double> maximum;
  std::optional<double> step;
};

// Every Bind() records here, so a settings interface can be driven from the desktop panes' own
// definitions instead of re-authoring all of them. Its consumer is the fullscreen-OSD follow-up
// project; nothing in DolphinQt reads it yet.
class ConfigSettingRegistry
{
public:
  static ConfigSettingRegistry& Instance();

  // Idempotent: merges into any existing entry for the same location, filling empty fields and
  // leaving populated ones alone. The same setting is bound in both a global and a per-game pane,
  // and panes reopen.
  void Record(SettingEntry entry);
  void SetText(const Config::Location& location, QString title, QString description);

  // Returns a pointer valid until the next Record() call.
  const SettingEntry* Find(const Config::Location& location) const;
  // Returns a span valid until the next Record() call.
  std::span<const SettingEntry> Entries() const { return m_entries; }

  void ClearForTesting();

private:
  ConfigSettingRegistry() = default;

  std::vector<SettingEntry> m_entries;
  std::map<Config::Location, size_t> m_index;
};
}  // namespace ConfigWidget
