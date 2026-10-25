// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QGridLayout>
#include <QSpinBox>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_AdvancedWidget.h"

TEST(GraphicsAdvancedUiTest, FormOwnsTheAdvancedGraphicsStructure)
{
  QWidget pane;
  Ui::AdvancedWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 8);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.debuggingGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.utilityGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.textureDumpingGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(3)->widget(), ui.frameDumpingGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(4)->widget(), ui.cropGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(5)->widget(), ui.miscGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(6)->widget(), ui.experimentalGroup);
  EXPECT_NE(ui.rootLayout->itemAt(7)->spacerItem(), nullptr);

  EXPECT_EQ(ui.resolutionTypeComboBox->count(), 3);
  EXPECT_EQ(ui.bitrateSpinBox->minimum(), 0);
  EXPECT_EQ(ui.bitrateSpinBox->maximum(), 1000000);
  EXPECT_EQ(ui.bitrateSpinBox->singleStep(), 1000);
  EXPECT_EQ(ui.pngCompressionSpinBox->minimum(), 0);
  EXPECT_EQ(ui.pngCompressionSpinBox->maximum(), 9);

  EXPECT_EQ(ui.cropLeftSpinBox->maximum(), 640);
  EXPECT_EQ(ui.cropTopSpinBox->maximum(), 528);
  EXPECT_EQ(ui.cropRightSpinBox->maximum(), 640);
  EXPECT_EQ(ui.cropBottomSpinBox->maximum(), 528);
  EXPECT_EQ(ui.customCropLayout->itemAtPosition(0, 1)->widget(), ui.cropLeftSpinBox);
  EXPECT_EQ(ui.customCropLayout->itemAtPosition(1, 3)->widget(), ui.cropBottomSpinBox);

  EXPECT_EQ(ui.miscLayout->itemAtPosition(0, 0)->widget(), ui.backendMultithreadingCheckBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(0, 1)->widget(), ui.progressiveScanCheckBox);
  EXPECT_EQ(ui.experimentalLayout->itemAtPosition(0, 1)->widget(),
            ui.manualTextureSamplingCheckBox);
}
