// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/FileUtil.h"
#include "Common/ScopeGuard.h"
#include "VideoCommon/PostProcessing/PresetArchive.h"

using namespace VideoCommon;

namespace
{
// Writes a zip containing the given {internal_path, contents} entries.
void WriteZip(const std::string& zip_path,
              const std::vector<std::pair<std::string, std::string>>& entries)
{
  void* writer = mz_zip_writer_create();
  ASSERT_EQ(mz_zip_writer_open_file(writer, zip_path.c_str(), 0, 0), MZ_OK);
  for (const auto& [name, data] : entries)
  {
    mz_zip_file file_info = {};
    file_info.filename = name.c_str();
    file_info.flag = MZ_ZIP_FLAG_UTF8;
    ASSERT_EQ(mz_zip_writer_add_buffer(writer, const_cast<char*>(data.data()),
                                       static_cast<int32_t>(data.size()), &file_info),
              MZ_OK);
  }
  mz_zip_writer_close(writer);
  mz_zip_writer_delete(&writer);
}
}  // namespace

TEST(PresetArchive, ExtractsPreservingStructureAndFindsPreset)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bundle.zip";
  const std::string dest = tmp + "/out";

  WriteZip(zip, {
                    {"crt-royale.slangp", "shaders = \"1\"\nshader0 = \"src/a.slang\"\n"},
                    {"src/a.slang",
                     "#pragma stage vertex\nvoid main(){}\n#pragma stage fragment\nvoid main(){}\n"},
                    {"src/masks/m.png", "\x89PNG\r\n"},  // content irrelevant to extraction
                });

  const auto result = ImportPresetArchive(zip, dest);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.preset_name, "crt-royale");
  EXPECT_TRUE(File::Exists(dest + "/crt-royale.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/src/a.slang"));
  EXPECT_TRUE(File::Exists(dest + "/src/masks/m.png"));

  File::DeleteDirRecursively(tmp);
}

TEST(PresetArchive, RejectsArchiveWithNoPreset)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bundle.zip";
  WriteZip(zip, {{"readme.txt", "no preset here"}});
  const auto result = ImportPresetArchive(zip, tmp + "/out");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.empty());
  File::DeleteDirRecursively(tmp);
}

TEST(PresetArchive, RejectsPathTraversalEntries)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bundle.zip";
  // A malicious entry escaping dest_root must be rejected, not written outside.
  WriteZip(zip, {
                    {"crt.slangp", "shaders = \"0\"\n"},
                    {"../evil.slang", "pwned"},
                });
  const auto result = ImportPresetArchive(zip, tmp + "/out");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(File::Exists(tmp + "/evil.slang"));
  File::DeleteDirRecursively(tmp);
}

TEST(PresetArchive, StripPrefixExtractsOnlySubtree)
{
  const std::string dir = File::CreateTempDir();
  ASSERT_FALSE(dir.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(dir); }};

  const std::string zip = dir + "/pack.zip";
  WriteZip(zip, {
      {"top/RetroArch/shaders/shaders_slang/crt/x.slangp", "shaders = 0\n"},
      {"top/RetroArch/shaders/shaders_slang/crt/shaders/s.slang", "// stage\n"},
      {"top/Reshade/ignore.fx", "// not us\n"},
  });

  const std::string dest = dir + "/out";
  std::string error;
  const std::vector<std::string> presets = ExtractSanitizedArchive(
      zip, dest, &error, "top/RetroArch/shaders/shaders_slang/");

  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(presets, (std::vector<std::string>{"crt/x.slangp"}));
  EXPECT_TRUE(File::Exists(dest + "/crt/x.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/crt/shaders/s.slang"));
  EXPECT_FALSE(File::Exists(dest + "/Reshade/ignore.fx"));
}
