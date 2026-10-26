// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

namespace UICommon::Settings::Logic
{
template <typename T>
T ReadInherited(const Config::Info<T>& setting, const Config::Layer* fallback)
{
  return fallback && fallback->Exists(setting.GetLocation()) ? fallback->Get(setting) :
                                                               Config::GetBase(setting);
}

template <typename T>
T Read(const Config::Info<T>& setting, const Config::Layer* layer,
       const Config::Layer* fallback = nullptr)
{
  if (!layer)
    return Config::Get(setting);
  return layer->Exists(setting.GetLocation()) ? layer->Get(setting) :
                                                ReadInherited(setting, fallback);
}

template <typename T>
void Write(const Config::Info<T>& setting, const Config::Location& location, Config::Layer* layer,
           const T& value)
{
  if (layer)
  {
    if (layer->Set(location, value))
      Config::OnConfigChanged();
  }
  else
    Config::SetBaseOrCurrent(setting, value);
}

inline bool IsLocal(const Config::Location& location, const Config::Layer* layer)
{
  return layer ? layer->Exists(location) :
                 Config::GetActiveLayerForConfig(location) != Config::LayerType::Base;
}

inline void Clear(const Config::Location& location, Config::Layer* layer)
{
  if (layer && layer->DeleteKey(location))
    Config::OnConfigChanged();
}
}  // namespace UICommon::Settings::Logic
