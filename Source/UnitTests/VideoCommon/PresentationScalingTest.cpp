// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PresentationScaling.h"

using namespace VideoCommon;

TEST(PresentationScaling, LandscapeSourceUsesIntegerWidth)
{
  EXPECT_EQ(ApplyIntegerScaling(1440, 1080, 640, 528), std::make_pair(1280, 960));
}

TEST(PresentationScaling, PortraitSourceUsesIntegerHeight)
{
  EXPECT_EQ(ApplyIntegerScaling(1080, 1440, 528, 640), std::make_pair(960, 1280));
}

TEST(PresentationScaling, ExactIntegerScaleIsUnchanged)
{
  EXPECT_EQ(ApplyIntegerScaling(1280, 960, 640, 480), std::make_pair(1280, 960));
}

TEST(PresentationScaling, DownscalingIsUnchanged)
{
  EXPECT_EQ(ApplyIntegerScaling(600, 450, 640, 480), std::make_pair(600, 450));
}

TEST(PresentationScaling, InvalidSourceIsUnchanged)
{
  EXPECT_EQ(ApplyIntegerScaling(1280, 960, 0, 0), std::make_pair(1280, 960));
}
