// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/PassSizing.h"

#include <algorithm>
#include <cmath>

namespace VideoCommon
{
u32 ComputePassAxisSize(ScaleType type, float scale, u32 source, u32 viewport)
{
  float base = 0.0f;
  switch (type)
  {
  case ScaleType::Source:
    base = static_cast<float>(source) * scale;
    break;
  case ScaleType::Viewport:
    base = static_cast<float>(viewport) * scale;
    break;
  case ScaleType::Absolute:
    base = scale;
    break;
  }
  const long rounded = std::lround(base);
  return static_cast<u32>(std::max<long>(1, rounded));
}

std::vector<PassSize> ComputePassChainSizes(const std::vector<SlangPassConfig>& passes,
                                            u32 source_w, u32 source_h, u32 viewport_w,
                                            u32 viewport_h)
{
  std::vector<PassSize> sizes;
  sizes.reserve(passes.size());

  u32 src_w = source_w;
  u32 src_h = source_h;
  for (const SlangPassConfig& pass : passes)
  {
    const u32 out_w = ComputePassAxisSize(pass.scale_type_x, pass.scale_x, src_w, viewport_w);
    const u32 out_h = ComputePassAxisSize(pass.scale_type_y, pass.scale_y, src_h, viewport_h);
    sizes.push_back({out_w, out_h});
    // The next pass samples this pass's output as its "source".
    src_w = out_w;
    src_h = out_h;
  }
  return sizes;
}
}  // namespace VideoCommon
