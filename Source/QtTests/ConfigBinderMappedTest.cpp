// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <QComboBox>
#include <QSlider>
#include <QString>
#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "NullConfigLoader.h"

namespace
{
enum class Mode
{
  Off = 0,
  SideBySide = 4,
  Anaglyph = 9,
};

const Config::Info<Mode> TEST_MODE{{Config::System::Main, "BinderMapped", "Mode"}, Mode::Off};
const Config::Info<int> TEST_TICKS{{Config::System::Main, "BinderMapped", "Ticks"}, 0};
const Config::Info<u32> TEST_U32{{Config::System::Main, "BinderMapped", "U32"}, 0};
const Config::Info<std::string> TEST_STR{{Config::System::Main, "BinderMapped", "Str"}, ""};

class ConfigBinderMappedTest : public ::testing::Test
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
};

constexpr std::array<Mode, 3> MODE_VALUES{Mode::Off, Mode::SideBySide, Mode::Anaglyph};
}  // namespace

TEST_F(ConfigBinderMappedTest, MappedComboPairsDesignerItemsWithValuesByIndex)
{
  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
  Config::SetBase(TEST_MODE, Mode::Anaglyph);

  ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES});
  EXPECT_EQ(box.currentIndex(), 2);
  EXPECT_EQ(box.count(), 3) << "the .ui items must not be duplicated or replaced";

  box.setCurrentIndex(1);
  EXPECT_EQ(Config::Get(TEST_MODE), Mode::SideBySide) << "saves the value, not the index";
}

TEST_F(ConfigBinderMappedTest, MappedComboSelectsNothingWhenNoValueMatches)
{
  // An unknown mapped value leaves the combo unselected rather than snapping to the first option.
  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
  Config::SetBase(TEST_MODE, static_cast<Mode>(77));

  ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES});
  EXPECT_EQ(box.currentIndex(), -1);
}

TEST_F(ConfigBinderMappedTest, MappedComboPairFormPopulatesTheCombo)
{
  const std::array<std::pair<QString, Mode>, 2> options{
      std::pair{QStringLiteral("Off"), Mode::Off},
      std::pair{QStringLiteral("Anaglyph"), Mode::Anaglyph}};
  QComboBox box;
  Config::SetBase(TEST_MODE, Mode::Anaglyph);

  ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const std::pair<QString, Mode>>{options});

  EXPECT_EQ(box.count(), 2);
  EXPECT_EQ(box.itemText(1), QStringLiteral("Anaglyph"));
  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigBinderMappedTest, U32ComboRoundTripsTheIndex)
{
  QComboBox box;
  box.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
  Config::SetBase(TEST_U32, 2u);

  ConfigWidget::Bind(&box, TEST_U32);
  EXPECT_EQ(box.currentIndex(), 2);

  box.setCurrentIndex(0);
  EXPECT_EQ(Config::Get(TEST_U32), 0u);
}

TEST_F(ConfigBinderMappedTest, StringChoiceTreatsTheOptionTextAsTheData)
{
  const std::array<std::string, 2> options{"Vulkan", "OpenGL"};
  QComboBox box;
  Config::SetBase(TEST_STR, "OpenGL");

  ConfigWidget::BindStringChoice(&box, TEST_STR, std::span<const std::string>{options});
  EXPECT_EQ(box.currentIndex(), 1);

  box.setCurrentIndex(0);
  EXPECT_EQ(Config::Get(TEST_STR), "Vulkan");
}

TEST_F(ConfigBinderMappedTest, StringChoiceSeparatesDisplayTextFromData)
{
  const std::array<std::pair<QString, QString>, 2> options{
      std::pair{QStringLiteral("Vulkan"), QStringLiteral("vulkan")},
      std::pair{QStringLiteral("OpenGL"), QStringLiteral("ogl")}};
  QComboBox box;
  Config::SetBase(TEST_STR, "ogl");

  ConfigWidget::BindStringChoice(&box, TEST_STR,
                                 std::span<const std::pair<QString, QString>>{options});
  EXPECT_EQ(box.currentIndex(), 1);
  EXPECT_EQ(box.itemText(1), QStringLiteral("OpenGL"));

  box.setCurrentIndex(0);
  EXPECT_EQ(Config::Get(TEST_STR), "vulkan") << "saves the data, not the display text";
}

TEST_F(ConfigBinderMappedTest, TickSliderMapsPositionsToArbitraryValues)
{
  constexpr std::array<int, 3> ticks{1, 2, 4};
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 2);
  Config::SetBase(TEST_TICKS, 4);

  ConfigWidget::BindMapped(&slider, TEST_TICKS, std::span<const int>{ticks});
  EXPECT_EQ(slider.minimum(), 0) << "Bind must not overwrite the range from the .ui file";
  EXPECT_EQ(slider.maximum(), 2) << "Bind must not overwrite the range from the .ui file";
  EXPECT_EQ(slider.value(), 2);

  slider.setValue(1);
  EXPECT_EQ(Config::Get(TEST_TICKS), 2);
}

TEST_F(ConfigBinderMappedTest, TickSliderDisablesItselfWhenNoTickMatches)
{
  constexpr std::array<int, 3> ticks{1, 2, 4};
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 2);
  Config::SetBase(TEST_TICKS, 3);

  ConfigWidget::BindMapped(&slider, TEST_TICKS, std::span<const int>{ticks});
  EXPECT_FALSE(slider.isEnabled()) << "an unrepresentable value must not be silently snapped";

  Config::SetBase(TEST_TICKS, 2);
  NotifyConfigChanged();
  EXPECT_TRUE(slider.isEnabled());
  EXPECT_EQ(slider.value(), 1);
}

TEST_F(ConfigBinderMappedTest, ScaledSliderMultipliesThePositionOnSaveAndDividesOnLoad)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 10);
  Config::SetBase(TEST_U32, 400u);

  ConfigWidget::BindScaled(&slider, TEST_U32, 100u);
  EXPECT_EQ(slider.value(), 4);

  slider.setValue(7);
  EXPECT_EQ(Config::Get(TEST_U32), 700u);
}

TEST_F(ConfigBinderMappedTest, ScaledSliderTruncatesNonMultiplesOfScale)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 10);
  Config::SetBase(TEST_U32, 405u);

  ConfigWidget::BindScaled(&slider, TEST_U32, 100u);
  EXPECT_EQ(slider.value(), 4) << "405 / 100 truncates to 4";

  NotifyConfigChanged();
  EXPECT_EQ(Config::Get(TEST_U32), 405u) << "refresh must not write back the truncated value";
}

TEST_F(ConfigBinderMappedTest, LayeredBindingWritesToTheLayerNotTheBase)
{
  Config::Layer layer{Config::LayerType::LocalGame};

  // Mapped combo
  {
    QComboBox box;
    box.addItems(
        {QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
    Config::SetBase(TEST_MODE, Mode::Off);
    ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES}, &layer);
    box.setCurrentIndex(2);
    EXPECT_TRUE(layer.Exists(TEST_MODE.GetLocation()));
    EXPECT_EQ(layer.Get(TEST_MODE), Mode::SideBySide);
    EXPECT_EQ(Config::GetBase(TEST_MODE), Mode::Off) << "base unchanged";
  }

  // String choice
  {
    const std::array<std::string, 2> options{"Vulkan", "OpenGL"};
    QComboBox box;
    Config::SetBase(TEST_STR, "Vulkan");
    ConfigWidget::BindStringChoice(&box, TEST_STR, std::span<const std::string>{options}, &layer);
    box.setCurrentIndex(2);
    EXPECT_TRUE(layer.Exists(TEST_STR.GetLocation()));
    EXPECT_EQ(layer.Get(TEST_STR), "OpenGL");
    EXPECT_EQ(Config::GetBase(TEST_STR), "Vulkan") << "base unchanged";
  }

  // Tick slider
  {
    constexpr std::array<int, 3> ticks{1, 2, 4};
    QSlider slider{Qt::Horizontal};
    slider.setRange(0, 2);
    Config::SetBase(TEST_TICKS, 1);
    ConfigWidget::BindMapped(&slider, TEST_TICKS, std::span<const int>{ticks}, &layer);
    slider.setValue(2);
    EXPECT_TRUE(layer.Exists(TEST_TICKS.GetLocation()));
    EXPECT_EQ(layer.Get(TEST_TICKS), 4);
    EXPECT_EQ(Config::GetBase(TEST_TICKS), 1) << "base unchanged";
  }

  // Scaled slider
  {
    QSlider slider{Qt::Horizontal};
    slider.setRange(0, 10);
    Config::SetBase(TEST_U32, 0u);
    ConfigWidget::BindScaled(&slider, TEST_U32, 100u, &layer);
    slider.setValue(3);
    EXPECT_TRUE(layer.Exists(TEST_U32.GetLocation()));
    EXPECT_EQ(layer.Get(TEST_U32), 300u);
    EXPECT_EQ(Config::GetBase(TEST_U32), 0u) << "base unchanged";
  }
}

TEST_F(ConfigBinderMappedTest, LayeredMappedChoicesNameAndClearTheInheritedValue)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::Layer shipped_game{Config::LayerType::GlobalGame};
  shipped_game.Set(TEST_MODE, Mode::Anaglyph);
  ConfigWidget::Logic::SetFallbackLayer(&layer, &shipped_game);

  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
  ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES}, &layer);

  EXPECT_EQ(box.currentIndex(), 0);
  EXPECT_EQ(box.currentText(), QStringLiteral("Use Global Setting [Anaglyph]"));

  box.setCurrentIndex(2);
  EXPECT_EQ(layer.Get(TEST_MODE), Mode::SideBySide);
  box.setCurrentIndex(0);
  EXPECT_FALSE(layer.Exists(TEST_MODE.GetLocation()));
  EXPECT_EQ(box.currentText(), QStringLiteral("Use Global Setting [Anaglyph]"));

  ConfigWidget::Logic::SetFallbackLayer(&layer, nullptr);
}

TEST_F(ConfigBinderMappedTest, NewBindingsRefreshWithoutWritingBack)
{
  Config::Layer layer{Config::LayerType::LocalGame};

  // Mapped combo
  {
    QComboBox box;
    box.addItems(
        {QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
    Config::SetBase(TEST_MODE, Mode::Anaglyph);
    ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES}, &layer);
    NotifyConfigChanged();
    EXPECT_FALSE(layer.Exists(TEST_MODE.GetLocation())) << "refresh must not plant an override";
  }

  // String choice
  {
    const std::array<std::string, 2> options{"Vulkan", "OpenGL"};
    QComboBox box;
    Config::SetBase(TEST_STR, "OpenGL");
    ConfigWidget::BindStringChoice(&box, TEST_STR, std::span<const std::string>{options}, &layer);
    NotifyConfigChanged();
    EXPECT_FALSE(layer.Exists(TEST_STR.GetLocation()));
  }

  // Tick slider
  {
    constexpr std::array<int, 3> ticks{1, 2, 4};
    QSlider slider{Qt::Horizontal};
    slider.setRange(0, 2);
    Config::SetBase(TEST_TICKS, 4);
    ConfigWidget::BindMapped(&slider, TEST_TICKS, std::span<const int>{ticks}, &layer);
    NotifyConfigChanged();
    EXPECT_FALSE(layer.Exists(TEST_TICKS.GetLocation()));
  }

  // Scaled slider
  {
    QSlider slider{Qt::Horizontal};
    slider.setRange(0, 10);
    Config::SetBase(TEST_U32, 400u);
    ConfigWidget::BindScaled(&slider, TEST_U32, 100u, &layer);
    NotifyConfigChanged();
    EXPECT_FALSE(layer.Exists(TEST_U32.GetLocation()));
  }
}
