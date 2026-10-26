// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureGraphicsConfig.h"

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"

namespace BigPicture
{
Config::LayerMap ReadRenderSurfaceConfig()
{
  Config::LayerMap result;
  for (const auto& location :
       {Config::MAIN_GFX_BACKEND.GetLocation(), Config::GFX_STEREO_MODE.GetLocation()})
    result[location] = Config::GetAsString(location);
  return result;
}

Config::LayerMap ReadGraphicsConfig()
{
  Config::LayerMap result;
  for (const auto type : Config::SEARCH_ORDER)
  {
    const auto layer = Config::GetLayer(type);
    if (!layer)
      continue;
    for (const auto& [location, value] : layer->GetLayerMap())
      if (location.system == Config::System::GFX && value)
        result.try_emplace(location, value);
  }
  for (const auto& location :
       {Config::MAIN_GFX_BACKEND.GetLocation(), Config::MAIN_EMULATION_SPEED.GetLocation()})
    result[location] = Config::GetAsString(location);
  return result;
}
}  // namespace BigPicture
