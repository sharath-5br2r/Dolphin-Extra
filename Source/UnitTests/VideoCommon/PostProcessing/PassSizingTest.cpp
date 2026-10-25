// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/PassSizing.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"

using namespace VideoCommon;

TEST(PassSizing, Source)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Source, 1.0f, 640, 1920), 640u);
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Source, 0.5f, 640, 1920), 320u);
}
TEST(PassSizing, Viewport)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Viewport, 1.0f, 640, 1920), 1920u);
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Viewport, 0.0625f, 640, 1920), 120u);
}
TEST(PassSizing, Absolute)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Absolute, 320.0f, 640, 1920), 320u);
}
TEST(PassSizing, ClampsToOne)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Absolute, 0.0f, 640, 1920), 1u);
}

// The physical (content) chain must scale with the internal-resolution-scaled source for ANY
// multiplier, odd or even -- there is no odd/even special-casing. A source-scaled pass at Nx
// produces native*N, and the multiply is exact for odd N too.
TEST(PassSizing, PhysicalChainScalesForOddAndEvenMultipliers)
{
  // crt-royale-fast's first pass: source x1.
  std::vector<SlangPassConfig> passes(1);
  passes[0].scale_type_x = ScaleType::Source;
  passes[0].scale_type_y = ScaleType::Source;
  passes[0].scale_x = passes[0].scale_y = 1.0f;

  const u32 native_w = 640, native_h = 528;
  for (u32 mult : {1u, 2u, 3u, 4u, 5u})
  {
    const std::vector<PassSize> sizes = ComputePassChainSizes(
        passes, native_w * mult, native_h * mult, /*viewport_w=*/1920, /*viewport_h=*/1080);
    ASSERT_EQ(sizes.size(), 1u);
    EXPECT_EQ(sizes[0].width, native_w * mult) << "mult=" << mult;
    EXPECT_EQ(sizes[0].height, native_h * mult) << "mult=" << mult;
  }
}

// The chain must seed pass 0's source with the GAME's real resolution (source_w/h), not the
// viewport. Each subsequent pass's source is the previous pass's output. Getting this wrong is
// what made crt-royale compute scanline pitch against the viewport (invisible/misoriented
// scanlines).
TEST(PassSizing, ChainSeedsFromSourceThenPreviousOutput)
{
  std::vector<SlangPassConfig> passes(3);
  // Pass 0: source x1 -> equals the game resolution.
  passes[0].scale_type_x = ScaleType::Source;
  passes[0].scale_type_y = ScaleType::Source;
  passes[0].scale_x = passes[0].scale_y = 1.0f;
  // Pass 1: source x0.5 of pass 0's output.
  passes[1].scale_type_x = ScaleType::Source;
  passes[1].scale_type_y = ScaleType::Source;
  passes[1].scale_x = passes[1].scale_y = 0.5f;
  // Pass 2 (final): viewport x1.
  passes[2].scale_type_x = ScaleType::Viewport;
  passes[2].scale_type_y = ScaleType::Viewport;
  passes[2].scale_x = passes[2].scale_y = 1.0f;

  const std::vector<PassSize> sizes =
      ComputePassChainSizes(passes, /*source_w=*/640, /*source_h=*/528,
                            /*viewport_w=*/1920, /*viewport_h=*/1080);
  ASSERT_EQ(sizes.size(), 3u);
  EXPECT_EQ(sizes[0].width, 640u);   // source, not viewport
  EXPECT_EQ(sizes[0].height, 528u);
  EXPECT_EQ(sizes[1].width, 320u);   // half of pass 0's output
  EXPECT_EQ(sizes[1].height, 264u);
  EXPECT_EQ(sizes[2].width, 1920u);  // viewport
  EXPECT_EQ(sizes[2].height, 1080u);
}
