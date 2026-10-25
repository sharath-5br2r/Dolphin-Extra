// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PerformanceMetrics.h"

TEST(PerformanceMetricsTest, LatestFrameBufferSizeReturnsOneCompleteSample)
{
  PerformanceMetrics metrics;

  metrics.SetLatestFrameBufferSize(1920, 1584);

  const auto [width, height] = metrics.GetLatestFrameBufferSize();
  EXPECT_EQ(width, 1920u);
  EXPECT_EQ(height, 1584u);
}
