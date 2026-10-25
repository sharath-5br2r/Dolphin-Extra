// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/ScopeGuard.h"
#include "VideoCommon/VideoConfig.h"

TEST(VideoConfig, VerifyValidityClampsRemovedStereoModes)
{
  // VerifyValidity() reads g_backend_info; keep geometry shaders "supported" so the pre-existing
  // clamp below the one under test does not fire, and put the global back afterwards.
  const BackendInfo saved_backend_info = g_backend_info;
  Common::ScopeGuard restore{[&] { g_backend_info = saved_backend_info; }};
  g_backend_info.bSupportsGeometryShaders = true;

  VideoConfig config;

  // Anaglyph and Passive were implemented by the removed legacy post-processing shader. A stored
  // GFX.ini value must not keep costing a second layer for a mono image.
  config.stereo_mode = StereoMode::Anaglyph;
  config.VerifyValidity();
  EXPECT_EQ(config.stereo_mode, StereoMode::Off);

  config.stereo_mode = StereoMode::Passive;
  config.VerifyValidity();
  EXPECT_EQ(config.stereo_mode, StereoMode::Off);

  // The modes the UI still offers must survive untouched.
  for (const StereoMode mode : {StereoMode::Off, StereoMode::SideBySide, StereoMode::TopAndBottom,
                                StereoMode::QuadBuffer})
  {
    config.stereo_mode = mode;
    config.VerifyValidity();
    EXPECT_EQ(config.stereo_mode, mode);
  }
}
