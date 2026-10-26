// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <unordered_map>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "UICommon/Settings/ConfigLayerLogic.h"

// Qt-free half of the config binder: everything about deciding *which* config layer a bound
// widget reads from and writes to. Kept free of Qt so it can be tested in the Qt-free `tests`
// binary. Mirrors the semantics of ConfigControl::ReadValue / SaveValue / IsConfigLocal.
namespace ConfigWidget::Logic
{
// Associates a per-game edit layer with the shipped game-INI layer it inherits before the user's
// global/base value. The registry is process-local and GUI-thread-owned; the Qt-free tests use it
// directly as well.
inline std::unordered_map<Config::Layer*, Config::Layer*>& FallbackLayers()
{
  static std::unordered_map<Config::Layer*, Config::Layer*> layers;
  return layers;
}

inline void SetFallbackLayer(Config::Layer* layer, Config::Layer* fallback)
{
  if (layer == nullptr)
    return;
  if (fallback == nullptr)
    FallbackLayers().erase(layer);
  else
    FallbackLayers().insert_or_assign(layer, fallback);
}

inline Config::Layer* GetFallbackLayer(Config::Layer* layer)
{
  const auto it = FallbackLayers().find(layer);
  return it == FallbackLayers().end() ? nullptr : it->second;
}

template <typename T>
T ReadInheritedValue(const Config::Info<T>& setting, Config::Layer* layer)
{
  return UICommon::Settings::Logic::ReadInherited(setting, GetFallbackLayer(layer));
}

// A null `layer` means the binding edits global config. A non-null `layer` means it edits a
// per-game layer, falling back to its optional shipped game-INI layer and then the base layer.
template <typename T>
T ReadValue(const Config::Info<T>& setting, Config::Layer* layer)
{
  return UICommon::Settings::Logic::Read(setting, layer, GetFallbackLayer(layer));
}

template <typename T>
void WriteValue(const Config::Info<T>& setting, const Config::Location& location,
                Config::Layer* layer, const T& value)
{
  UICommon::Settings::Logic::Write(setting, location, layer, value);
}

// Whether the value shown comes from somewhere other than the base layer. Drives the bold font
// that marks an overridden setting.
inline bool IsLocal(const Config::Location& location, Config::Layer* layer)
{
  return UICommon::Settings::Logic::IsLocal(location, layer);
}

// Drops a per-game override so the setting reverts to the global value. A no-op for global
// bindings, which have no override to drop.
inline void ClearLocal(const Config::Location& location, Config::Layer* layer)
{
  UICommon::Settings::Logic::Clear(location, layer);
}
}  // namespace ConfigWidget::Logic
