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
#include "Common/IOFile.h"
#include "Common/ScopeGuard.h"
#include "VideoCommon/PostProcessing/ShaderPackDownload.h"
#include "VideoCommon/PostProcessing/ShaderPackSource.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"

using namespace VideoCommon;

namespace
{
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

TEST(ShaderPackDownload, InstallsAllPresetsFromPackZip)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/shaders_slang.zip";
  const std::string dest = tmp + "/Shaders";

  // Mirrors the buildbot pack shape: many presets across subdirs sharing an include/ tree.
  WriteZip(zip, {
                    {"crt/crt-royale.slangp", "shaders = \"1\"\nshader0 = \"../include/a.slang\"\n"},
                    {"crt/crt-geom.slangp", "shaders = \"1\"\nshader0 = \"../include/a.slang\"\n"},
                    {"include/a.slang",
                     "#pragma stage vertex\nvoid main(){}\n#pragma stage fragment\nvoid main(){}\n"},
                    {"README.md", "libretro slang shaders"},
                });

  const auto result = InstallShaderPackFromZip(zip, dest);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.preset_count, 2u);
  EXPECT_TRUE(File::Exists(dest + "/crt/crt-royale.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/crt/crt-geom.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/include/a.slang"));  // shared include preserved

  File::DeleteDirRecursively(tmp);
}

TEST(ShaderPackDownload, RejectsCorruptZip)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bad.zip";
  File::IOFile(zip, "wb").WriteBytes("not a zip", 9);
  const auto result = InstallShaderPackFromZip(zip, tmp + "/Shaders");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.empty());
  File::DeleteDirRecursively(tmp);
}

TEST(ShaderPackDownload, InstallSourceAppliesSubpathAndSubdir)
{
  const std::string dir = File::CreateTempDir();
  ASSERT_FALSE(dir.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(dir); }};

  const std::string zip = dir + "/satpixie.zip";
  WriteZip(zip, {
      {"satpixie-crt-shader/RetroArch/shaders/shaders_slang/crt/satpixie-crt.slangp", "shaders = 0\n"},
      {"satpixie-crt-shader/Reshade/ignore.fx", "// no\n"},
  });

  const VideoCommon::ShaderPackSource source{
      "satpixie", "CRT-SatPixie", "unused-url",
      "satpixie-crt-shader/RetroArch/shaders/shaders_slang/", "shaders_slang", {}};

  const std::string shaders_root = dir + "/Shaders";
  const ShaderPackDownloadResult result =
      InstallShaderPackSource(source, zip, shaders_root);

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.preset_count, 1u);
  EXPECT_TRUE(File::Exists(shaders_root + "/shaders_slang/crt/satpixie-crt.slangp"));
  EXPECT_FALSE(File::Exists(shaders_root + "/shaders_slang/Reshade/ignore.fx"));
}

TEST(ShaderPackDownload, LolaRetroPreservesPackLayoutAndRelativeShaderPaths)
{
  const std::string dir = File::CreateTempDir();
  ASSERT_FALSE(dir.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(dir); }};

  const ShaderPackSource* source = FindShaderPackSource("lola-retro");
  ASSERT_NE(source, nullptr);
  EXPECT_TRUE(source->depends_on.empty());

  const std::string zip = dir + "/lola-retro.zip";
  const std::string preset_text = "shaders = 1\nshader0 = ../../shaders/crt.slang\n";
  // The official release keeps full, lite and HDR presets next to their shared shader tree.
  WriteZip(zip, {{"lola-retro/systems/curved/gamecube.slangp", preset_text},
                 {"lola-retro/lite/systems/flat/gamecube.slangp",
                  "shaders = 1\nshader0 = ../../../shaders/crt-lite.slang\n"},
                 {"lola-retro/hdr/systems/curved/gamecube.slangp", "shaders = 0\n"},
                 {"lola-retro/shaders/crt.slang", "#include \"include/params.inc\"\n"},
                 {"lola-retro/shaders/crt-lite.slang", "#include \"include/params.inc\"\n"},
                 {"lola-retro/shaders/include/params.inc", "// shared parameters\n"},
                 {"lola-retro/LICENSE", "MIT license\n"}});

  const std::string shaders_root = dir + "/Shaders";
  const std::string existing_shader = shaders_root + "/shaders_slang/crt/satpixie-crt.slangp";
  ASSERT_TRUE(File::CreateFullPath(existing_shader));
  ASSERT_TRUE(File::CreateEmptyFile(existing_shader));

  const auto result = InstallShaderPackSource(*source, zip, shaders_root);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.preset_count, 3u);
  EXPECT_TRUE(File::Exists(shaders_root + "/" + source->install_marker));
  EXPECT_TRUE(File::Exists(existing_shader));

  const std::string pack_root = shaders_root + "/shaders_slang/lola-retro";
  std::string error;
  const auto preset = ParseSlangPreset(preset_text, pack_root + "/systems/curved", &error);
  ASSERT_TRUE(preset.has_value()) << error;
  ASSERT_EQ(preset->passes.size(), 1u);
  EXPECT_EQ(preset->passes[0].shader_path, NormalizePath(pack_root + "/shaders/crt.slang"));
  EXPECT_TRUE(File::Exists(preset->passes[0].shader_path));
  EXPECT_TRUE(File::Exists(pack_root + "/shaders/include/params.inc"));
  EXPECT_TRUE(File::Exists(pack_root + "/lite/systems/flat/gamecube.slangp"));
  EXPECT_TRUE(File::Exists(pack_root + "/hdr/systems/curved/gamecube.slangp"));
  EXPECT_TRUE(File::Exists(pack_root + "/LICENSE"));
}

TEST(ShaderPackDownload, UnknownIdReturnsError)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(tmp); }};

  const ShaderPackDownloadResult result = DownloadShaderPackById("nope", tmp + "/Shaders", nullptr);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.preset_count, 0u);
  EXPECT_EQ(result.error, "unknown shader pack id");
}

TEST(ShaderPackDownload, FailsWhenSubpathMatchesNothing)
{
  const std::string dir = File::CreateTempDir();
  ASSERT_FALSE(dir.empty());
  Common::ScopeGuard guard{[&] { File::DeleteDirRecursively(dir); }};

  const std::string zip = dir + "/pack.zip";
  WriteZip(zip, {
      {"other-subtree/preset.slangp", "shaders = 0\n"},
      {"other-subtree/shader.slang", "#pragma stage vertex\nvoid main(){}\n"},
  });

  const VideoCommon::ShaderPackSource source{
      "test-wrong-path", "Test", "unused-url",
      "nonexistent-subtree/", "shaders_slang", {}};

  const std::string shaders_root = dir + "/Shaders";
  const ShaderPackDownloadResult result = InstallShaderPackSource(source, zip, shaders_root);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.preset_count, 0u);
  EXPECT_FALSE(result.error.empty());
}
