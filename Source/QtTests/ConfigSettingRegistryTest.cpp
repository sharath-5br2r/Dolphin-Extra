// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>

#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "NullConfigLoader.h"

namespace
{
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderRegistry", "Bool"}, false};
const Config::Info<int> TEST_INT{{Config::System::Main, "BinderRegistry", "Int"}, 0};
const Config::Info<u32> TEST_U32{{Config::System::Main, "BinderRegistry", "U32"}, 0u};
const Config::Info<float> TEST_FLOAT{{Config::System::Main, "BinderRegistry", "Float"}, 0.0f};

class ConfigSettingRegistryTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
    // A process-wide registry, so each test starts from empty.
    ConfigWidget::ConfigSettingRegistry::Instance().ClearForTesting();
  }
  void TearDown() override { Config::Shutdown(); }

  static const ConfigWidget::SettingEntry* Find(const Config::Location& location)
  {
    return ConfigWidget::ConfigSettingRegistry::Instance().Find(location);
  }
};
}  // namespace

TEST_F(ConfigSettingRegistryTest, BindingRecordsTheLocationAndKind)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  const auto* const entry = Find(TEST_BOOL.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Bool);
  EXPECT_FALSE(entry->per_game);
}

TEST_F(ConfigSettingRegistryTest, ABindingWithALayerIsMarkedPerGame)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  ASSERT_NE(Find(TEST_BOOL.GetLocation()), nullptr);
  EXPECT_TRUE(Find(TEST_BOOL.GetLocation())->per_game);
}

TEST_F(ConfigSettingRegistryTest, BindingTheSameSettingTwiceMergesIntoOneEntry)
{
  // The same setting appears in a global pane and a per-game pane, and panes reopen.
  QCheckBox global;
  Config::Layer layer{Config::LayerType::LocalGame};
  QCheckBox per_game;

  ConfigWidget::Bind(&global, TEST_BOOL);
  ConfigWidget::Bind(&per_game, TEST_BOOL, &layer);

  EXPECT_EQ(ConfigWidget::ConfigSettingRegistry::Instance().Entries().size(), 1u);
}

TEST_F(ConfigSettingRegistryTest, SetDescriptionFillsInTheTitleAndDescription)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  ConfigWidget::SetDescription(&box, QStringLiteral("Progressive Scan"),
                               QStringLiteral("Enables 480p output."));

  const auto* const entry = Find(TEST_BOOL.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->title, QStringLiteral("Progressive Scan"));
  EXPECT_EQ(entry->description, QStringLiteral("Enables 480p output."));
}

TEST_F(ConfigSettingRegistryTest, SetDescriptionOnAnUnboundWidgetRecordsNothing)
{
  QCheckBox box;  // Help metadata without a config setting must not create a registry entry.
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));

  EXPECT_TRUE(ConfigWidget::ConfigSettingRegistry::Instance().Entries().empty());
}

TEST_F(ConfigSettingRegistryTest, SpinBoxRangeIsReadOffTheWidget)
{
  QSpinBox spin;
  spin.setRange(10, 90);  // as the .ui file would have set it

  ConfigWidget::Bind(&spin, TEST_INT);

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  ASSERT_TRUE(entry->minimum.has_value());
  EXPECT_DOUBLE_EQ(*entry->minimum, 10.0);
  EXPECT_DOUBLE_EQ(*entry->maximum, 90.0);
}

TEST_F(ConfigSettingRegistryTest, FloatSliderRecordsItsFloatRangeNotItsPositionRange)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 6);  // MaximumPosition for 0.5 to 2.0 with step 0.25

  ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.5f, 2.0f, 0.25f);

  const auto* const entry = Find(TEST_FLOAT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Float);
  ASSERT_TRUE(entry->step.has_value());
  EXPECT_DOUBLE_EQ(*entry->minimum, 0.5);
  EXPECT_DOUBLE_EQ(*entry->maximum, 2.0);
  EXPECT_DOUBLE_EQ(*entry->step, 0.25);
}

TEST_F(ConfigSettingRegistryTest, ScaledSliderRecordsItsScaledRangeNotItsPositionRange)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 10);

  ConfigWidget::BindScaled(&slider, TEST_U32, 5u);

  const auto* const entry = Find(TEST_U32.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::U32);
  ASSERT_TRUE(entry->minimum.has_value());
  EXPECT_DOUBLE_EQ(*entry->minimum, 0.0);
  EXPECT_DOUBLE_EQ(*entry->maximum, 50.0);
  EXPECT_DOUBLE_EQ(*entry->step, 5.0);
}

TEST_F(ConfigSettingRegistryTest, MappedSliderRecordsTickValuesNotPositionRange)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 3);
  const int ticks[] = {10, 50, 100, 200};

  ConfigWidget::BindMapped(&slider, TEST_INT, ticks);

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Int);
  ASSERT_TRUE(entry->minimum.has_value());
  EXPECT_DOUBLE_EQ(*entry->minimum, 10.0);
  EXPECT_DOUBLE_EQ(*entry->maximum, 200.0);
  EXPECT_FALSE(entry->step.has_value());
}

TEST_F(ConfigSettingRegistryTest, ChoiceBindingRecordsTheItemTexts)
{
  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("On")});

  ConfigWidget::Bind(&box, TEST_INT);

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Choice);
  ASSERT_EQ(entry->choices.size(), 2u);
  EXPECT_EQ(entry->choices[1], QStringLiteral("On"));
}

TEST_F(ConfigSettingRegistryTest, MappedComboWithDesignerItemsRecordsTheItemTexts)
{
  QComboBox box;
  box.addItems({QStringLiteral("Low"), QStringLiteral("Medium"), QStringLiteral("High")});
  const int values[] = {1, 5, 10};

  ConfigWidget::BindMapped(&box, TEST_INT, std::span<const int>{values});

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Choice);
  ASSERT_EQ(entry->choices.size(), 3u);
  EXPECT_EQ(entry->choices[0], QStringLiteral("Low"));
  EXPECT_EQ(entry->choices[2], QStringLiteral("High"));
}

TEST_F(ConfigSettingRegistryTest, MappedComboThatPopulatesItselfRecordsTheItemTexts)
{
  QComboBox box;
  const std::pair<QString, int> options[] = {{QStringLiteral("Option A"), 100},
                                             {QStringLiteral("Option B"), 200}};

  ConfigWidget::BindMapped(&box, TEST_INT, std::span<const std::pair<QString, int>>{options});

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Choice);
  ASSERT_EQ(entry->choices.size(), 2u);
  EXPECT_EQ(entry->choices[0], QStringLiteral("Option A"));
  EXPECT_EQ(entry->choices[1], QStringLiteral("Option B"));
}

TEST_F(ConfigSettingRegistryTest, EntriesAreInBindOrder)
{
  QCheckBox box;
  QSpinBox spin;
  ConfigWidget::Bind(&spin, TEST_INT);
  ConfigWidget::Bind(&box, TEST_BOOL);

  const auto entries = ConfigWidget::ConfigSettingRegistry::Instance().Entries();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].location, TEST_INT.GetLocation());
  EXPECT_EQ(entries[1].location, TEST_BOOL.GetLocation());
}
