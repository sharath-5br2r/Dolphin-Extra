// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/BigPicture/InGameMenu.h"

namespace VideoCommon::BigPicture
{
InGameMenu BuildLibraryMenu(const ::BigPicture::Snapshot& snapshot,
                            const ::BigPicture::NavigationText& text);
}
