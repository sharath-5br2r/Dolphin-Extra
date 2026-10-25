// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMargins>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_GBATASInputWindow.h"
#include "ui_GCTASInputWindow.h"
#include "ui_TASInputWindow.h"
#include "ui_WiiTASInputWindow.h"

TEST(TasUiTest, SharedFormOwnsScrollAndSettingsShell)
{
  QDialog dialog;
  Ui::TASInputWindow ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.outerLayout->contentsMargins(), QMargins());
  EXPECT_TRUE(ui.scrollArea->widgetResizable());
  EXPECT_EQ(ui.scrollArea->horizontalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
  EXPECT_EQ(ui.scrollArea->verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
  EXPECT_EQ(ui.scrollArea->widget(), ui.scrollWidget);
  EXPECT_EQ(ui.contentLayout->count(), 1);
  EXPECT_EQ(ui.contentLayout->itemAt(0)->widget(), ui.settingsBox);

  EXPECT_EQ(ui.settingsBox->title(), QStringLiteral("Settings"));
  EXPECT_EQ(ui.settingsLayout->rowCount(), 3);
  EXPECT_EQ(ui.useControllerCheckBox->text(), QStringLiteral("Enable Controller Inpu&t"));
  EXPECT_FALSE(ui.useControllerCheckBox->toolTip().isEmpty());
  EXPECT_EQ(ui.turboPressLabel->buddy(), ui.turboPressSpinBox);
  EXPECT_EQ(ui.turboReleaseLabel->buddy(), ui.turboReleaseSpinBox);
  EXPECT_EQ(ui.turboPressSpinBox->minimum(), 1);
  EXPECT_EQ(ui.turboReleaseSpinBox->minimum(), 1);

  const auto focus_index = [&ui](QWidget* target) {
    QWidget* current = ui.useControllerCheckBox;
    for (int index = 1; index < 100; ++index)
    {
      current = current->nextInFocusChain();
      for (QWidget* ancestor = current; ancestor; ancestor = ancestor->parentWidget())
      {
        if (ancestor == target)
          return index;
      }
      if (current == ui.useControllerCheckBox)
        break;
    }
    return -1;
  };

  const int press_focus_index = focus_index(ui.turboPressSpinBox);
  const int release_focus_index = focus_index(ui.turboReleaseSpinBox);
  ASSERT_GT(press_focus_index, 0);
  ASSERT_GT(release_focus_index, 0);
  EXPECT_LT(press_focus_index, release_focus_index);
}

TEST(TasUiTest, GameCubeFormOwnsControllerContainers)
{
  QWidget widget;
  Ui::GCTASInputWindow ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.stickLayout->count(), 0);
  EXPECT_EQ(ui.triggersBox->title(), QStringLiteral("Triggers"));
  EXPECT_EQ(ui.triggersLayout->count(), 0);
  EXPECT_EQ(ui.buttonsBox->title(), QStringLiteral("Buttons"));
  ASSERT_NE(ui.buttonsLayout->itemAtPosition(0, 7), nullptr);
  EXPECT_NE(ui.buttonsLayout->itemAtPosition(0, 7)->spacerItem(), nullptr);
}

TEST(TasUiTest, GbaFormOwnsButtonGrid)
{
  QWidget widget;
  Ui::GBATASInputWindow ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.buttonsBox->title(), QStringLiteral("Buttons"));
  ASSERT_NE(ui.buttonsLayout->itemAtPosition(0, 4), nullptr);
  EXPECT_NE(ui.buttonsLayout->itemAtPosition(0, 4)->spacerItem(), nullptr);
}

TEST(TasUiTest, WiiFormOwnsAttachmentDependentContainers)
{
  QWidget widget;
  Ui::WiiTASInputWindow ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.mainLayout->count(), 8);
  EXPECT_EQ(ui.stickLayout->count(), 0);
  EXPECT_EQ(ui.remoteAccelerometerBox->title(), QStringLiteral("Wii Remote Accelerometer"));
  EXPECT_EQ(ui.remoteGyroscopeBox->title(), QStringLiteral("Wii Remote Gyroscope"));
  EXPECT_EQ(ui.nunchukAccelerometerBox->title(), QStringLiteral("Nunchuk Accelerometer"));
  EXPECT_EQ(ui.triggersBox->title(), QStringLiteral("Triggers"));
  EXPECT_EQ(ui.remoteButtonsBox->title(), QStringLiteral("Wii Remote Buttons"));
  EXPECT_EQ(ui.nunchukButtonsBox->title(), QStringLiteral("Nunchuk Buttons"));
  EXPECT_EQ(ui.classicButtonsBox->title(), QStringLiteral("Classic Buttons"));
  EXPECT_EQ(ui.remoteAccelerometerLayout->count(), 0);
  EXPECT_EQ(ui.remoteGyroscopeLayout->count(), 0);
  EXPECT_EQ(ui.nunchukAccelerometerLayout->count(), 0);
  EXPECT_EQ(ui.triggersLayout->count(), 0);
  ASSERT_NE(ui.remoteButtonsLayout->itemAtPosition(0, 7), nullptr);
  EXPECT_NE(ui.remoteButtonsLayout->itemAtPosition(0, 7)->spacerItem(), nullptr);
  EXPECT_EQ(ui.nunchukButtonsLayout->count(), 1);
  EXPECT_NE(ui.nunchukButtonsLayout->itemAt(0)->spacerItem(), nullptr);
  ASSERT_NE(ui.classicButtonsLayout->itemAtPosition(0, 8), nullptr);
  EXPECT_NE(ui.classicButtonsLayout->itemAtPosition(0, 8)->spacerItem(), nullptr);
}
