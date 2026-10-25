// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace VideoCommon
{
// Decision for how to reduce an internally-upscaled source frame down to the game's native
// resolution before it is fed to a RetroArch (slang) filter chain. CRT-type chains derive their
// scanline count and every source-relative pass size from the source texture, so feeding the
// upscaled frame beats the internal-resolution multiplier into horizontal moire and runs every
// pass at the upscaled pixel count for no visual gain. Downscaling to native first mirrors what
// RetroArch does implicitly by consuming a console's own output.
struct SlangSourceDownscalePlan
{
  // True when a native-sized source texture should be materialized. PCSX2 performs this
  // normalization even at 1x so cropped regions and selected texture layers are consistently
  // presented to librashader.
  bool normalize = false;
  // True to box-average the whole factor x factor footprint (real SSAA); false to bilinear-resample
  // a single tap. A box filter only fits an exact, equal integer factor on both axes.
  bool box_filter = false;
  // The integer downscale factor when box_filter is true; otherwise unused (0).
  u32 factor = 0;
};

// Chooses the source-normalization strategy from the selected source region and native frame
// dimensions. native_w/native_h of 0 mean "native size unknown" and skip normalization. The box
// filter is selected only for an exact, equal integer reduction on both axes (2x/3x/4x/...);
// native-size copies, upscales, fractional reductions, and mismatched axis factors use bilinear
// sampling.
SlangSourceDownscalePlan PlanSlangSourceDownscale(u32 src_w, u32 src_h, u32 native_w, u32 native_h);
}  // namespace VideoCommon
