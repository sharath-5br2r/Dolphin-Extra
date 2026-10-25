// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QGridLayout>
#include <QGroupBox>
#include <QSlider>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_HacksWidget.h"

TEST(GraphicsHacksUiTest, FormOwnsTheGraphicsHacksStructure)
{
  QWidget pane;
  Ui::HacksWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 5);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.efbGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.textureCacheGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.xfbGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(3)->widget(), ui.otherGroup);
  EXPECT_NE(ui.rootLayout->itemAt(4)->spacerItem(), nullptr);

  EXPECT_EQ(ui.efbLayout->itemAtPosition(0, 0)->widget(), ui.skipEfbCpuCheckBox);
  EXPECT_EQ(ui.efbLayout->itemAtPosition(0, 1)->widget(), ui.ignoreFormatChangesCheckBox);
  EXPECT_EQ(ui.efbLayout->itemAtPosition(1, 0)->widget(), ui.storeEfbCopiesCheckBox);
  EXPECT_EQ(ui.efbLayout->itemAtPosition(1, 1)->widget(), ui.deferEfbCopiesCheckBox);

  EXPECT_EQ(ui.accuracySlider->minimum(), 0);
  EXPECT_EQ(ui.accuracySlider->maximum(), 2);
  EXPECT_EQ(ui.accuracySlider->pageStep(), 1);
  EXPECT_EQ(ui.accuracySlider->tickPosition(), QSlider::TicksBelow);
  EXPECT_EQ(ui.textureCacheLayout->itemAtPosition(0, 2)->widget(), ui.accuracySlider);

  EXPECT_EQ(ui.xfbLayout->count(), 3);
  EXPECT_EQ(ui.otherLayout->itemAtPosition(0, 0)->widget(), ui.fastDepthCalculationCheckBox);
  EXPECT_EQ(ui.otherLayout->itemAtPosition(0, 1)->widget(), ui.disableBoundingBoxCheckBox);
  EXPECT_EQ(ui.otherLayout->itemAtPosition(2, 0)->widget(), ui.viSkipCheckBox);
}
