// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Layer.h"

namespace BigPicture
{
// Qt-owner snapshot of effective graphics settings. Library, input and audio changes must not
// recreate the idle surface and discard in-flight navigation/text events.
Config::LayerMap ReadGraphicsConfig();
// Only native-surface requirements determine whether an existing drawing window is reusable.
Config::LayerMap ReadRenderSurfaceConfig();
}  // namespace BigPicture
