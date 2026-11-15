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

TEST(ShaderPackSource, LatestLolaReleaseSelectsRetroArchZipForANewTag)
{
  const auto* source = FindShaderPackSource("lola-retro");
  ASSERT_NE(source, nullptr);
  const auto url = FindShaderPackReleaseAssetUrl(*source, R"({
    "tag_name": "v99.0",
    "zipball_url": "https://api.github.com/repos/zoran123456/lola-retro/zipball/v99.0",
    "assets": [
      {"name": "lola-retro-kodi-v99.0.zip", "browser_download_url": "https://github.com/wrong-kodi.zip"},
      {"name": "lola-retro-amiberry-v99.0.zip", "browser_download_url": "https://github.com/wrong-amiberry.zip"},
      {"name": "lola-retro-v99.0.zip", "browser_download_url": "https://github.com/zoran123456/lola-retro/releases/download/v99.0/lola-retro-v99.0.zip"},
      {"name": "lola-retro-86box-v99.0.zip", "browser_download_url": "https://github.com/wrong-86box.zip"}
    ]
  })");
  ASSERT_TRUE(url.has_value());
  EXPECT_EQ(*url, "https://github.com/zoran123456/lola-retro/releases/download/v99.0/"
                  "lola-retro-v99.0.zip");
}

TEST(ShaderPackSource, LatestReleaseDoesNotFallBackToOtherPortsOrOlderAssets)
{
  const auto* source = FindShaderPackSource("lola-retro");
  ASSERT_NE(source, nullptr);
  EXPECT_FALSE(FindShaderPackReleaseAssetUrl(*source, R"({
    "tag_name": "v99.0", "assets": [
      {"name": "lola-retro-kodi-v99.0.zip", "browser_download_url": "https://github.com/kodi.zip"},
      {"name": "lola-retro-v98.0.zip", "browser_download_url": "https://github.com/older.zip"}
    ]
  })"));
}

TEST(ShaderPackSource, LatestReleaseRejectsMalformedOrMissingMetadata)
{
  const auto* source = FindShaderPackSource("lola-retro");
  ASSERT_NE(source, nullptr);
  for (const char* json : {"not JSON", "null", "[]", "{}", R"({"message":"rate limit exceeded"})",
                           R"({"tag_name":42,"assets":[]})", R"({"tag_name":"","assets":[]})",
                           R"({"tag_name":"v99.0","assets":{}})",
                           R"({"tag_name":"v99.0","assets":[null,42,{}, {"name":42}]})"})
  {
    SCOPED_TRACE(json);
    EXPECT_FALSE(FindShaderPackReleaseAssetUrl(*source, json));
  }
}

TEST(ShaderPackSource, LatestReleaseRejectsAmbiguousOrInvalidDownloadUrls)
{
  const auto* source = FindShaderPackSource("lola-retro");
  ASSERT_NE(source, nullptr);
  EXPECT_FALSE(FindShaderPackReleaseAssetUrl(*source, R"({
    "tag_name":"v99.0", "assets":[
      {"name":"lola-retro-v99.0.zip","browser_download_url":"https://github.com/first.zip"},
      {"name":"lola-retro-v99.0.zip","browser_download_url":"https://github.com/second.zip"}
    ]
  })"));
  for (const char* url : {"null", "42", "\"\"", "\"http://github.com/asset.zip\"",
                          "\"https://github.com.example/asset.zip\""})
  {
    SCOPED_TRACE(url);
    EXPECT_FALSE(FindShaderPackReleaseAssetUrl(
        *source,
        std::string(
            R"({"tag_name":"v99.0","assets":[{"name":"lola-retro-v99.0.zip","browser_download_url":)") +
            url + "}]}"));
  }
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
