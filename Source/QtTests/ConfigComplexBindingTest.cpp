// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QCoreApplication>
#include <QMouseEvent>
#include <gtest/gtest.h>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "NullConfigLoader.h"

namespace
{
const Config::Info<bool> TEST_ENABLED{{Config::System::Main, "BinderComplex", "Enabled"}, false};
const Config::Info<int> TEST_MODE{{Config::System::Main, "BinderComplex", "Mode"}, 0};

class ConfigComplexBindingTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
  }
  void TearDown() override { Config::Shutdown(); }

  static void NotifyConfigChanged()
  {
    ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast();
  }

  static void RightClick(QWidget* widget)
  {
    QMouseEvent press{QEvent::MouseButtonPress, QPointF{1, 1},   QPointF{1, 1},
                      Qt::RightButton,          Qt::RightButton, Qt::NoModifier};
    QCoreApplication::sendEvent(widget, &press);
  }

  // The shape all three existing call sites use: an "off" row plus two "on" rows.
  static ConfigWidget::ComplexBinding* BindThreeOptions(QComboBox* box, Config::Layer* layer)
  {
    auto* const binding = ConfigWidget::BindComplex(box, TEST_ENABLED, TEST_MODE, layer);
    binding->Add(QStringLiteral("Off"), false, 0);
    binding->Add(QStringLiteral("On, mode 1"), true, 1);
    binding->Add(QStringLiteral("On, mode 2"), true, 2);
    return binding;
  }
};
}  // namespace

TEST_F(ConfigComplexBindingTest, SelectingAnOptionWritesBothSettings)
{
  QComboBox box;
  BindThreeOptions(&box, nullptr);

  box.setCurrentIndex(2);

  EXPECT_TRUE(Config::Get(TEST_ENABLED));
  EXPECT_EQ(Config::Get(TEST_MODE), 2);
}

TEST_F(ConfigComplexBindingTest, TheIndexReflectsTheCurrentPairOfValues)
{
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 1);
  QComboBox box;
  BindThreeOptions(&box, nullptr);

  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigComplexBindingTest, AddingOptionsDoesNotWriteConfig)
{
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 2);
  QComboBox box;

  BindThreeOptions(&box, nullptr);

  // The upper-bound guard in OnIndexChanged rejects auto-selection emissions while m_options is
  // empty, so config is unchanged.
  EXPECT_TRUE(Config::Get(TEST_ENABLED));
  EXPECT_EQ(Config::Get(TEST_MODE), 2);
}

TEST_F(ConfigComplexBindingTest, ADefaultStateOptionMatchesTheSettingsOwnDefault)
{
  QComboBox box;
  auto* const binding = ConfigWidget::BindComplex(&box, TEST_ENABLED, TEST_MODE, nullptr);
  // The DefaultState row must not be row 0, or the first addItem's auto-selection satisfies this.
  binding->Add(QStringLiteral("On, mode 2"), true, 2);
  binding->Add(QStringLiteral("Auto"), Config::DefaultState{}, Config::DefaultState{});

  // Both settings are untouched, so both hold their defaults and only the DefaultState row matches.
  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigComplexBindingTest, NoMatchingOptionSelectsTheConfiguredDefaultIndex)
{
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 99);
  QComboBox box;
  auto* const binding = BindThreeOptions(&box, nullptr);

  binding->SetDefault(1);

  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigComplexBindingTest, WithoutADefaultIndexNoMatchSelectsNothing)
{
  Config::SetBase(TEST_MODE, 99);
  QComboBox box;
  BindThreeOptions(&box, nullptr);

  EXPECT_EQ(box.currentIndex(), -1);
}

TEST_F(ConfigComplexBindingTest, ResetClearsBothTheItemsAndTheOptions)
{
  QComboBox box;
  auto* const binding = BindThreeOptions(&box, nullptr);
  ASSERT_EQ(box.count(), 3);

  // Ruling 5: pin the index < 0 guard by ensuring Reset doesn't trigger a write.
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 1);
  box.setCurrentIndex(2);  // Writes true, 2

  binding->Reset();

  EXPECT_EQ(box.count(), 0);
  // Settings still hold the values from the last write (index 2) — no write from index -1.
  EXPECT_TRUE(Config::Get(TEST_ENABLED));
  EXPECT_EQ(Config::Get(TEST_MODE), 2);

  binding->Add(QStringLiteral("Only"), false, 0);
  EXPECT_EQ(box.count(), 1);
}

TEST_F(ConfigComplexBindingTest, EitherOverriddenSettingMakesTheComboBold)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QComboBox box;
  BindThreeOptions(&box, &layer);
  EXPECT_FALSE(box.font().bold());

  layer.Set(TEST_MODE.GetLocation(), 2);
  NotifyConfigChanged();

  EXPECT_TRUE(box.font().bold()) << "the second setting alone is enough";
}

TEST_F(ConfigComplexBindingTest, RightClickClearsBothKeys)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QComboBox box;
  BindThreeOptions(&box, &layer);
  box.setCurrentIndex(2);
  ASSERT_TRUE(layer.Exists(TEST_ENABLED.GetLocation()));
  ASSERT_TRUE(layer.Exists(TEST_MODE.GetLocation()));

  RightClick(&box);

  EXPECT_FALSE(layer.Exists(TEST_ENABLED.GetLocation()));
  EXPECT_FALSE(layer.Exists(TEST_MODE.GetLocation()));
}

TEST_F(ConfigComplexBindingTest, PerGameReadShowsTheInheritedGlobalRow)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 2);
  QComboBox box;

  auto* const binding = ConfigWidget::BindComplex(&box, TEST_ENABLED, TEST_MODE, &layer);
  binding->Add(QStringLiteral("On, mode 2"), true, 2);
  binding->Add(QStringLiteral("Off"), false, 0);

  EXPECT_EQ(box.currentIndex(), 0);
  EXPECT_EQ(box.currentText(), QStringLiteral("Use Global Setting [On, mode 2]"));

  box.setCurrentIndex(2);
  EXPECT_FALSE(layer.Get(TEST_ENABLED));
  EXPECT_EQ(layer.Get(TEST_MODE), 0);
  box.setCurrentIndex(0);
  EXPECT_FALSE(layer.Exists(TEST_ENABLED.GetLocation()));
  EXPECT_FALSE(layer.Exists(TEST_MODE.GetLocation()));
}

TEST_F(ConfigComplexBindingTest, GetLocationsReturnsTheTwoLocationsInSetting1Setting2Order)
{
  QComboBox box;
  auto* const binding = ConfigWidget::BindComplex(&box, TEST_ENABLED, TEST_MODE, nullptr);
  binding->Add(QStringLiteral("Off"), false, 0);

  const auto [loc1, loc2] = binding->GetLocations();

  EXPECT_EQ(loc1, TEST_ENABLED.GetLocation());
  EXPECT_EQ(loc2, TEST_MODE.GetLocation());
}

TEST_F(ConfigComplexBindingTest, SettingIndexToMinusOneDoesNotWrite)
{
  QComboBox box;
  BindThreeOptions(&box, nullptr);
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 1);
  box.setCurrentIndex(2);  // Writes true, 2
  ASSERT_TRUE(Config::Get(TEST_ENABLED));
  ASSERT_EQ(Config::Get(TEST_MODE), 2);

  box.setCurrentIndex(-1);

  // Settings still hold what was last written — the index < 0 guard prevented the write.
  EXPECT_TRUE(Config::Get(TEST_ENABLED));
  EXPECT_EQ(Config::Get(TEST_MODE), 2);
}
