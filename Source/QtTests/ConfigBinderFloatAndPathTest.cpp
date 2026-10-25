// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>

#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/FileUtil.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "NullConfigLoader.h"

namespace
{
const Config::Info<float> TEST_FLOAT{{Config::System::Main, "BinderFloat", "Depth"}, 0.0f};
const Config::Info<std::string> TEST_PATH{{Config::System::Main, "BinderFloat", "Path"}, ""};

class ConfigBinderFloatAndPathTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
    m_saved_user_path = File::GetUserPath(D_WIIROOT_IDX);
    m_warnings = 0;
    ConfigWidget::SetPathWarningHandlerForTesting(
        [this](QWidget*, const QString&) { ++m_warnings; });
  }

  void TearDown() override
  {
    ConfigWidget::SetPathWarningHandlerForTesting({});
    File::SetUserPath(D_WIIROOT_IDX, m_saved_user_path);
    Config::Shutdown();
  }

  std::string m_saved_user_path;
  int m_warnings = 0;
};
}  // namespace

TEST_F(ConfigBinderFloatAndPathTest, FloatSliderMapsTheAuthoredPositionRangeOntoTheFloatRange)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 200);
  slider.setTickInterval(7);
  Config::SetBase(TEST_FLOAT, 50.0f);

  const auto handle = ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.0f, 200.0f, 1.0f);

  EXPECT_EQ(handle.range.MaximumPosition(), 200);
  EXPECT_EQ(slider.value(), 50);
  EXPECT_FLOAT_EQ(handle.Value(), 50.0f);
  EXPECT_EQ(slider.tickInterval(), 7);
}

TEST_F(ConfigBinderFloatAndPathTest, FloatSliderRoundTripsThroughTheStep)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 6);
  Config::SetBase(TEST_FLOAT, 1.0f);
  const auto handle = ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.5f, 2.0f, 0.25f);
  EXPECT_EQ(slider.value(), 2) << "(1.0 - 0.5) / 0.25";

  slider.setValue(5);
  EXPECT_FLOAT_EQ(Config::Get(TEST_FLOAT), 1.75f);
  EXPECT_FLOAT_EQ(handle.Value(), 1.75f);
}

TEST_F(ConfigBinderFloatAndPathTest, MirrorFloatValueTracksTheSlider)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 200);
  QLabel label;
  Config::SetBase(TEST_FLOAT, 0.0f);
  const auto handle = ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.0f, 200.0f, 1.0f);

  ConfigWidget::MirrorFloatValue(&label, handle, QStringLiteral("%.0f%%"));
  EXPECT_EQ(label.text(), QStringLiteral("0%")) << "the label is set immediately, not only on move";

  slider.setValue(75);
  EXPECT_EQ(label.text(), QStringLiteral("75%"));
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathShowsTheUserPathWhenConfigIsEmpty)
{
  File::SetUserPath(D_WIIROOT_IDX, "/tmp/dolphin-test-wiiroot");
  Config::SetBase(TEST_PATH, "");
  QLineEdit edit;

  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  EXPECT_EQ(edit.text(), QStringLiteral("/tmp/dolphin-test-wiiroot/"))
      << "an empty config value means 'use the current user path'";
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathPrefersTheConfigValueWhenItIsSet)
{
  File::SetUserPath(D_WIIROOT_IDX, "/tmp/dolphin-test-wiiroot");
  Config::SetBase(TEST_PATH, "/tmp/dolphin-test-configured");
  QLineEdit edit;

  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  EXPECT_EQ(edit.text(), QStringLiteral("/tmp/dolphin-test-configured"));
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathWritesBothConfigAndTheUserPath)
{
  QLineEdit edit;
  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  edit.setText(QStringLiteral("  /tmp/dolphin-test-new  "));
  emit edit.editingFinished();

  EXPECT_EQ(Config::Get(TEST_PATH), "/tmp/dolphin-test-new") << "and it is trimmed";
  EXPECT_EQ(File::GetUserPath(D_WIIROOT_IDX), "/tmp/dolphin-test-new/");
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathRejectsAnEmptyValueAndRestoresTheText)
{
  Config::SetBase(TEST_PATH, "/tmp/dolphin-test-keep");
  QLineEdit edit;
  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);
  const std::string user_path_before = File::GetUserPath(D_WIIROOT_IDX);

  edit.setText(QStringLiteral("   "));
  emit edit.editingFinished();

  EXPECT_EQ(m_warnings, 1);
  EXPECT_EQ(Config::Get(TEST_PATH), "/tmp/dolphin-test-keep");
  EXPECT_EQ(edit.text(), QStringLiteral("/tmp/dolphin-test-keep"));
  EXPECT_EQ(File::GetUserPath(D_WIIROOT_IDX), user_path_before);
}
