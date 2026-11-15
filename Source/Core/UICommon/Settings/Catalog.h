// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

#include "Common/Settings/Descriptor.h"
#include "UICommon/Settings/ConfigLayerLogic.h"

namespace UICommon::Settings
{
using Common::Settings::ApplyPolicy;
using Common::Settings::Choice;
using Common::Settings::Control;
using Common::Settings::Descriptor;
using Common::Settings::EditTarget;
using Common::Settings::EnumStorage;
using Common::Settings::Operation;
using Common::Settings::Range;
using Common::Settings::Request;
using Common::Settings::Result;
using Common::Settings::Snapshot;
using Common::Settings::State;
using Common::Settings::Status;
using Common::Settings::Value;

// The service retains these layers for the edit session. No layer or operation enters a snapshot.
struct Context
{
  EditTarget target = EditTarget::Effective;
  std::shared_ptr<Config::Layer> layer;
  std::shared_ptr<Config::Layer> fallback;
};

struct Environment
{
  bool emulating = false;
  bool transitioning = false;
  bool netplay = false;
  bool hardcore = false;
  bool post_processing = false;
  bool movie = false;
  int max_efb_scale = 12;
  std::vector<Choice> graphics_backends;
  std::vector<Choice> graphics_adapters;
  std::vector<Choice> audio_backends;
  std::vector<Choice> wasapi_devices;
  std::vector<Choice> languages;
  std::vector<Choice> anti_aliasing;
  std::vector<Choice> cpu_engines;
  bool gpu_texture_decoding = false;
  bool bounding_box = false;
  bool ssaa = false;
  bool hdr = false;
  bool stereo = false;
  bool operator==(const Environment&) const = default;
};

struct Entry
{
  Descriptor descriptor;
  std::function<Value(const Context&)> read;
  std::function<Value(const Context&)> inherited;
  std::function<void(const Context&, const Value&)> write;
  std::function<std::vector<Choice>(const Environment&)> options;
  std::function<std::string(const Environment&, const Context&)> unavailable;
};

template <typename T>
Value ToValue(const T& value)
{
  if constexpr (std::is_enum_v<T>)
    return static_cast<EnumStorage<T>>(value);
  else
    return value;
}

template <typename T>
T FromValue(const Value& value)
{
  if constexpr (std::is_enum_v<T>)
    return static_cast<T>(std::get<EnumStorage<T>>(value));
  else
    return std::get<T>(value);
}

template <typename T>
Entry MakeEntry(const Config::Info<T>& setting, Descriptor descriptor)
{
  descriptor.location = setting.GetLocation();
  descriptor.default_value = ToValue(setting.GetDefaultValue());
  Entry entry;
  entry.descriptor = std::move(descriptor);
  entry.read = [setting](const Context& context) {
    return ToValue(context.target == EditTarget::Base ?
                       Config::GetBase(setting) :
                       Logic::Read(setting, context.layer.get(), context.fallback.get()));
  };
  entry.inherited = [setting](const Context& context) {
    return ToValue(Logic::ReadInherited(setting, context.fallback.get()));
  };
  entry.write = [setting](const Context& context, const Value& value) {
    const T typed = FromValue<T>(value);
    if (context.target == EditTarget::Base)
      Config::SetBase(setting, typed);
    else
      Logic::Write(setting, setting.GetLocation(), context.layer.get(), typed);
  };
  return entry;
}

// Immutable after construction. Descriptors and typed operations are explicitly bootstrapped,
// never inferred from the set of Qt panes the user happened to open.
class Catalog
{
public:
  explicit Catalog(std::vector<Entry> entries);
  const Entry* Find(std::string_view id) const;
  const Entry* Find(const Config::Location& location) const;
  std::span<const Entry> Entries() const { return m_entries; }

private:
  std::vector<Entry> m_entries;
};

const Catalog& GetCatalog();
void AppendApplicationSettings(std::vector<Entry>& entries);
}  // namespace UICommon::Settings
