// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PresentationScaling.h"

#include <algorithm>
#include <cmath>

namespace VideoCommon
{
std::pair<int, int> ApplyIntegerScaling(int target_width, int target_height, int source_width,
                                        int source_height)
{
  if (target_width <= 0 || target_height <= 0 || source_width <= 0 || source_height <= 0)
    return {target_width, target_height};

  const double source_aspect = static_cast<double>(source_width) / source_height;
  const double scale = source_aspect >= 1.0 ?
                           static_cast<double>(target_width) / source_width :
                           static_cast<double>(target_height) / source_height;
  if (scale <= 1.0)
    return {target_width, target_height};

  const double adjustment = std::floor(scale) / scale;
  return {std::max(1, static_cast<int>(std::round(target_width * adjustment))),
          std::max(1, static_cast<int>(std::round(target_height * adjustment)))};
}
}  // namespace VideoCommon
