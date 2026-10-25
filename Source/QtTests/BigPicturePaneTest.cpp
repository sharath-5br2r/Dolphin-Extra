// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/Config/UISettings.h"
#include "DolphinQt/Settings/BigPicturePane.h"
#include "InputCommon/BigPictureInput.h"
#include "InputCommon/InputConfig.h"
#include "NullConfigLoader.h"

class BigPicturePaneTest : public testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(BigPictureInput::GetConfig()->ControllersNeedToBeCreated());
    previous_config_path = File::GetUserPath(D_CONFIG_IDX);
    File::SetUserPath(D_CONFIG_IDX, directory.path().toStdString() + "/");
    for (int controller = 0; controller < BigPictureInput::CONTROLLER_COUNT; ++controller)
      BigPictureInput::GetConfig()->CreateController<BigPictureInput::MenuController>(controller);
  }
  void TearDown() override
  {
    BigPictureInput::GetConfig()->ClearControllers();
    File::SetUserPath(D_CONFIG_IDX, previous_config_path);
    Config::Shutdown();
  }
  QTemporaryDir directory;
  std::string previous_config_path;
};

TEST_F(BigPicturePaneTest, ConfigureButtonsRequestTheMatchingInputEditor)
{
  BigPicturePane pane;
  std::vector<int> requests;
  QObject::connect(&pane, &BigPicturePane::ConfigureInput, &pane,
                   [&](int controller) { requests.push_back(controller); });

  for (int controller = 0; controller < 4; ++controller)
  {
    auto* button = pane.findChild<QPushButton*>(
        QStringLiteral("configureController%1Button").arg(controller + 1));
    ASSERT_NE(button, nullptr);
    button->click();
  }
  EXPECT_EQ(requests, (std::vector<int>{0, 1, 2, 3}));
}

TEST_F(BigPicturePaneTest, StartupPreferenceRoundTripsThroughTheGlobalConfiguration)
{
  BigPicturePane pane;
  auto* const startup = pane.findChild<QCheckBox*>(QStringLiteral("startBigPictureCheckBox"));
  ASSERT_NE(startup, nullptr);
  EXPECT_FALSE(startup->isChecked());
  startup->setChecked(true);
  EXPECT_TRUE(Config::Get(Config::MAIN_START_BIG_PICTURE));
  BigPicturePane second;
  EXPECT_TRUE(second.findChild<QCheckBox*>(QStringLiteral("startBigPictureCheckBox"))->isChecked());
}

TEST_F(BigPicturePaneTest, SharedPreferencesLoadAndPersistWithoutChangingBindings)
{
  auto* const controller = BigPictureInput::GetController(0);
  controller->SetOptions({true, 1.25f, 0.08f});
  controller->GetButtons()->SetControlExpression(0, "F10");
  BigPicturePane pane;
  auto* const swap = pane.findChild<QCheckBox*>(QStringLiteral("swapConfirmBackCheckBox"));
  auto* const scale = pane.findChild<QDoubleSpinBox*>(QStringLiteral("interfaceScaleSpinBox"));
  auto* const safe_area = pane.findChild<QDoubleSpinBox*>(QStringLiteral("safeAreaSpinBox"));
  ASSERT_NE(swap, nullptr);
  ASSERT_NE(scale, nullptr);
  ASSERT_NE(safe_area, nullptr);
  EXPECT_TRUE(swap->isChecked());
  EXPECT_DOUBLE_EQ(scale->value(), 125.0);
  EXPECT_DOUBLE_EQ(safe_area->value(), 8.0);
  EXPECT_FALSE(QFile::exists(directory.filePath(QStringLiteral("BigPicture.ini"))));

  swap->setChecked(false);
  scale->setValue(135.5);
  safe_area->setValue(12.5);
  const auto expected = BigPicture::NavigationOptions{false, 1.355f, 0.125f};
  EXPECT_EQ(controller->GetOptions(), expected);

  Common::IniFile saved;
  ASSERT_TRUE(saved.Load(directory.filePath(QStringLiteral("BigPicture.ini")).toStdString()));
  BigPictureInput::MenuController reloaded(0);
  reloaded.LoadConfig(saved.GetOrCreateSection("BigPicture1"));
  EXPECT_EQ(reloaded.GetOptions(), expected);
  EXPECT_EQ(reloaded.GetButtons()->controls[0]->control_ref->GetExpression(), "F10");
}

TEST_F(BigPicturePaneTest, RefreshesSharedPreferencesOnShowAndWindowActivation)
{
  QWidget window;
  BigPicturePane pane(&window);
  window.show();
  auto* const controller = BigPictureInput::GetController(0);
  controller->SetOptions({true, 1.1f, 0.1f});
  QEvent activation(QEvent::WindowActivate);
  QApplication::sendEvent(&window, &activation);
  auto* const swap = pane.findChild<QCheckBox*>(QStringLiteral("swapConfirmBackCheckBox"));
  auto* const scale = pane.findChild<QDoubleSpinBox*>(QStringLiteral("interfaceScaleSpinBox"));
  auto* const safe_area = pane.findChild<QDoubleSpinBox*>(QStringLiteral("safeAreaSpinBox"));
  ASSERT_NE(swap, nullptr);
  ASSERT_NE(scale, nullptr);
  ASSERT_NE(safe_area, nullptr);
  EXPECT_TRUE(swap->isChecked());
  EXPECT_DOUBLE_EQ(scale->value(), 110.0);
  EXPECT_DOUBLE_EQ(safe_area->value(), 10.0);

  pane.hide();
  controller->SetOptions({false, 0.9f, 0.03f});
  pane.show();
  EXPECT_FALSE(swap->isChecked());
  EXPECT_DOUBLE_EQ(scale->value(), 90.0);
  EXPECT_DOUBLE_EQ(safe_area->value(), 3.0);
  EXPECT_FALSE(QFile::exists(directory.filePath(QStringLiteral("BigPicture.ini"))));
}
