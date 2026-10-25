// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_EnhancementsWidget.h"

TEST(GraphicsEnhancementsUiTest, FormOwnsTheEnhancementsGraphicsStructure)
{
  QWidget pane;
  Ui::EnhancementsWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 3);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.enhancementsGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.stereoscopyGroup);
  EXPECT_NE(ui.rootLayout->itemAt(2)->spacerItem(), nullptr);

  EXPECT_EQ(ui.enhancementsLayout->itemAtPosition(0, 1)->widget(), ui.internalResolutionComboBox);
  EXPECT_EQ(ui.enhancementsLayout->itemAtPosition(1, 1)->widget(), ui.antiAliasingComboBox);
  EXPECT_EQ(ui.enhancementsLayout->itemAtPosition(2, 1)->widget(), ui.textureFilteringComboBox);
  EXPECT_TRUE(ui.postProcessingPresetLineEdit->isReadOnly());
  EXPECT_EQ(ui.enhancementsLayout->itemAtPosition(4, 0)->layout(), ui.postProcessingButtonsLayout);
  EXPECT_FALSE(ui.postProcessingBrowseButton->autoDefault());
  EXPECT_FALSE(ui.postProcessingClearButton->autoDefault());
  EXPECT_FALSE(ui.downloadShaderPackButton->autoDefault());
  EXPECT_FALSE(ui.postProcessingParametersButton->autoDefault());

  EXPECT_EQ(ui.stereoModeComboBox->count(), 4);
  EXPECT_EQ(ui.stereoModeComboBox->itemText(0), QStringLiteral("Off"));
  EXPECT_EQ(ui.stereoModeComboBox->itemText(3), QStringLiteral("HDMI 3D"));
  EXPECT_EQ(ui.stereoDepthSlider->minimum(), 0);
  EXPECT_EQ(ui.stereoDepthSlider->maximum(), 100);
  EXPECT_EQ(ui.stereoConvergenceSlider->minimum(), 0);
  EXPECT_EQ(ui.stereoConvergenceSlider->maximum(), 20000);
}
