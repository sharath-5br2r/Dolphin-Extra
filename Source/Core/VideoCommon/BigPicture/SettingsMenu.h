// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/BigPicture/InGameMenu.h"

namespace VideoCommon::BigPicture
{
InGameMenu BuildSettingsMenu(const ::BigPicture::Snapshot& snapshot,
                             const ::BigPicture::NavigationText& text,
                             ::BigPicture::NavigationOptions options);
}
