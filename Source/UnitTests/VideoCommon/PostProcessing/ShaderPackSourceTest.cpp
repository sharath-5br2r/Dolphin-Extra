// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#include "Common/ScopeGuard.h"
#include "VideoCommon/PostProcessing/ShaderPackSource.h"

using namespace VideoCommon;

TEST(ShaderPackSource, LibretroAndSatpixieRegistered)
{
  const ShaderPackSource* libretro = FindShaderPackSource("libretro");
  ASSERT_NE(libretro, nullptr);
  EXPECT_EQ(libretro->install_subdir, "shaders_slang");
  EXPECT_TRUE(libretro->extract_subpath.empty());
  EXPECT_TRUE(libretro->depends_on.empty());

  const ShaderPackSource* satpixie = FindShaderPackSource("satpixie");
  ASSERT_NE(satpixie, nullptr);
  EXPECT_EQ(satpixie->install_subdir, "shaders_slang");
  EXPECT_EQ(satpixie->extract_subpath, "satpixie-crt-shader/RetroArch/shaders/shaders_slang/");
}

TEST(ShaderPackSource, UnknownIdReturnsNull)
{
  EXPECT_EQ(FindShaderPackSource("nope"), nullptr);
}

TEST(ShaderPackSource, RetroCrisisDependsOnLibretro)
{
  const ShaderPackSource* rc = FindShaderPackSource("retrocrisis");
  ASSERT_NE(rc, nullptr);
  ASSERT_EQ(rc->depends_on.size(), 1u);
  EXPECT_EQ(rc->depends_on[0], "libretro");
  EXPECT_EQ(rc->install_subdir, "RetroCrisis");
}

TEST(ShaderPackSource, MissingDependenciesDetectsAbsentLibretro)
{
  const std::string dir = File::CreateTempDir();
  ASSERT_FALSE(dir.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(dir); }};

  const ShaderPackSource* rc = FindShaderPackSource("retrocrisis");
  ASSERT_NE(rc, nullptr);

  // libretro marker absent -> reported missing.
  EXPECT_EQ(MissingDependencies(*rc, dir), (std::vector<std::string>{"libretro"}));

  // Create the libretro marker -> no longer missing.
  const ShaderPackSource* libretro = FindShaderPackSource("libretro");
  ASSERT_NE(libretro, nullptr);
  const std::string marker = dir + "/" + libretro->install_marker;
  File::CreateFullPath(marker);
  File::CreateEmptyFile(marker);
  EXPECT_TRUE(MissingDependencies(*rc, dir).empty());
}
