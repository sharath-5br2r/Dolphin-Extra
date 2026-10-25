// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"

namespace
{
const Config::Info<int> TEST_INT{{Config::System::Main, "BinderTest", "Int"}, 7};
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderTest", "Bool"}, false};

class NullLoader final : public Config::ConfigLayerLoader
{
public:
  NullLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer*) override {}
  void Save(Config::Layer*) override {}
};

class ConfigBindingLogicTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
  }
  void TearDown() override { Config::Shutdown(); }
};
}  // namespace

TEST_F(ConfigBindingLogicTest, GlobalReadFallsBackToDefault)
{
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, nullptr), 7);
}

TEST_F(ConfigBindingLogicTest, GlobalWriteThenReadRoundTrips)
{
  ConfigWidget::Logic::WriteValue(TEST_INT, TEST_INT.GetLocation(), nullptr, 42);
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, nullptr), 42);
}

TEST_F(ConfigBindingLogicTest, LayerReadFallsBackToBaseWhenKeyAbsent)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 11);
  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 11);
}

TEST_F(ConfigBindingLogicTest, LayerReadPrefersShippedGameFallbackBeforeBase)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::Layer shipped_game{Config::LayerType::GlobalGame};
  Config::SetBase(TEST_INT, 11);
  shipped_game.Set(TEST_INT, 23);
  ConfigWidget::Logic::SetFallbackLayer(&layer, &shipped_game);

  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 23);

  ConfigWidget::Logic::SetFallbackLayer(&layer, nullptr);
}

TEST_F(ConfigBindingLogicTest, LocalValueWinsOverShippedGameFallback)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::Layer shipped_game{Config::LayerType::GlobalGame};
  Config::SetBase(TEST_INT, 11);
  shipped_game.Set(TEST_INT, 23);
  layer.Set(TEST_INT, 37);
  ConfigWidget::Logic::SetFallbackLayer(&layer, &shipped_game);

  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 37);

  ConfigWidget::Logic::SetFallbackLayer(&layer, nullptr);
}

TEST_F(ConfigBindingLogicTest, LayerReadPrefersLayerValueWhenKeyPresent)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 11);
  ConfigWidget::Logic::WriteValue(TEST_INT, TEST_INT.GetLocation(), &layer, 99);
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 99);
  EXPECT_EQ(Config::GetBase(TEST_INT), 11) << "writing to a layer must not touch the base layer";
}

TEST_F(ConfigBindingLogicTest, IsLocalIsFalseForUntouchedGlobalSetting)
{
  EXPECT_FALSE(ConfigWidget::Logic::IsLocal(TEST_BOOL.GetLocation(), nullptr));
}

TEST_F(ConfigBindingLogicTest, IsLocalIsTrueOnceLayerKeyExists)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  EXPECT_FALSE(ConfigWidget::Logic::IsLocal(TEST_BOOL.GetLocation(), &layer));
  ConfigWidget::Logic::WriteValue(TEST_BOOL, TEST_BOOL.GetLocation(), &layer, true);
  EXPECT_TRUE(ConfigWidget::Logic::IsLocal(TEST_BOOL.GetLocation(), &layer));
}

TEST_F(ConfigBindingLogicTest, ClearLocalRemovesTheLayerKeyAndRestoresBase)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 11);
  ConfigWidget::Logic::WriteValue(TEST_INT, TEST_INT.GetLocation(), &layer, 99);
  ConfigWidget::Logic::ClearLocal(TEST_INT.GetLocation(), &layer);
  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 11);
}

TEST_F(ConfigBindingLogicTest, ClearLocalOnGlobalBindingIsANoOp)
{
  Config::SetBase(TEST_INT, 11);
  ConfigWidget::Logic::ClearLocal(TEST_INT.GetLocation(), nullptr);
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, nullptr), 11);
}
