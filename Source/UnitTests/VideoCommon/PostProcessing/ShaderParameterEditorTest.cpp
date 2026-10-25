// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "VideoCommon/PostProcessing/LibrashaderParameters.h"
#include "VideoCommon/PostProcessing/ShaderParameterEditor.h"

using VideoCommon::GetParameterEditingAvailability;
using VideoCommon::IsParameterEditingBackend;
using VideoCommon::MakeSliderMapping;
using VideoCommon::ParameterEditingAvailability;
using VideoCommon::SliderMapping;
using VideoCommon::SliderPositionFor;
using VideoCommon::SliderValueAt;
using VideoCommon::LibrashaderParameters::ParameterInfo;

namespace
{
ParameterInfo MakeInfo(float initial, float minimum, float maximum, float step)
{
  ParameterInfo info;
  info.name = "param";
  info.description = "Parameter";
  info.initial = initial;
  info.minimum = minimum;
  info.maximum = maximum;
  info.step = step;
  return info;
}
}  // namespace

TEST(SliderMapping, WholeStepsGetOnePositionEach)
{
  const ParameterInfo info = MakeInfo(0.5f, 0.0f, 1.0f, 0.05f);
  const SliderMapping mapping = MakeSliderMapping(info);

  EXPECT_EQ(mapping.positions, 20);
  EXPECT_FLOAT_EQ(mapping.increment, 0.05f);
  EXPECT_FLOAT_EQ(SliderValueAt(info, mapping, 0), 0.0f);
  EXPECT_FLOAT_EQ(SliderValueAt(info, mapping, 3), 0.15f);
}

TEST(SliderMapping, CapsAtTenThousandAndStretchesIncrement)
{
  // A million whole steps would make a slider whose positions cannot be told apart; the cap keeps
  // the slider usable and stretches each position across several steps instead.
  const ParameterInfo info = MakeInfo(0.0f, 0.0f, 1000000.0f, 1.0f);
  const SliderMapping mapping = MakeSliderMapping(info);

  EXPECT_EQ(mapping.positions, VideoCommon::MAX_SLIDER_POSITIONS);
  EXPECT_EQ(VideoCommon::MAX_SLIDER_POSITIONS, 10000);
  EXPECT_FLOAT_EQ(mapping.increment, 100.0f);
}

TEST(SliderMapping, DegenerateRangesGetNoSlider)
{
  // No usable positions: these get the numeric field alone.
  EXPECT_EQ(MakeSliderMapping(MakeInfo(0.0f, 0.0f, 1.0f, 0.0f)).positions, 0);   // zero step
  EXPECT_EQ(MakeSliderMapping(MakeInfo(0.0f, 0.0f, 1.0f, -0.1f)).positions, 0);  // negative step
  EXPECT_EQ(MakeSliderMapping(MakeInfo(1.0f, 1.0f, 1.0f, 0.1f)).positions, 0);   // empty range
  EXPECT_EQ(MakeSliderMapping(MakeInfo(0.0f, 1.0f, 0.0f, 0.1f)).positions, 0);   // inverted
  EXPECT_EQ(MakeSliderMapping(MakeInfo(0.0f, 0.0f, 0.4f, 1.0f)).positions, 0);   // under a step
}

TEST(SliderMapping, NonMultipleRangeEndsExactlyAtMaximum)
{
  // 0..1 in steps of 0.3 rounds to 3 whole steps. Position 3 would be 0.9 by arithmetic; the last
  // position is the maximum exactly so the slider can always reach it.
  const ParameterInfo info = MakeInfo(0.0f, 0.0f, 1.0f, 0.3f);
  const SliderMapping mapping = MakeSliderMapping(info);

  ASSERT_EQ(mapping.positions, 3);
  EXPECT_FLOAT_EQ(SliderValueAt(info, mapping, 2), 0.6f);
  EXPECT_FLOAT_EQ(SliderValueAt(info, mapping, 3), 1.0f);
}

TEST(SliderMapping, PositionNearestTheDefaultIsTheDefault)
{
  // minimum + p * increment accumulates float noise on wide ranges; the position nearest the
  // default must yield the default bit-for-bit, or Reset stays enabled at the default position.
  const ParameterInfo info = MakeInfo(0.7f, -3.0f, 3.0f, 0.1f);
  const SliderMapping mapping = MakeSliderMapping(info);

  const int position = SliderPositionFor(info, mapping, info.initial);
  EXPECT_EQ(position, 37);
  EXPECT_EQ(SliderValueAt(info, mapping, position), 0.7f);
}

TEST(SliderMapping, PositionForValueRoundsAndClamps)
{
  const ParameterInfo info = MakeInfo(0.5f, 0.0f, 1.0f, 0.1f);
  const SliderMapping mapping = MakeSliderMapping(info);

  EXPECT_EQ(SliderPositionFor(info, mapping, 0.34f), 3);
  EXPECT_EQ(SliderPositionFor(info, mapping, 0.36f), 4);
  EXPECT_EQ(SliderPositionFor(info, mapping, -5.0f), 0);
  EXPECT_EQ(SliderPositionFor(info, mapping, 5.0f), 10);
}

TEST(SliderMapping, OutOfRangePositionsClampToTheEnds)
{
  const ParameterInfo info = MakeInfo(0.5f, 0.0f, 1.0f, 0.1f);
  const SliderMapping mapping = MakeSliderMapping(info);

  EXPECT_FLOAT_EQ(SliderValueAt(info, mapping, -1), 0.0f);
  EXPECT_FLOAT_EQ(SliderValueAt(info, mapping, 11), 1.0f);
}

TEST(ParameterEditingAvailability, BackendsInScopeAreVulkanAndDirect3D)
{
  EXPECT_TRUE(IsParameterEditingBackend("Vulkan"));
  EXPECT_TRUE(IsParameterEditingBackend("D3D"));
  EXPECT_TRUE(IsParameterEditingBackend("D3D12"));

  // OGL is GLES on Android, which librashader's GL runtime cannot drive.
  EXPECT_FALSE(IsParameterEditingBackend("OGL"));
  EXPECT_FALSE(IsParameterEditingBackend("Metal"));
  EXPECT_FALSE(IsParameterEditingBackend("Software Renderer"));
  EXPECT_FALSE(IsParameterEditingBackend("Null"));
  EXPECT_FALSE(IsParameterEditingBackend(""));
  EXPECT_FALSE(IsParameterEditingBackend("vulkan"));
}

TEST(ParameterEditingAvailability, ReportsTheFirstUnmetRequirement)
{
  // Order is what the user can act on first: pick a preset, then (rarely) a device without the
  // library, then switch backends.
  EXPECT_EQ(GetParameterEditingAvailability(false, "OGL", ""),
            ParameterEditingAvailability::NoPreset);
  EXPECT_EQ(GetParameterEditingAvailability(false, "OGL", "crt/crt-royale"),
            ParameterEditingAvailability::LibraryUnavailable);
  EXPECT_EQ(GetParameterEditingAvailability(true, "OGL", "crt/crt-royale"),
            ParameterEditingAvailability::UnsupportedBackend);
  EXPECT_EQ(GetParameterEditingAvailability(true, "Vulkan", "crt/crt-royale"),
            ParameterEditingAvailability::Available);
}

TEST(ParameterEditingAvailability, ALegacyChainCountsAsItsFirstPreset)
{
  // GFX.ini can still hold a ';'-separated chain; only its first entry loads, and a chain whose
  // first entry is blank loads nothing.
  EXPECT_EQ(GetParameterEditingAvailability(true, "Vulkan", "crt/a;crt/b"),
            ParameterEditingAvailability::Available);
  EXPECT_EQ(GetParameterEditingAvailability(true, "Vulkan", " ;crt/b"),
            ParameterEditingAvailability::NoPreset);
}

namespace
{
using VideoCommon::ShaderParameterEditor;
using VideoCommon::LibrashaderParameters::Overrides;

constexpr char EDITOR_KEY[] = "crt/crt-test.slangp";

ParameterInfo MakeNamedInfo(std::string name, float initial, float minimum, float maximum,
                            float step)
{
  ParameterInfo info = MakeInfo(initial, minimum, maximum, step);
  info.name = std::move(name);
  info.description = info.name + " description";
  return info;
}

std::vector<ParameterInfo> ThreeParameters()
{
  return {MakeNamedInfo("brightness", 1.0f, 0.0f, 2.0f, 0.05f),
          MakeNamedInfo("mask_type", 1.0f, 0.0f, 3.0f, 1.0f),
          MakeNamedInfo("gamma", 2.4f, 1.0f, 3.0f, 0.01f)};
}
}  // namespace

TEST(ShaderParameterEditor, RowsStartAtTheirDefaults)
{
  const ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});

  EXPECT_EQ(editor.GetStatus(), ShaderParameterEditor::Status::Ok);
  EXPECT_EQ(editor.GetKey(), EDITOR_KEY);
  ASSERT_EQ(editor.GetRows().size(), 3u);
  EXPECT_EQ(editor.GetRows()[0].info.name, "brightness");
  EXPECT_EQ(editor.GetRows()[0].value, 1.0f);
  EXPECT_EQ(editor.GetRows()[1].slider.positions, 3);
  EXPECT_EQ(editor.GetRows()[2].value, 2.4f);
  for (size_t i = 0; i < editor.GetRows().size(); ++i)
    EXPECT_TRUE(editor.IsDefault(i));
}

TEST(ShaderParameterEditor, StoredValuesApplyAndUnknownNamesAreIgnored)
{
  // A stored name the preset no longer declares (a pack update renamed it) is ignored, not an
  // error.
  const ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(),
                                     Overrides{{"gamma", 2.2f}, {"retired_param", 9.0f}});

  EXPECT_FLOAT_EQ(editor.GetRows()[2].value, 2.2f);
  EXPECT_FALSE(editor.IsDefault(2));
  EXPECT_TRUE(editor.IsDefault(0));
  EXPECT_EQ(editor.GetSliderPosition(2), 120);
}

TEST(ShaderParameterEditor, OutOfRangeStoredValuesAreClamped)
{
  const ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(),
                                     Overrides{{"brightness", 7.5f}, {"mask_type", -2.0f}});

  EXPECT_FLOAT_EQ(editor.GetRows()[0].value, 2.0f);
  EXPECT_FLOAT_EQ(editor.GetRows()[1].value, 0.0f);
}

TEST(ShaderParameterEditor, StoredValuesForUnrangedParametersAreKept)
{
  // No declared range means nothing to clamp to.
  const ShaderParameterEditor editor(
      EDITOR_KEY, {MakeNamedInfo("free", 0.0f, 0.0f, 0.0f, 0.0f)}, Overrides{{"free", 42.0f}});

  EXPECT_FLOAT_EQ(editor.GetRows()[0].value, 42.0f);
  EXPECT_EQ(editor.GetRows()[0].slider.positions, 0);
  EXPECT_EQ(editor.GetSliderPosition(0), 0);
}

TEST(ShaderParameterEditor, AnEmptyListMeansNoParameters)
{
  const ShaderParameterEditor editor(EDITOR_KEY, {}, {});

  EXPECT_EQ(editor.GetStatus(), ShaderParameterEditor::Status::NoParameters);
  EXPECT_TRUE(editor.GetRows().empty());
}

TEST(ShaderParameterEditor, FailedCarriesTheReasonAndNoRows)
{
  const auto editor = ShaderParameterEditor::Failed(EDITOR_KEY, "preset not found");

  EXPECT_EQ(editor->GetStatus(), ShaderParameterEditor::Status::Error);
  EXPECT_EQ(editor->GetError(), "preset not found");
  EXPECT_TRUE(editor->GetRows().empty());
}

TEST(ShaderParameterEditor, FormatValueUsesTheStepsPrecisionAndAPoint)
{
  // Always '.', whatever the process locale: the Android side shows this text directly.
  const ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(),
                                     Overrides{{"brightness", 1.25f}, {"gamma", 2.2f}});

  EXPECT_EQ(editor.FormatValue(0), "1.25");
  EXPECT_EQ(editor.FormatValue(1), "1");
  EXPECT_EQ(editor.FormatValue(2), "2.20");
}

namespace
{
// Same arrangement as LibrashaderParametersStorage: a Base layer whose loader stores nothing, so
// the layer's contents are the test's and ~Layer's Save() cannot reach a real GFX.ini.
class NullLoader final : public Config::ConfigLayerLoader
{
public:
  NullLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer*) override {}
  void Save(Config::Layer*) override {}
};

std::optional<std::string> StoredValue()
{
  return Config::GetAsString(
      Config::Location{Config::System::GFX, "LibrashaderParameters", EDITOR_KEY});
}
}  // namespace

class ShaderParameterEditorStorage : public testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
  }

  void TearDown() override { Config::Shutdown(); }
};

TEST_F(ShaderParameterEditorStorage, SetValuePersistsOnlyNonDefaults)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});

  EXPECT_TRUE(editor.SetValue(2, 2.2f));
  EXPECT_FLOAT_EQ(editor.GetRows()[2].value, 2.2f);
  EXPECT_EQ(StoredValue(), "gamma=2.200000");
}

TEST_F(ShaderParameterEditorStorage, SetValueClampsToTheDeclaredRange)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});

  EXPECT_TRUE(editor.SetValue(0, 9.0f));
  EXPECT_FLOAT_EQ(editor.GetRows()[0].value, 2.0f);
  EXPECT_TRUE(editor.SetValue(0, -9.0f));
  EXPECT_FLOAT_EQ(editor.GetRows()[0].value, 0.0f);
}

TEST_F(ShaderParameterEditorStorage, AnUnchangedValueDoesNotWrite)
{
  // Every write bumps the generation and makes a running chain push all of its parameters; an
  // edit that changes nothing must not cost that.
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});
  const u32 generation = VideoCommon::LibrashaderParameters::CurrentGeneration();

  EXPECT_FALSE(editor.SetValue(0, 1.0f));
  EXPECT_FALSE(editor.SetValue(0, 1.0f + 1e-8f));
  EXPECT_TRUE(editor.SetValue(0, 5.0f));   // clamps to 2.0: the one write
  EXPECT_FALSE(editor.SetValue(0, 9.0f));  // clamps to 2.0 again: nothing changed
  EXPECT_EQ(VideoCommon::LibrashaderParameters::CurrentGeneration(), generation + 1);
}

TEST_F(ShaderParameterEditorStorage, SetSliderPositionMapsThroughTheSlider)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});

  EXPECT_TRUE(editor.SetSliderPosition(0, 5));
  EXPECT_FLOAT_EQ(editor.GetRows()[0].value, 0.25f);
  EXPECT_EQ(editor.GetSliderPosition(0), 5);
  EXPECT_TRUE(editor.SetSliderPosition(0, 40));
  EXPECT_EQ(editor.GetRows()[0].value, 2.0f);
  // Back to the default position lands on the default exactly, so nothing stays stored.
  EXPECT_TRUE(editor.SetSliderPosition(0, 20));
  EXPECT_TRUE(editor.IsDefault(0));
  EXPECT_EQ(StoredValue(), std::nullopt);
}

TEST_F(ShaderParameterEditorStorage, ResetRemovesTheEntry)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});
  editor.SetValue(0, 1.5f);
  editor.SetValue(2, 2.0f);

  EXPECT_TRUE(editor.Reset(0));
  EXPECT_EQ(editor.GetRows()[0].value, 1.0f);
  EXPECT_EQ(StoredValue(), "gamma=2.000000");
  EXPECT_FALSE(editor.Reset(0));
}

TEST_F(ShaderParameterEditorStorage, ResetAllRemovesTheKey)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), Overrides{{"mask_type", 2.0f}});
  editor.SetValue(0, 1.5f);

  EXPECT_TRUE(editor.ResetAll());
  for (size_t i = 0; i < editor.GetRows().size(); ++i)
    EXPECT_TRUE(editor.IsDefault(i));
  EXPECT_EQ(StoredValue(), std::nullopt);
  EXPECT_FALSE(editor.ResetAll());
}

TEST_F(ShaderParameterEditorStorage, CollectOverridesKeepsPresetOrder)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), {});
  editor.SetValue(2, 2.0f);
  editor.SetValue(0, 0.5f);

  const Overrides overrides = editor.CollectOverrides();
  ASSERT_EQ(overrides.size(), 2u);
  EXPECT_EQ(overrides[0].first, "brightness");
  EXPECT_EQ(overrides[1].first, "gamma");
  EXPECT_EQ(StoredValue(), "brightness=0.500000;gamma=2.000000");
}

TEST_F(ShaderParameterEditorStorage, AClampedStoredValueIsWhatTheNextEditPersists)
{
  ShaderParameterEditor editor(EDITOR_KEY, ThreeParameters(), Overrides{{"brightness", 7.5f}});
  editor.SetValue(2, 2.0f);

  EXPECT_EQ(StoredValue(), "brightness=2.000000;gamma=2.000000");
}

TEST(ShaderParameterEditor, OpenWithNothingConfiguredFails)
{
  const auto editor = ShaderParameterEditor::Open("");

  EXPECT_EQ(editor->GetStatus(), ShaderParameterEditor::Status::Error);
  EXPECT_FALSE(editor->GetError().empty());
}
