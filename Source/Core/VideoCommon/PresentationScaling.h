// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

namespace VideoCommon
{
// Reduces an aspect-corrected presentation size so its primary source axis is a whole-number
// multiple of the rendered source. The other axis is reduced by the same ratio, preserving the
// display aspect ratio for sources whose pixels are not square.
std::pair<int, int> ApplyIntegerScaling(int target_width, int target_height, int source_width,
                                        int source_height);
}  // namespace VideoCommon
