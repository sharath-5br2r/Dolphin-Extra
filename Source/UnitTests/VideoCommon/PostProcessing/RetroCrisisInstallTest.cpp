// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/ScopeGuard.h"
#include "VideoCommon/PostProcessing/RetroCrisisInstall.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"

using namespace VideoCommon;

namespace
{
void WriteFile(const std::string& path, const std::string& text)
{
  File::CreateFullPath(path);
  File::IOFile f(path, "wb");
  f.WriteBytes(text.data(), text.size());
}
}  // namespace

TEST(RetroCrisisInstall, ProfileOfExtractsTopFolder)
{
  EXPECT_EQ(RetroCrisisProfileOf("retro crisis/1080p Curved/nes.slangp"), "1080p Curved");
  EXPECT_EQ(RetroCrisisProfileOf("retro crisis/4K Flat/snes.slangp"), "4K Flat");
  EXPECT_EQ(RetroCrisisProfileOf("readme.txt"), "");
}

TEST(RetroCrisisInstall, ClosureFollowsReferenceChain)
{
  // 1080p Curved -> 1080p Flat -> 4K Flat
  const std::map<std::string, std::set<std::string>> refs = {
      {"1080p Curved", {"1080p Flat"}},
      {"1080p Flat", {"4K Flat"}},
      {"4K Flat", {}},
      {"1440p Flat", {"4K Flat"}},
  };
  EXPECT_EQ(ComputeRetroCrisisClosure("1080p Curved", refs),
            (std::set<std::string>{"1080p Curved", "1080p Flat", "4K Flat"}));
  EXPECT_EQ(ComputeRetroCrisisClosure("4K Flat", refs),
            (std::set<std::string>{"4K Flat"}));
}

TEST(RetroCrisisInstall, InstallsChosenProfileClosureOnly)
{
  const std::string dir = File::CreateTempDir();
  ASSERT_FALSE(dir.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(dir); }};

  const std::string extract = dir + "/extract";
  WriteFile(extract + "/retro crisis/4K Flat/nes.slangp",
            "shaders = 1\nshader0 = ../../../shaders_slang/crt/stock.slang\n");
  WriteFile(extract + "/retro crisis/1080p Flat/nes.slangp",
            "#reference \"../4K Flat/nes.slangp\"\nmasksize = 2.0\n");
  WriteFile(extract + "/retro crisis/1440p Flat/nes.slangp",
            "#reference \"../4K Flat/nes.slangp\"\nmasksize = 3.0\n");

  const std::string install = dir + "/Shaders/RetroCrisis";
  const u32 count = InstallRetroCrisisProfile(extract, install, "1080p Flat");

  EXPECT_EQ(count, 1u);  // one preset under the chosen profile
  EXPECT_TRUE(File::Exists(install + "/retro crisis/1080p Flat/nes.slangp"));
  EXPECT_TRUE(File::Exists(install + "/retro crisis/4K Flat/nes.slangp"));   // closure dep
  EXPECT_FALSE(File::Exists(install + "/retro crisis/1440p Flat/nes.slangp"));  // not in closure
  EXPECT_EQ(ReadRetroCrisisProfile(install), "1080p Flat");
}

TEST(RetroCrisisInstall, HidesNonChosenProfilePresets)
{
  const std::string root = "/u/Shaders/RetroCrisis";
  EXPECT_FALSE(IsHiddenRetroCrisisPreset(
      root + "/retro crisis/1080p Flat/nes.slangp", root, "1080p Flat"));
  EXPECT_TRUE(IsHiddenRetroCrisisPreset(
      root + "/retro crisis/4K Flat/nes.slangp", root, "1080p Flat"));
  // Non-RetroCrisis preset is never hidden.
  EXPECT_FALSE(IsHiddenRetroCrisisPreset(
      "/u/Shaders/shaders_slang/crt/crt-royale.slangp", root, "1080p Flat"));
}

TEST(RetroCrisisInstall, ProfileListMatchesPackFolders)
{
  // These are the seven top-level folder names inside the Retro Crisis pack, and they are also the
  // entries, in order, of the Android picker's post_processing_retrocrisis_profiles string-array
  // (Source/Android/app/src/main/res/values/strings.xml). Spelled out here so that editing
  // GetRetroCrisisProfiles() without updating the pack (or the picker) fails the build's tests
  // instead of silently installing a profile whose folder does not exist.
  EXPECT_EQ(GetRetroCrisisProfiles(),
            (std::vector<std::string>{"1080p Flat", "1440p Flat", "4K Flat", "1080p Curved",
                                      "1440p Curved", "4K Curved", "720p Steam Deck"}));

  const auto& profiles = GetRetroCrisisProfiles();
  const std::set<std::string> unique(profiles.begin(), profiles.end());
  EXPECT_EQ(unique.size(), profiles.size()) << "duplicate profile name";

  // Every profile must be a name RetroCrisisProfileOf can recover from a preset path, i.e. it must
  // be a single path component. RetroCrisisProfileOf expects a pack-relative path.
  for (const std::string& profile : profiles)
  {
    EXPECT_EQ(profile.find('/'), std::string::npos) << profile;
    EXPECT_EQ(RetroCrisisProfileOf("retro crisis/" + profile + "/x.slangp"), profile);
  }
}
