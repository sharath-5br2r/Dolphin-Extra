// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <map>

#include "VideoCommon/PostProcessing/SlangPreset.h"

using namespace VideoCommon;

TEST(SlangPreset, ParsesPassCountAndPaths)
{
  const std::string text =
      "shaders = \"2\"\n"
      "shader0 = \"a.slang\"\n"
      "alias0 = \"FIRST\"\n"
      "filter_linear0 = \"true\"\n"
      "scale_type0 = \"source\"\n"
      "scale0 = \"1.0\"\n"
      "shader1 = \"../b.slang\"\n"
      "scale_type_x1 = \"viewport\"\n"
      "scale_x1 = \"0.5\"\n"
      "scale_type_y1 = \"absolute\"\n"
      "scale_y1 = \"240\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "/root/preset", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  ASSERT_EQ(cfg->passes.size(), 2u);
  EXPECT_EQ(cfg->passes[0].shader_path, "/root/preset/a.slang");
  EXPECT_EQ(cfg->passes[0].alias, "FIRST");
  EXPECT_TRUE(cfg->passes[0].filter_linear);
  EXPECT_EQ(cfg->passes[0].scale_type_x, ScaleType::Source);
  EXPECT_EQ(cfg->passes[0].scale_type_y, ScaleType::Source);
  // "../b.slang" resolves against the preset dir's parent.
  EXPECT_EQ(cfg->passes[1].shader_path, "/root/b.slang");
  EXPECT_EQ(cfg->passes[1].scale_type_x, ScaleType::Viewport);
  EXPECT_FLOAT_EQ(cfg->passes[1].scale_x, 0.5f);
  EXPECT_EQ(cfg->passes[1].scale_type_y, ScaleType::Absolute);
  EXPECT_FLOAT_EQ(cfg->passes[1].scale_y, 240.0f);
}

TEST(SlangPreset, ParsesTextures)
{
  const std::string text =
      "shaders = \"1\"\n"
      "shader0 = \"a.slang\"\n"
      "textures = \"MASK;GRID\"\n"
      "MASK = \"masks/m.png\"\n"
      "MASK_wrap_mode = \"repeat\"\n"
      "MASK_linear = \"true\"\n"
      "MASK_mipmap = \"true\"\n"
      "GRID = \"g.png\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "/root", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  ASSERT_EQ(cfg->luts.size(), 2u);
  EXPECT_EQ(cfg->luts[0].name, "MASK");
  EXPECT_EQ(cfg->luts[0].path, "/root/masks/m.png");
  EXPECT_EQ(cfg->luts[0].wrap_mode, SlangWrapMode::Repeat);
  EXPECT_TRUE(cfg->luts[0].linear);
  EXPECT_TRUE(cfg->luts[0].mipmap);
  EXPECT_EQ(cfg->luts[1].name, "GRID");
}

TEST(SlangPreset, RejectsMissingShaderCount)
{
  std::string error;
  const auto cfg = ParseSlangPreset("shader0 = \"a.slang\"\n", "/root", &error);
  EXPECT_FALSE(cfg.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(SlangPreset, CollectsNonStructuralKeysAsParameterOverrides)
{
  const std::string text =
      "shaders = 1\n"
      "shader0 = stock.slang\n"
      "filter_linear0 = true\n"
      "scale0 = 2.0\n"
      "scale_x0 = 1.5\n"
      "textures = LUT\n"
      "LUT = lut.png\n"
      "LUT_linear = true\n"
      "masksize = 2.000000\n"
      "mask_zoom = -1.500000\n"
      "some_label = notanumber\n";  // non-numeric -> ignored

  std::string error;
  const auto config = ParseSlangPreset(text, "/base", &error);
  ASSERT_TRUE(config.has_value()) << error;

  EXPECT_EQ(config->parameter_overrides.size(), 2u);
  EXPECT_FLOAT_EQ(config->parameter_overrides.at("masksize"), 2.0f);
  EXPECT_FLOAT_EQ(config->parameter_overrides.at("mask_zoom"), -1.5f);
  EXPECT_EQ(config->parameter_overrides.count("shaders"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("shader0"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("filter_linear0"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("scale0"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("scale_x0"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("textures"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("LUT"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("LUT_linear"), 0u);
  EXPECT_EQ(config->parameter_overrides.count("some_label"), 0u);
}

namespace
{
// In-memory reader mapping absolute preset paths to their text, for #reference tests.
VideoCommon::SlangPresetReader MapReader(std::map<std::string, std::string> files)
{
  return [files = std::move(files)](const std::string& path, std::string* out) {
    const auto it = files.find(path);
    if (it == files.end())
      return false;
    *out = it->second;
    return true;
  };
}
}  // namespace

TEST(SlangPreset, ReferenceInheritsPassesAndMergesOverrides)
{
  // Child references base and only overrides a parameter.
  const std::string base =
      "shaders = 1\n"
      "shader0 = ../shaders_slang/crt/stock.slang\n"
      "masksize = 1.000000\n"
      "gamma = 2.400000\n";
  const std::string child =
      "#reference \"../4K Flat/preset.slangp\"\n"
      "masksize = 3.000000\n";

  const auto reader = MapReader({{"/packs/4K Flat/preset.slangp", base}});
  std::string error;
  const auto config =
      ParseSlangPreset(child, "/packs/1080p Flat", &error, reader);
  ASSERT_TRUE(config.has_value()) << error;

  // Passes inherited from base, resolved relative to the BASE file's directory.
  ASSERT_EQ(config->passes.size(), 1u);
  EXPECT_EQ(config->passes[0].shader_path, "/packs/shaders_slang/crt/stock.slang");
  // Child override wins; base's other override is retained.
  EXPECT_FLOAT_EQ(config->parameter_overrides.at("masksize"), 3.0f);
  EXPECT_FLOAT_EQ(config->parameter_overrides.at("gamma"), 2.4f);
}

TEST(SlangPreset, ReferenceChainDepthTwo)
{
  const std::string k4 =
      "shaders = 1\n"
      "shader0 = ../shaders_slang/crt/stock.slang\n"
      "curvature = 0.000000\n";
  const std::string k1080 =
      "#reference \"../4K Flat/p.slangp\"\n"
      "masksize = 2.000000\n";
  const std::string curved =
      "#reference \"../1080p Flat/p.slangp\"\n"
      "curvature = 1.000000\n";

  const auto reader = MapReader({
      {"/packs/4K Flat/p.slangp", k4},
      {"/packs/1080p Flat/p.slangp", k1080},
  });
  std::string error;
  const auto config = ParseSlangPreset(curved, "/packs/1080p Curved", &error, reader);
  ASSERT_TRUE(config.has_value()) << error;
  EXPECT_EQ(config->passes[0].shader_path, "/packs/shaders_slang/crt/stock.slang");
  EXPECT_FLOAT_EQ(config->parameter_overrides.at("masksize"), 2.0f);   // from 1080p Flat
  EXPECT_FLOAT_EQ(config->parameter_overrides.at("curvature"), 1.0f);  // curved wins over base 0
}

TEST(SlangPreset, MissingReferenceFails)
{
  std::string error;
  const auto config = ParseSlangPreset("#reference \"nope.slangp\"\nshaders = 0\n",
                                       "/packs/x", &error, MapReader({}));
  EXPECT_FALSE(config.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(SlangPreset, NormalizePathAcceptsWindowsSeparators)
{
  EXPECT_EQ(NormalizePath("E:\\work\\dolphin\\Sys\\Shaders"), "E:/work/dolphin/Sys/Shaders");
  EXPECT_EQ(NormalizePath("crt\\shaders\\royale"), "crt/shaders/royale");
}

TEST(SlangPreset, NormalizePathCollapsesDotDotAcrossWindowsSeparators)
{
  EXPECT_EQ(NormalizePath("E:\\a\\b\\..\\c.slang"), "E:/a/c.slang");
  EXPECT_EQ(NormalizePath("E:\\Sys\\Shaders\\crt/../misc/x.slang"), "E:/Sys/Shaders/misc/x.slang");
}

TEST(SlangPreset, NormalizePathKeepsWindowsDriveRoot)
{
  // ".." must never eat the drive designator.
  EXPECT_EQ(NormalizePath("E:\\..\\..\\x"), "E:/x");
  EXPECT_EQ(NormalizePath("E:/"), "E:/");
}

TEST(SlangPreset, NormalizePathKeepsUncRoot)
{
  // "\\host\share" is the whole root on Windows -- the share is the drive-letter equivalent -- so
  // ".." must not be able to eat either the share or the host.
  EXPECT_EQ(NormalizePath("\\\\host\\share\\..\\a"), "//host/share/a");
  EXPECT_EQ(NormalizePath("\\\\host\\share\\..\\..\\..\\a"), "//host/share/a");
  EXPECT_EQ(NormalizePath("//host/share"), "//host/share");
  EXPECT_EQ(NormalizePath("//host/share/"), "//host/share");
  EXPECT_EQ(NormalizePath("//host/share/pack/../x.slangp"), "//host/share/x.slangp");
  // A real preset reference out of a UNC-installed pack keeps host and share.
  EXPECT_EQ(NormalizePath("//nas/dolphin/User/Load/Shaders/pack/../../shaders_slang/crt/x.slang"),
            "//nas/dolphin/User/Load/shaders_slang/crt/x.slang");
  // Degenerate forms still normalize without losing the leading "//".
  EXPECT_EQ(NormalizePath("//host"), "//host");
  EXPECT_EQ(NormalizePath("//"), "//");
  EXPECT_EQ(NormalizePath("//../a"), "//a");
}

TEST(SlangPreset, NormalizePathPreservesPosixBehavior)
{
  EXPECT_EQ(NormalizePath("/root/preset/../b.slang"), "/root/b.slang");
  EXPECT_EQ(NormalizePath("/root/./a//b"), "/root/a/b");
  EXPECT_EQ(NormalizePath("/../a"), "/a");
  EXPECT_EQ(NormalizePath("a/../../b"), "../b");
  EXPECT_EQ(NormalizePath(""), "");
}

TEST(SlangPreset, ResolvesPassPathsFromWindowsBaseDir)
{
  const std::string text = "shaders = \"1\"\n"
                           "shader0 = \"../misc/x.slang\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "E:\\Sys\\Shaders\\crt", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  ASSERT_EQ(cfg->passes.size(), 1u);
  EXPECT_EQ(cfg->passes[0].shader_path, "E:/Sys/Shaders/misc/x.slang");
}

TEST(SlangPreset, ResolvesAbsoluteShaderPathIgnoringBaseDir)
{
  const std::string text = "shaders = \"1\"\n"
                           "shader0 = \"/abs/x.slang\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "/root/preset", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  EXPECT_EQ(cfg->passes[0].shader_path, "/abs/x.slang");
}
