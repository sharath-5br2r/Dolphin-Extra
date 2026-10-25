// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"

namespace VideoCommon
{
struct PassSizeInputs
{
  u32 source_width;    // previous pass output (or pipeline input for pass 0)
  u32 source_height;
  u32 viewport_width;  // final on-screen target
  u32 viewport_height;
};

struct PassSize
{
  u32 width;
  u32 height;
};

// Computes one axis. scale_type Source: round(source * scale); Viewport: round(viewport * scale);
// Absolute: round(scale). Result is clamped to >= 1.
u32 ComputePassAxisSize(ScaleType type, float scale, u32 source, u32 viewport);

// Computes every pass's output size for the chain. Pass 0's "source" is the game's real input
// resolution (source_w/h) -- NOT the viewport; RetroArch shaders derive scanline/mask geometry
// from SourceSize, so this must be the emulated framebuffer size. Each later pass's "source" is
// the previous pass's output. `viewport_w/h` is the final on-screen target.
std::vector<PassSize> ComputePassChainSizes(const std::vector<SlangPassConfig>& passes,
                                            u32 source_w, u32 source_h, u32 viewport_w,
                                            u32 viewport_h);
}  // namespace VideoCommon
