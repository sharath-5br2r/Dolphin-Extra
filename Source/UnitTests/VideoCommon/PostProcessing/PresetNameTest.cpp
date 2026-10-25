// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"

using namespace VideoCommon;

// The identifier stored in GFX_ENHANCE_POST_SHADER must round-trip: GetPresetList produces it
// from a discovered path, and LoadPreset resolves it back to <root>/<name>.slangp. Presets in
// the libretro pack live in subdirectories (crt/, border/, ...), so the name must keep the
// subdirectory, not collapse to the bare basename.
TEST(PresetName, KeepsSubdirectoryRelativeToRoot)
{
  const std::vector<std::string> roots = {"/user/Shaders/", "/sys/Shaders/"};
  EXPECT_EQ(PresetNameFromPath("/user/Shaders/crt/crt-royale.slangp", roots), "crt/crt-royale");
  EXPECT_EQ(PresetNameFromPath("/sys/Shaders/border/sgb/sgb-crt-royale.slangp", roots),
            "border/sgb/sgb-crt-royale");
}

TEST(PresetName, TopLevelPresetHasNoSlash)
{
  const std::vector<std::string> roots = {"/user/Shaders/"};
  EXPECT_EQ(PresetNameFromPath("/user/Shaders/bilinear.slangp", roots), "bilinear");
}

TEST(PresetName, HandlesRootWithoutTrailingSeparator)
{
  const std::vector<std::string> roots = {"/user/Shaders"};
  EXPECT_EQ(PresetNameFromPath("/user/Shaders/crt/crt-royale.slangp", roots), "crt/crt-royale");
}

TEST(PresetName, NormalizesBackslashes)
{
  const std::vector<std::string> roots = {"C:/user/Shaders/"};
  EXPECT_EQ(PresetNameFromPath("C:\\user\\Shaders\\crt\\crt-royale.slangp", roots),
            "crt/crt-royale");
}

// After re-rooting, presets live under <Shaders>/shaders_slang/. The display name must strip
// that segment so existing GFX_ENHANCE_POST_SHADER values (e.g. "crt/crt-royale") keep matching.
TEST(PresetName, StripsShadersSlangPrefix)
{
  const std::vector<std::string> roots = {"/user/Shaders/shaders_slang/", "/user/Shaders/"};
  EXPECT_EQ(
      PresetNameFromPath("/user/Shaders/shaders_slang/crt/crt-royale.slangp", roots),
      "crt/crt-royale");
}
