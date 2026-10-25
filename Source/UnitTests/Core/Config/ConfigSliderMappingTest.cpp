// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "DolphinQt/Config/Binder/ConfigSliderMapping.h"

namespace
{
constexpr ConfigWidget::FloatSliderRange UNIT_RANGE{0.0f, 1.0f, 0.01f};
constexpr ConfigWidget::FloatSliderRange SCALE_RANGE{0.5f, 2.0f, 0.25f};
}  // namespace

TEST(ConfigSliderMapping, MinimumAndMaximumMapToTheEndPositions)
{
  EXPECT_EQ(UNIT_RANGE.PositionForValue(0.0f), 0);
  EXPECT_EQ(UNIT_RANGE.PositionForValue(1.0f), UNIT_RANGE.MaximumPosition());
  EXPECT_FLOAT_EQ(UNIT_RANGE.ValueForPosition(0), 0.0f);
  EXPECT_FLOAT_EQ(UNIT_RANGE.ValueForPosition(UNIT_RANGE.MaximumPosition()), 1.0f);
}

TEST(ConfigSliderMapping, MaximumPositionIsTheHighestPositionNotThePositionCount)
{
  // 0.0 to 1.0 in steps of 0.01 is 101 positions, the highest of which is 100.
  EXPECT_EQ(UNIT_RANGE.MaximumPosition(), 100);
  EXPECT_EQ(SCALE_RANGE.MaximumPosition(), 6);
}

TEST(ConfigSliderMapping, RoundTripsEveryStepExactly)
{
  for (int position = 0; position <= SCALE_RANGE.MaximumPosition(); ++position)
    EXPECT_EQ(SCALE_RANGE.PositionForValue(SCALE_RANGE.ValueForPosition(position)), position)
        << "position " << position;
}

TEST(ConfigSliderMapping, SnapsAnOffStepValueToTheNearestPosition)
{
  // 0.6 sits between the 0.5 and 0.75 steps, nearer 0.5.
  EXPECT_EQ(SCALE_RANGE.PositionForValue(0.6f), 0);
  // 0.7 is nearer 0.75.
  EXPECT_EQ(SCALE_RANGE.PositionForValue(0.7f), 1);
}

TEST(ConfigSliderMapping, ClampsValuesOutsideTheRange)
{
  EXPECT_EQ(SCALE_RANGE.PositionForValue(-5.0f), 0);
  EXPECT_EQ(SCALE_RANGE.PositionForValue(100.0f), SCALE_RANGE.MaximumPosition());
}

TEST(ConfigSliderMapping, ClampsPositionsOutsideTheRange)
{
  EXPECT_FLOAT_EQ(SCALE_RANGE.ValueForPosition(-3), 0.5f);
  EXPECT_FLOAT_EQ(SCALE_RANGE.ValueForPosition(999), 2.0f);
}
