// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/SlangSourceDownscale.h"

using namespace VideoCommon;

// No native size supplied (the caller path that cannot report one): never downscale.
TEST(SlangSourceDownscale, NoNativeSizeIsNoOp)
{
  const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(1824, 1590, 0, 0);
  EXPECT_FALSE(plan.normalize);
}

// PCSX2 still materializes its native-sized chain input at 1x.
TEST(SlangSourceDownscale, SourceAtNativeUsesBilinearCopy)
{
  const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(608, 530, 608, 530);
  EXPECT_TRUE(plan.normalize);
  EXPECT_FALSE(plan.box_filter);
}

// A source below native size is enlarged with the same bilinear normalization path.
TEST(SlangSourceDownscale, SourceBelowNativeUsesBilinearCopy)
{
  const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(320, 240, 640, 480);
  EXPECT_TRUE(plan.normalize);
  EXPECT_FALSE(plan.box_filter);
}

// Exact integer upscale on both axes: box-average (SSAA) with the whole NxN footprint.
TEST(SlangSourceDownscale, ExactIntegerFactorUsesBoxFilter)
{
  for (u32 factor : {2u, 3u, 4u, 6u, 8u})
  {
    const SlangSourceDownscalePlan plan =
        PlanSlangSourceDownscale(608 * factor, 530 * factor, 608, 530);
    EXPECT_TRUE(plan.normalize) << "factor=" << factor;
    EXPECT_TRUE(plan.box_filter) << "factor=" << factor;
    EXPECT_EQ(plan.factor, factor) << "factor=" << factor;
  }
}

// Fractional (non-integer) multiplier: no integer box fits, fall back to a bilinear resample.
TEST(SlangSourceDownscale, FractionalFactorFallsBackToBilinear)
{
  // 1824x1590 from 608x530 is 3x exactly, but 1500x1300 is a fractional multiple.
  const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(1500, 1300, 608, 530);
  EXPECT_TRUE(plan.normalize);
  EXPECT_FALSE(plan.box_filter);
}

// Different integer factor per axis (fx != fy): a single box factor cannot cover both -> bilinear.
TEST(SlangSourceDownscale, MismatchedAxisFactorsFallBackToBilinear)
{
  const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(608 * 2, 530 * 3, 608, 530);
  EXPECT_TRUE(plan.normalize);
  EXPECT_FALSE(plan.box_filter);
}

// Only one axis exceeds native: still a downscale (mirrors the OR condition), but no square box.
TEST(SlangSourceDownscale, SingleAxisOverNativeFallsBackToBilinear)
{
  const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(608 * 2, 530, 608, 530);
  EXPECT_TRUE(plan.normalize);
  EXPECT_FALSE(plan.box_filter);
}
