// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_GeneralWidget.h"

TEST(GraphicsGeneralUiTest, FormOwnsTheGeneralGraphicsStructure)
{
  QWidget pane;
  Ui::GeneralWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 4);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.basicGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.otherGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.shaderCompilationGroup);
  EXPECT_NE(ui.rootLayout->itemAt(3)->spacerItem(), nullptr);

  EXPECT_EQ(ui.aspectRatioComboBox->count(), 6);
  EXPECT_EQ(ui.aspectRatioComboBox->itemText(0), QStringLiteral("Auto"));
  EXPECT_EQ(ui.aspectRatioComboBox->itemText(4), QStringLiteral("Custom"));
  EXPECT_EQ(ui.aspectRatioComboBox->itemText(5), QStringLiteral("Custom (Stretch)"));
  EXPECT_EQ(ui.customAspectWidthSpinBox->minimum(), 1);
  EXPECT_EQ(ui.customAspectWidthSpinBox->maximum(), 10000);
  EXPECT_EQ(ui.customAspectHeightSpinBox->minimum(), 1);
  EXPECT_EQ(ui.customAspectHeightSpinBox->maximum(), 10000);

  EXPECT_EQ(ui.basicOptionsLayout->itemAtPosition(0, 0)->widget(), ui.vsyncCheckBox);
  EXPECT_EQ(ui.basicOptionsLayout->itemAtPosition(0, 1)->widget(), ui.fullscreenCheckBox);
  EXPECT_EQ(ui.basicOptionsLayout->itemAtPosition(1, 0)->widget(), ui.precisionFrameTimingCheckBox);
  EXPECT_EQ(ui.basicOptionsLayout->itemAtPosition(1, 1)->widget(), ui.integerScalingCheckBox);
  EXPECT_EQ(ui.otherLayout->itemAtPosition(0, 0)->widget(), ui.renderToMainWindowCheckBox);
  EXPECT_EQ(ui.otherLayout->itemAtPosition(0, 1)->widget(), ui.autoAdjustWindowSizeCheckBox);
  EXPECT_EQ(ui.shaderCompilationLayout->itemAtPosition(0, 0)->widget(),
            ui.specializedShaderRadioButton);
  EXPECT_EQ(ui.shaderCompilationLayout->itemAtPosition(0, 1)->widget(),
            ui.exclusiveUbershadersRadioButton);
  EXPECT_EQ(ui.shaderCompilationLayout->itemAtPosition(1, 0)->widget(),
            ui.hybridUbershadersRadioButton);
  EXPECT_EQ(ui.shaderCompilationLayout->itemAtPosition(1, 1)->widget(), ui.skipDrawingRadioButton);
  EXPECT_EQ(ui.shaderCompilationLayout->itemAtPosition(2, 0)->widget(), ui.waitForShadersCheckBox);

  EXPECT_TRUE(ui.specializedShaderRadioButton->autoExclusive());
  EXPECT_TRUE(ui.exclusiveUbershadersRadioButton->autoExclusive());
  EXPECT_TRUE(ui.hybridUbershadersRadioButton->autoExclusive());
  EXPECT_TRUE(ui.skipDrawingRadioButton->autoExclusive());
}
