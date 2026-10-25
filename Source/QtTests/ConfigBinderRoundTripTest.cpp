// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigBinding.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "NullConfigLoader.h"

namespace
{
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderRoundTrip", "Bool"}, false};
const Config::Info<int> TEST_INT{{Config::System::Main, "BinderRoundTrip", "Int"}, 0};
const Config::Info<std::string> TEST_STRING{{Config::System::Main, "BinderRoundTrip", "String"},
                                            ""};

class ConfigBinderRoundTripTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
  }
  void TearDown() override { Config::Shutdown(); }

  // Bindings refresh on ConfigChangeBroadcaster::Changed, which dolphin-emu drives from
  // Settings::ConfigChanged. Tests drive it directly.
  static void NotifyConfigChanged()
  {
    ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast();
  }
};
}  // namespace

TEST_F(ConfigBinderRoundTripTest, CheckBoxAdoptsTheConfigValueWhenBound)
{
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ASSERT_FALSE(box.isChecked());

  ConfigWidget::Bind(&box, TEST_BOOL);

  EXPECT_TRUE(box.isChecked());
}

TEST_F(ConfigBinderRoundTripTest, CheckBoxFollowsLaterConfigChanges)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);
  ASSERT_FALSE(box.isChecked());

  Config::SetBase(TEST_BOOL, true);
  NotifyConfigChanged();

  EXPECT_TRUE(box.isChecked());
}

TEST_F(ConfigBinderRoundTripTest, TogglingTheCheckBoxWritesConfig)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  box.setChecked(true);

  EXPECT_TRUE(Config::Get(TEST_BOOL));
}

TEST_F(ConfigBinderRoundTripTest, ReverseInvertsBothDirections)
{
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, nullptr, true);
  EXPECT_FALSE(box.isChecked()) << "reverse means a true setting shows as unchecked";

  box.setChecked(true);
  EXPECT_FALSE(Config::Get(TEST_BOOL));
}

TEST_F(ConfigBinderRoundTripTest, LayeredBindingWritesToTheLayerNotTheBase)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  box.setChecked(true);

  EXPECT_TRUE(layer.Exists(TEST_BOOL.GetLocation()));
  EXPECT_TRUE(layer.Get(TEST_BOOL));
  EXPECT_FALSE(Config::GetBase(TEST_BOOL));
}

TEST_F(ConfigBinderRoundTripTest, RefreshingFromConfigDoesNotWriteBack)
{
  // The constructor's RefreshFromConfig() calls setChecked(true), which emits toggled. Without the
  // IsUpdating() guard, OnToggled would write into the empty layer, creating a per-game override
  // that nobody asked for.
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  NotifyConfigChanged();

  EXPECT_EQ(box.checkState(), Qt::PartiallyChecked) << "shows that the base value is inherited";
  EXPECT_TRUE(ConfigWidget::EffectiveChecked(&box)) << "the inherited base value is true";
  EXPECT_FALSE(layer.Exists(TEST_BOOL.GetLocation()))
      << "a refresh must not create a per-game override";
}

TEST_F(ConfigBinderRoundTripTest, LayeredCheckBoxUsesPartialStateForInheritance)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  QCheckBox box;

  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  EXPECT_TRUE(box.isTristate());
  EXPECT_EQ(box.checkState(), Qt::PartiallyChecked);
  EXPECT_FALSE(ConfigWidget::EffectiveChecked(&box));
  EXPECT_TRUE(ConfigWidget::IsInherited(&box));

  box.setCheckState(Qt::Checked);
  EXPECT_TRUE(layer.Get(TEST_BOOL));
  EXPECT_FALSE(ConfigWidget::IsInherited(&box));

  box.setCheckState(Qt::PartiallyChecked);
  EXPECT_FALSE(layer.Exists(TEST_BOOL.GetLocation()));
  EXPECT_FALSE(ConfigWidget::EffectiveChecked(&box));
}

TEST_F(ConfigBinderRoundTripTest, ReverseLayeredCheckBoxReportsTheDisplayedInheritedValue)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;

  ConfigWidget::Bind(&box, TEST_BOOL, &layer, true);

  EXPECT_EQ(box.checkState(), Qt::PartiallyChecked);
  EXPECT_FALSE(ConfigWidget::EffectiveChecked(&box));

  box.setCheckState(Qt::Checked);
  EXPECT_FALSE(layer.Get(TEST_BOOL)) << "checked display writes the reversed false config value";
}

TEST_F(ConfigBinderRoundTripTest, FindBindingReturnsTheAttachedBinding)
{
  QCheckBox unbound;
  EXPECT_EQ(ConfigWidget::FindBinding(&unbound), nullptr);

  QCheckBox bound;
  ConfigWidget::Bind(&bound, TEST_BOOL);
  EXPECT_NE(ConfigWidget::FindBinding(&bound), nullptr);
}

TEST_F(ConfigBinderRoundTripTest, BindingIsDestroyedWhenWidgetIsDestroyed)
{
  auto* box = new QCheckBox;
  ConfigWidget::Bind(box, TEST_BOOL);
  auto* binding = ConfigWidget::FindBinding(box);
  ASSERT_NE(binding, nullptr);
  QPointer<QObject> binding_ptr{qobject_cast<QObject*>(binding)};

  delete box;

  EXPECT_TRUE(binding_ptr.isNull()) << "binding must be destroyed with its widget";
}

TEST_F(ConfigBinderRoundTripTest, ComboBoxRoundTripsTheCurrentIndex)
{
  QComboBox box;
  box.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
  Config::SetBase(TEST_INT, 2);

  ConfigWidget::Bind(&box, TEST_INT);
  EXPECT_EQ(box.currentIndex(), 2);

  box.setCurrentIndex(1);
  EXPECT_EQ(Config::Get(TEST_INT), 1);
}

TEST_F(ConfigBinderRoundTripTest, LayeredComboBoxAddsAndClearsAnInheritedRow)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QComboBox box;
  box.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
  Config::SetBase(TEST_INT, 2);

  ConfigWidget::Bind(&box, TEST_INT, &layer);

  EXPECT_EQ(box.count(), 4);
  EXPECT_EQ(box.currentIndex(), 0);
  EXPECT_EQ(box.currentText(), QStringLiteral("Use Global Setting [c]"));

  box.setCurrentIndex(2);
  EXPECT_EQ(layer.Get(TEST_INT), 1);

  box.setCurrentIndex(0);
  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_EQ(box.currentText(), QStringLiteral("Use Global Setting [c]"));
}

TEST_F(ConfigBinderRoundTripTest, SpinBoxRoundTripsAndKeepsItsDesignerRange)
{
  QSpinBox spin;
  spin.setRange(10, 90);
  Config::SetBase(TEST_INT, 42);

  ConfigWidget::Bind(&spin, TEST_INT);
  EXPECT_EQ(spin.value(), 42);
  EXPECT_EQ(spin.minimum(), 10) << "Bind must not overwrite the range from the .ui file";
  EXPECT_EQ(spin.maximum(), 90);

  spin.setValue(55);
  EXPECT_EQ(Config::Get(TEST_INT), 55);
}

TEST_F(ConfigBinderRoundTripTest, LayeredSpinBoxPrefixesTheInheritedValue)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QSpinBox spin;
  spin.setRange(0, 100);
  Config::SetBase(TEST_INT, 42);

  ConfigWidget::Bind(&spin, TEST_INT, &layer);

  EXPECT_EQ(spin.value(), 42);
  EXPECT_EQ(spin.prefix(), QStringLiteral("Default: "));
  spin.setValue(55);
  EXPECT_EQ(layer.Get(TEST_INT), 55);
  EXPECT_TRUE(spin.prefix().isEmpty());
}

TEST_F(ConfigBinderRoundTripTest, SliderRoundTrips)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 100);
  Config::SetBase(TEST_INT, 30);

  ConfigWidget::Bind(&slider, TEST_INT);
  EXPECT_EQ(slider.value(), 30);

  slider.setValue(70);
  EXPECT_EQ(Config::Get(TEST_INT), 70);
}

TEST_F(ConfigBinderRoundTripTest, LayeredSliderShowsInheritedValueAndEditingCreatesOverride)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 100);
  Config::SetBase(TEST_INT, 30);

  ConfigWidget::Bind(&slider, TEST_INT, &layer);

  EXPECT_EQ(slider.value(), 30);
  EXPECT_TRUE(ConfigWidget::IsInherited(&slider));
  slider.setValue(70);
  EXPECT_EQ(layer.Get(TEST_INT), 70);
  EXPECT_FALSE(ConfigWidget::IsInherited(&slider));
}

TEST_F(ConfigBinderRoundTripTest, RadioButtonChecksOnlyWhenItsValueIsSelected)
{
  QWidget parent;
  auto* const first = new QRadioButton{&parent};
  auto* const second = new QRadioButton{&parent};
  Config::SetBase(TEST_INT, 1);

  ConfigWidget::Bind(first, TEST_INT, 0);
  ConfigWidget::Bind(second, TEST_INT, 1);

  EXPECT_FALSE(first->isChecked());
  EXPECT_TRUE(second->isChecked());
}

TEST_F(ConfigBinderRoundTripTest, CheckingARadioButtonWritesItsOwnValue)
{
  QWidget parent;
  auto* const first = new QRadioButton{&parent};
  auto* const second = new QRadioButton{&parent};
  ConfigWidget::Bind(first, TEST_INT, 0);
  ConfigWidget::Bind(second, TEST_INT, 1);

  second->setChecked(true);
  EXPECT_EQ(Config::Get(TEST_INT), 1);

  // Qt unchecks `second` as part of checking `first`. Only the winner may write.
  first->setChecked(true);
  EXPECT_EQ(Config::Get(TEST_INT), 0);
}

TEST_F(ConfigBinderRoundTripTest, LineEditWritesOnEditingFinishedNotOnEveryKeystroke)
{
  Config::SetBase(TEST_STRING, "before");
  QLineEdit edit;
  ConfigWidget::Bind(&edit, TEST_STRING);
  EXPECT_EQ(edit.text(), QStringLiteral("before"));

  edit.setText(QStringLiteral("partial-p"));
  EXPECT_EQ(Config::Get(TEST_STRING), "before") << "a keystroke must not write config";

  edit.setText(QStringLiteral("after"));
  emit edit.editingFinished();
  EXPECT_EQ(Config::Get(TEST_STRING), "after");
}

TEST_F(ConfigBinderRoundTripTest, ReadOnlyLineEditDoesNotWriteOnEditingFinished)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_STRING, "inherited");
  QLineEdit edit;
  edit.setReadOnly(true);
  ConfigWidget::Bind(&edit, TEST_STRING, &layer);
  ASSERT_EQ(edit.text(), QStringLiteral("inherited"));

  edit.setText(QStringLiteral("display-only"));
  emit edit.editingFinished();

  EXPECT_FALSE(layer.Exists(TEST_STRING.GetLocation()));
  EXPECT_EQ(Config::GetBase(TEST_STRING), "inherited");
}

TEST_F(ConfigBinderRoundTripTest, EveryWidgetTypeRefreshesWithoutWritingBack)
{
  // Same re-entrancy guard as the check box case, for every type.
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 5);
  Config::SetBase(TEST_STRING, "base");

  QComboBox combo;
  combo.addItems({QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"),
                  QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")});
  QSpinBox spin;
  spin.setRange(0, 100);
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 100);
  QLineEdit edit;

  ConfigWidget::Bind(&combo, TEST_INT, &layer);
  ConfigWidget::Bind(&spin, TEST_INT, &layer);
  ConfigWidget::Bind(&slider, TEST_INT, &layer);
  ConfigWidget::Bind(&edit, TEST_STRING, &layer);

  NotifyConfigChanged();

  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_FALSE(layer.Exists(TEST_STRING.GetLocation()));
}
