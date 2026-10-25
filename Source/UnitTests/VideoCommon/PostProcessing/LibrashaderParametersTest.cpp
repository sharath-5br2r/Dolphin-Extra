// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/FileUtil.h"
#include "VideoCommon/PostProcessing/LibrashaderParameters.h"

using namespace VideoCommon::LibrashaderParameters;

TEST(LibrashaderParameters, DecimalsForStepUsesFormulaNotTable)
{
  // PCSX2's logarithmic formula, not a lookup table. The table cannot work: the shader pack
  // declares 73 distinct step values; a six-entry table leaves 67 undefined.
  EXPECT_EQ(DecimalsForStep(0.0f), 3);
  EXPECT_EQ(DecimalsForStep(-1.0f), 3);
  EXPECT_EQ(DecimalsForStep(1.0f), 0);
  EXPECT_EQ(DecimalsForStep(0.5f), 1);   // ceil(-log10(0.5)) = ceil(0.301...) = 1
  EXPECT_EQ(DecimalsForStep(0.05f), 2);  // ceil(-log10(0.05)) = ceil(1.301...) = 2
  EXPECT_EQ(DecimalsForStep(0.01f), 2);  // The 1e-6 bias exists for this case
  EXPECT_EQ(DecimalsForStep(0.001f), 3);
  EXPECT_EQ(DecimalsForStep(0.0001f), 4);
  EXPECT_EQ(DecimalsForStep(0.00001f), 4);  // Clamped to max 4
  EXPECT_EQ(DecimalsForStep(0.00009f), 4);
  EXPECT_EQ(DecimalsForStep(2.0f), 0);

  // Four real steps from the shader pack (73 distinct values total).
  // 0.017453292519943295 = pi/180 (degrees-to-radians): ceil(-log10(0.017453...)) = 2
  EXPECT_EQ(DecimalsForStep(0.017453292519943295f), 2);
  // 0.00390625 = 1/256: ceil(-log10(0.00390625)) = 3
  EXPECT_EQ(DecimalsForStep(0.00390625f), 3);
  // 0.05555: ceil(-log10(0.05555)) = 2
  EXPECT_EQ(DecimalsForStep(0.05555f), 2);
  // 0.25: ceil(-log10(0.25)) = 1
  EXPECT_EQ(DecimalsForStep(0.25f), 1);
}

TEST(LibrashaderParameters, IsDefaultValueUsesRelativeEpsilon)
{
  // PCSX2's relative epsilon: abs(value - initial) <= 1e-6 * max(1, abs(initial)).
  EXPECT_TRUE(IsDefaultValue(0.0f, 0.0f));
  EXPECT_TRUE(IsDefaultValue(1.0f, 1.0f));
  EXPECT_TRUE(IsDefaultValue(1.0f + 1e-7f, 1.0f));
  EXPECT_FALSE(IsDefaultValue(1.0f + 1e-5f, 1.0f));

  // Large initial values scale the epsilon proportionally.
  EXPECT_TRUE(IsDefaultValue(1000.0f + 1e-4f, 1000.0f));
  EXPECT_FALSE(IsDefaultValue(1000.0f + 1e-2f, 1000.0f));

  // Edge: initial is negative.
  EXPECT_TRUE(IsDefaultValue(-1.0f - 1e-7f, -1.0f));
  EXPECT_FALSE(IsDefaultValue(-1.0f - 1e-5f, -1.0f));
}

TEST(LibrashaderParameters, ParseOverridesSucceeds)
{
  // Parameter names come from #pragma parameter and none in the shader pack contains ';' or '=',
  // so ';' is a safe separator and '=' splits name from value.
  const std::vector<std::string> entries = {"param1=1.5", "param2=2.0", "param3=0.0"};
  const Overrides overrides = ParseOverrides(entries);

  ASSERT_EQ(overrides.size(), 3u);
  EXPECT_EQ(overrides[0].first, "param1");
  EXPECT_FLOAT_EQ(overrides[0].second, 1.5f);
  EXPECT_EQ(overrides[1].first, "param2");
  EXPECT_FLOAT_EQ(overrides[1].second, 2.0f);
  EXPECT_EQ(overrides[2].first, "param3");
  EXPECT_FLOAT_EQ(overrides[2].second, 0.0f);
}

TEST(LibrashaderParameters, ParseOverridesSkipsMalformed)
{
  // No '=', value unparseable, name empty: all skipped so malformed entries cannot poison the list.
  const std::vector<std::string> entries = {"valid=1.0", "no_equals", "=2.0", "bad=value",
                                            "also_valid=3.14"};
  const Overrides overrides = ParseOverrides(entries);

  ASSERT_EQ(overrides.size(), 2u);
  EXPECT_EQ(overrides[0].first, "valid");
  EXPECT_FLOAT_EQ(overrides[0].second, 1.0f);
  EXPECT_EQ(overrides[1].first, "also_valid");
  EXPECT_FLOAT_EQ(overrides[1].second, 3.14f);
}

TEST(LibrashaderParameters, FormatOverridesRoundTrips)
{
  // Non-default values only are persisted, so resetting a parameter removes it instead of writing
  // the default back -- which is how a preset's own defaults can change on a pack update without
  // users being pinned to the old ones.
  const Overrides overrides = {{"brightness", 1.5f}, {"contrast", 0.75f}};
  const std::vector<std::string> entries = FormatOverrides(overrides);

  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0], "brightness=1.500000");
  EXPECT_EQ(entries[1], "contrast=0.750000");

  // Round-trip: parse what we just formatted.
  const Overrides parsed = ParseOverrides(entries);
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0].first, "brightness");
  EXPECT_FLOAT_EQ(parsed[0].second, 1.5f);
  EXPECT_EQ(parsed[1].first, "contrast");
  EXPECT_FLOAT_EQ(parsed[1].second, 0.75f);
}

TEST(LibrashaderParameters, FormatOverridesSkipsNamesTheFormatCannotHold)
{
  // ';' separates entries and '=' splits name from value, so a name containing either cannot be
  // written: "beam;width=2.0" would read back as a parameter "beam" plus a malformed entry, and
  // "gamma=out=1.0" as a parameter "gamma" set to nothing parseable. Both would silently apply the
  // wrong value to a *different* parameter, so such names are dropped entirely rather than mangled.
  // No name in the shader pack contains either character; this is defence against a future one.
  const Overrides overrides = {{"beam;width", 2.0f}, {"gamma=out", 1.0f}, {"contrast", 0.75f}};
  const std::vector<std::string> entries = FormatOverrides(overrides);

  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], "contrast=0.750000");
}

TEST(LibrashaderParameters, KeyForPresetIsRelativeToTheShadersRoot)
{
  // The key is the resolved preset's path relative to whichever shaders root it was found in, so
  // the same preset addresses the same overrides whether it shipped with Dolphin or the user
  // installed it.
  const std::string user_root = File::GetUserPath(D_SHADERS_IDX) + "shaders_slang" DIR_SEP;
  EXPECT_EQ(KeyForPreset(user_root + "crt/crt-royale.slangp", "crt/crt-royale"),
            "crt/crt-royale.slangp");

  const std::string sys_root =
      File::GetSysDirectory() + SHADERS_DIR DIR_SEP "shaders_slang" DIR_SEP;
  EXPECT_EQ(KeyForPreset(sys_root + "crt/crt-royale.slangp", "crt/crt-royale"),
            "crt/crt-royale.slangp");
}

TEST(LibrashaderParameters, KeyForPresetFallsBackToTheResolvedName)
{
  // A preset found outside a shaders_slang root, or one that did not resolve at all, keys off the
  // configured name -- resolved through ResolveConfiguredPreset, never the raw setting. A GFX.ini
  // written before shader chains were removed can still hold a ';'-separated list, and keying off
  // that would file the overrides under a string no other code path produces, so nothing would ever
  // read them back.
  EXPECT_EQ(KeyForPreset("", "crt/crt-royale"), "crt/crt-royale");
  EXPECT_EQ(KeyForPreset("", "crt/crt-royale;misc/image-adjustment"), "crt/crt-royale");
  EXPECT_EQ(KeyForPreset(File::GetUserPath(D_SHADERS_IDX) + "loose.slangp", "\tloose "), "loose");
}

namespace
{
// Save writes through Config::GetLayer(LayerType::Base), which Config::Init does not create -- it
// installs CurrentRun and nothing else -- and dereferences it without checking. Config::AddLayer is
// the only public way to install a Base layer and it demands a loader, so this one stores nothing:
// both halves are no-ops, which leaves the layer's contents entirely in the test's hands and, more
// importantly, means ~Layer's unconditional Save() cannot reach a real GFX.ini.
class NullLoader final : public Config::ConfigLayerLoader
{
public:
  NullLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer*) override {}
  void Save(Config::Layer*) override {}
};

constexpr char PRESET_KEY[] = "crt/crt-royale.slangp";

std::optional<std::string> StoredValue(const std::string& preset_relative_path)
{
  return Config::GetAsString(
      Config::Location{Config::System::GFX, "LibrashaderParameters", preset_relative_path});
}
}  // namespace

class LibrashaderParametersStorage : public testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
  }

  void TearDown() override { Config::Shutdown(); }
};

TEST_F(LibrashaderParametersStorage, SaveThenLoadRoundTrips)
{
  Save(PRESET_KEY, {{"brightness", 1.5f}, {"contrast", 0.75f}});

  // Assert the stored string, not just what Load gives back: this is the text that ends up in a
  // user's GFX.ini, so a change to it silently discards every override already on disk. Load and
  // Save agreeing with each other would not catch that.
  const std::optional<std::string> stored = StoredValue(PRESET_KEY);
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(*stored, "brightness=1.500000;contrast=0.750000");

  const Overrides loaded = Load(PRESET_KEY);
  ASSERT_EQ(loaded.size(), 2u);
  EXPECT_EQ(loaded[0].first, "brightness");
  EXPECT_FLOAT_EQ(loaded[0].second, 1.5f);
  EXPECT_EQ(loaded[1].first, "contrast");
  EXPECT_FLOAT_EQ(loaded[1].second, 0.75f);
}

TEST_F(LibrashaderParametersStorage, LoadReturnsEmptyForAPresetWithNothingStored)
{
  // The common case by far: most presets are never edited, and the post-processor calls Load on
  // every chain build regardless.
  EXPECT_TRUE(Load(PRESET_KEY).empty());
  EXPECT_TRUE(Load("").empty());
}

TEST_F(LibrashaderParametersStorage, SavingNothingRemovesTheKeyRatherThanEmptyingIt)
{
  Save(PRESET_KEY, {{"brightness", 1.5f}});
  ASSERT_TRUE(StoredValue(PRESET_KEY).has_value());

  // Resetting every parameter must delete the key. An empty string left behind would be a stale
  // "this preset was customised" marker in the INI, and it is what makes "only non-default values
  // are persisted" true rather than approximately true.
  Save(PRESET_KEY, {});
  EXPECT_FALSE(StoredValue(PRESET_KEY).has_value());
  EXPECT_TRUE(Load(PRESET_KEY).empty());
}

TEST_F(LibrashaderParametersStorage, GenerationAdvancesOnEveryWriteIncludingDeletes)
{
  // The video thread polls this counter once per frame and rebuilds the chain when it moves; it is
  // the whole mechanism by which an edit in the dialog reaches the running game. A delete has to
  // bump it as well, or resetting a parameter would leave the old value on screen until something
  // else forced a rebuild.
  const u32 before = CurrentGeneration();

  Save(PRESET_KEY, {{"brightness", 1.5f}});
  const u32 after_write = CurrentGeneration();
  EXPECT_EQ(after_write, before + 1);

  Save(PRESET_KEY, {});
  EXPECT_EQ(CurrentGeneration(), after_write + 1);
}

// Both Enumerate tests are limited to the two platforms where this repo ships a librashader binary.
// Elsewhere there is nothing to load, so a load failure would be the build's normal state rather
// than a defect, and a test that reports it is noise.
#if defined(_WIN32) || defined(__APPLE__)
// Enumerate against the in-tree fixture preset. On these two platforms the CMake places librashader
// next to the test binary, so load failure is a real bug and must fail the test rather than skip.
// This provides unconditional coverage of Enumerate across the C ABI.
TEST(LibrashaderParameters, EnumerateFixturePreset)
{
  // Locate the fixture using the pattern from PatchAllowlistTest.cpp:43-48.
  std::string test_data_dir = File::GetExeDirectory()
#if defined(__APPLE__)
                              + DIR_SEP "Tests"  // FIXME: Ugly hack.
#endif
                              + DIR_SEP "TestData";
  const std::string preset_path = test_data_dir + DIR_SEP "parameter-fixture.slangp";

  std::vector<ParameterInfo> params;
  std::string error;
  const bool ok = Enumerate(preset_path, &params, &error);

  ASSERT_TRUE(ok) << "Enumerate failed: " << error;
  ASSERT_EQ(params.size(), 2u) << "Fixture declares exactly 2 parameters";

  // Assert against the measured values. test_gamma initial=2.4 (not 2.5) is the assertion that
  // proves the preset override is folded into p.initial, not the #pragma parameter default.
  auto test_gamma = std::find_if(params.begin(), params.end(),
                                 [](const ParameterInfo& p) { return p.name == "test_gamma"; });
  ASSERT_NE(test_gamma, params.end()) << "test_gamma not found";
  // description is the second const char* in libra_preset_param_t, so it is where a struct-layout
  // shift would surface that a name-only check cannot see.
  EXPECT_EQ(test_gamma->description, "Test Gamma");
  EXPECT_FLOAT_EQ(test_gamma->initial, 2.4f);
  EXPECT_FLOAT_EQ(test_gamma->minimum, 1.0f);
  EXPECT_FLOAT_EQ(test_gamma->maximum, 5.0f);
  EXPECT_FLOAT_EQ(test_gamma->step, 0.025f);

  auto test_flag = std::find_if(params.begin(), params.end(),
                                [](const ParameterInfo& p) { return p.name == "test_flag"; });
  ASSERT_NE(test_flag, params.end()) << "test_flag not found";
  EXPECT_EQ(test_flag->description, "Test Flag");
  EXPECT_FLOAT_EQ(test_flag->initial, 0.0f);
  EXPECT_FLOAT_EQ(test_flag->minimum, 0.0f);
  EXPECT_FLOAT_EQ(test_flag->maximum, 1.0f);
  EXPECT_FLOAT_EQ(test_flag->step, 1.0f);
}

// Enumerate against a real preset from the SLANG_PRESET environment variable, matching the gate in
// SlangCompileTest.cpp:888. Generic invariants only; an arbitrary preset would not satisfy
// crt-royale-specific assertions. This skipping by default is not a repeat of the round-1 defect:
// it runs whenever the env var is set, and the coverage it used to provide is now unconditional in
// EnumerateFixturePreset above.
//
// It shares SLANG_PRESET with SlangCompile.RealPresetCompilesAllPasses, which needs no librashader
// and so is useful on every platform. That sharing is why this test sits inside the platform guard:
// otherwise setting the variable to run that test on Linux would fail this one, for no reason a
// reader could guess from the failure.
TEST(LibrashaderParameters, EnumerateRealPresetFromEnv)
{
  const char* preset_path = std::getenv("SLANG_PRESET");
  if (!preset_path)
  {
    GTEST_SKIP() << "SLANG_PRESET not set; skipping real-preset test";
  }

  std::vector<ParameterInfo> params;
  std::string error;
  const bool ok = Enumerate(preset_path, &params, &error);

  ASSERT_TRUE(ok) << "Enumerate failed: " << error;
  EXPECT_FALSE(params.empty()) << "Real preset should declare at least one parameter";

  // Generic invariants: every parameter has a non-empty name and a valid range.
  for (const auto& p : params)
  {
    EXPECT_FALSE(p.name.empty());
    EXPECT_LE(p.minimum, p.initial);
    EXPECT_LE(p.initial, p.maximum);
    EXPECT_GE(p.step, 0.0f);
  }
}
#endif
