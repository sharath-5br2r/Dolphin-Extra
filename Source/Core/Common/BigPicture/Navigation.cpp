// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/BigPicture/Navigation.h"

#include <algorithm>
#include <cmath>

namespace BigPicture
{
std::vector<NavTransition> NavigationFilter::Update(const NavValues& values, bool connected,
                                                    bool enabled, std::uint64_t epoch)
{
  std::vector<NavTransition> transitions;
  const bool reset =
      !m_epoch || *m_epoch != epoch || m_connected != connected || m_enabled != enabled;
  for (std::size_t i = 0; i < m_down.size(); ++i)
  {
    if (reset)
    {
      if (m_down[i])
        transitions.push_back({static_cast<NavButton>(i), false});
      m_down[i] = false;
      m_require_release[i] = true;
    }
    const double value = std::isfinite(values[i]) ? values[i] : 0.0;
    if (value <= 0.3)
      m_require_release[i] = false;
    const bool down =
        connected && enabled && !m_require_release[i] && value > (m_down[i] ? 0.3 : 0.55);
    if (down != m_down[i])
    {
      transitions.push_back({static_cast<NavButton>(i), down});
      m_down[i] = down;
    }
  }
  m_epoch = epoch;
  m_connected = connected;
  m_enabled = enabled;
  return transitions;
}

NavigationOptions SanitizeNavigationOptions(NavigationOptions options)
{
  options.scale = std::isfinite(options.scale) ? std::clamp(options.scale, 0.75f, 1.5f) : 1.0f;
  options.safe_area =
      std::isfinite(options.safe_area) ? std::clamp(options.safe_area, 0.0f, 0.15f) : 0.05f;
  return options;
}
}  // namespace BigPicture
