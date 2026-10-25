// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/MathUtil.h"
#include "VideoCommon/PostProcessing/ChainOutputPolicy.h"

using VideoCommon::ShouldRenderChainDirectly;

// The chain may render straight into the backbuffer only when the draw rect IS the backbuffer:
// then librashader's OutputSize equals the drawn extent either way, so skipping the intermediate
// target cannot change a pixel.
TEST(ChainOutputPolicy, FullBackbufferRectRendersDirectly)
{
  EXPECT_TRUE(
      ShouldRenderChainDirectly(MathUtil::Rectangle<int>(0, 0, 1920, 1080), 1920, 1080, true));
}

TEST(ChainOutputPolicy, PillarboxedRectUsesIntermediateTarget)
{
  // Same height, narrower and offset: OutputSize would be wrong if rendered into the backbuffer.
  EXPECT_FALSE(
      ShouldRenderChainDirectly(MathUtil::Rectangle<int>(240, 0, 1680, 1080), 1920, 1080, true));
}

TEST(ChainOutputPolicy, LetterboxedRectUsesIntermediateTarget)
{
  EXPECT_FALSE(
      ShouldRenderChainDirectly(MathUtil::Rectangle<int>(0, 60, 1920, 1020), 1920, 1080, true));
}

TEST(ChainOutputPolicy, StereoHalfRectUsesIntermediateTarget)
{
  // Side-by-side stereo: full height, half width, at the origin.
  EXPECT_FALSE(
      ShouldRenderChainDirectly(MathUtil::Rectangle<int>(0, 0, 960, 1080), 1920, 1080, true));
}

TEST(ChainOutputPolicy, RectLargerThanBackbufferUsesIntermediateTarget)
{
  EXPECT_FALSE(
      ShouldRenderChainDirectly(MathUtil::Rectangle<int>(0, 0, 1920, 1080), 1280, 720, true));
}

// OpenGL's window framebuffer is FBO 0 with no attachment at all, and librashader's GL runtime
// builds its own FBO around the output TEXTURE it is handed -- so a framebuffer that owns no image
// cannot be the chain's render target, however well the rect matches.
TEST(ChainOutputPolicy, TargetWithoutAnImageUsesIntermediateTarget)
{
  EXPECT_FALSE(
      ShouldRenderChainDirectly(MathUtil::Rectangle<int>(0, 0, 1920, 1080), 1920, 1080, false));
}

// DynamicRenderingPolicy.RequiresFeatureAndEntryPoint used to sit here: the four rows of
// ChooseDynamicRendering's truth table, three of which were already static_asserted beside the
// function. Since the assertions have to hold for this file to compile at all, the test could only
// ever run green, and it reported nothing the build had not already refused. The missing fourth row
// was added to ChainOutputPolicy.h instead, where it is checked in every translation unit that
// includes the header rather than only when the test binary is built.
//
// ShouldRenderChainDirectly is constexpr too, but the cases above are not duplicates of anything:
// they name concrete geometries (pillarbox, letterbox, stereo half, oversized rect, imageless
// target) and exist to record which real situation each one stands for.
