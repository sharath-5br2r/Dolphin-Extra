// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/Common.h"

namespace BigPicture
{
struct ThemeChoice
{
  const char* id;
  const char* label;
};
// Stable configuration values shared by both frontends. Unknown values remain stored;
// renderers fall back to Dark until a newer version understands them.
inline constexpr std::array THEME_CHOICES = {
    ThemeChoice{"Dark", _trans("Dark")},
    ThemeChoice{"Light", _trans("Light")},
    ThemeChoice{"AMOLED", _trans("AMOLED")},
    ThemeChoice{"CobaltSky", _trans("Cobalt Sky")},
    ThemeChoice{"GreyMatter", _trans("Grey Matter")},
    ThemeChoice{"UntouchedLagoon", _trans("Untouched Lagoon")},
    ThemeChoice{"BabyPastel", _trans("Baby Pastel")},
    ThemeChoice{"PizzaBrown", _trans("Pizza Brown")},
    ThemeChoice{"ScarletDevil", _trans("Scarlet Devil")},
    ThemeChoice{"VioletAngel", _trans("Violet Angel")},
    ThemeChoice{"DolphinBlue", _trans("Dolphin Blue")},
};
}  // namespace BigPicture
