// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "NullConfigLoader.h"

namespace
{
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderOverride", "Bool"}, false};

class ClickRecorder final : public QCheckBox
{
public:
  int presses = 0;

protected:
  void mousePressEvent(QMouseEvent* event) override
  {
    ++presses;
    QCheckBox::mousePressEvent(event);
  }
};

class ConfigBinderOverrideTest : public ::testing::Test
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
};
}  // namespace

TEST_F(ConfigBinderOverrideTest, LayeredOverrideShowsBoldAndClearingRemovesIt)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  ClickRecorder box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  EXPECT_FALSE(box.font().bold());

  box.setChecked(true);  // creates the per-game override
  NotifyConfigChanged();
  EXPECT_TRUE(box.font().bold());

  RightClick(&box);
  EXPECT_FALSE(layer.Exists(TEST_BOOL.GetLocation()));
  EXPECT_FALSE(box.font().bold());
  EXPECT_EQ(box.checkState(), Qt::PartiallyChecked);
  EXPECT_FALSE(ConfigWidget::EffectiveChecked(&box)) << "the widget falls back to the base value";
  EXPECT_EQ(box.presses, 0) << "swallowed";
}

TEST_F(ConfigBinderOverrideTest, ShippedGameFallbackUsesItalicAlongsideLocalBold)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::Layer shipped_game{Config::LayerType::GlobalGame};
  shipped_game.Set(TEST_BOOL, true);
  ConfigWidget::Logic::SetFallbackLayer(&layer, &shipped_game);
  QCheckBox box;
  QLabel label{QStringLiteral("Setting:")};

  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  ConfigWidget::MirrorFont(&label, &box);

  EXPECT_TRUE(box.font().italic());
  EXPECT_TRUE(label.font().italic());
  EXPECT_FALSE(box.font().bold());
  EXPECT_TRUE(ConfigWidget::EffectiveChecked(&box));

  box.setCheckState(Qt::Unchecked);
  NotifyConfigChanged();
  EXPECT_TRUE(box.font().italic());
  EXPECT_TRUE(box.font().bold());

  ConfigWidget::Logic::SetFallbackLayer(&layer, nullptr);
}

TEST_F(ConfigBinderOverrideTest, GlobalBindingIsBoldWhenANonBaseLayerIsActive)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);
  EXPECT_FALSE(box.font().bold());

  Config::SetCurrent(TEST_BOOL, true);
  NotifyConfigChanged();
  EXPECT_TRUE(box.font().bold()) << "the active layer is no longer Base";
}

TEST_F(ConfigBinderOverrideTest, RightClickOnAGlobalBindingIsForwardedNotSwallowed)
{
  Config::SetBase(TEST_BOOL, true);
  ClickRecorder box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  RightClick(&box);

  // Nothing to clear without a layer, and the widget must still get its own right-click so context
  // menus keep working.
  EXPECT_TRUE(Config::Get(TEST_BOOL));
  EXPECT_TRUE(box.isChecked());
  EXPECT_EQ(box.presses, 1) << "forwarded";
}

TEST_F(ConfigBinderOverrideTest, RightClickOnADisabledControlIsIgnored)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  layer.Set(TEST_BOOL.GetLocation(), true);
  ClickRecorder box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  EXPECT_TRUE(box.font().bold()) << "override exists";

  box.setEnabled(false);
  RightClick(&box);

  EXPECT_TRUE(layer.Exists(TEST_BOOL.GetLocation())) << "override not cleared";
  EXPECT_TRUE(box.font().bold()) << "font still bold";
  // The filter forwards the event (returns false), but QWidget::event() still drops it, so
  // mousePressEvent never runs. The important assertion is that the override still exists.
  EXPECT_EQ(box.presses, 0);
}

TEST_F(ConfigBinderOverrideTest, MirroredLabelFollowsTheControlsFont)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  QCheckBox box;
  QLabel label{QStringLiteral("Setting:")};
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  ConfigWidget::MirrorFont(&label, &box);
  EXPECT_FALSE(label.font().bold());

  box.setChecked(true);
  NotifyConfigChanged();

  EXPECT_TRUE(box.font().bold());
  EXPECT_TRUE(label.font().bold()) << "the label marks the override alongside its control";

  RightClick(&box);
  EXPECT_FALSE(label.font().bold()) << "label un-bolds when override cleared";
}

TEST_F(ConfigBinderOverrideTest, MirroredLabelIsBoldImmediatelyWhenAlreadyOverridden)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  layer.Set(TEST_BOOL.GetLocation(), true);
  QCheckBox box;
  QLabel label{QStringLiteral("Setting:")};
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  ConfigWidget::MirrorFont(&label, &box);

  EXPECT_TRUE(label.font().bold()) << "MirrorFont applies the current state, not just future ones";
}

TEST_F(ConfigBinderOverrideTest, MirroredLabelSurvivesItsControlBeingDestroyed)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  layer.Set(TEST_BOOL.GetLocation(), true);
  QLabel label{QStringLiteral("Setting:")};
  {
    QCheckBox box;
    ConfigWidget::Bind(&box, TEST_BOOL, &layer);
    ConfigWidget::MirrorFont(&label, &box);
    EXPECT_TRUE(label.font().bold()) << "override is active";
  }

  // Primary subject: absence of use-after-free when broadcast reaches a binding whose widget died.
  // Secondary: the label's font is unchanged.
  NotifyConfigChanged();  // must not touch the destroyed control or the freed binding
  EXPECT_TRUE(label.font().bold()) << "label font unchanged after control destroyed";
}

TEST_F(ConfigBinderOverrideTest, MirrorSurvivesItsLabelBeingDestroyed)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  layer.Set(TEST_BOOL.GetLocation(), true);
  QCheckBox box;
  QLabel surviving_label{QStringLiteral("Surviving:")};
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  ConfigWidget::MirrorFont(&surviving_label, &box);
  {
    QLabel* dying_label = new QLabel{QStringLiteral("Dying:")};
    ConfigWidget::MirrorFont(dying_label, &box);
    EXPECT_TRUE(dying_label->font().bold());
    delete dying_label;
  }

  // Primary subject: QPointer null guard at ConfigBinding.cpp:62-64. Without it, this is a
  // heap-use-after-free in QWidget::font(). Secondary: surviving label still updates correctly.
  NotifyConfigChanged();
  EXPECT_TRUE(surviving_label.font().bold()) << "surviving label still correct";
}
