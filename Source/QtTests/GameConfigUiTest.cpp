// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTextEdit>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_GameConfigEdit.h"
#include "ui_GameConfigWidget.h"

namespace
{
QWidget* NextTabFocusWidget(QWidget* widget)
{
  QWidget* next = widget;
  do
  {
    next = next->nextInFocusChain();
  } while (!(next->focusPolicy() & Qt::TabFocus));
  return next;
}
}  // namespace

TEST(GameConfigUiTest, FormOwnsThePerGamePageStructure)
{
  QWidget page;
  Ui::GameConfigWidget ui;
  ui.setupUi(&page);

  ASSERT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.helpFrame);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.tabWidget);
  ASSERT_EQ(ui.tabWidget->count(), 3);
  EXPECT_EQ(ui.tabWidget->widget(0), ui.generalTab);
  EXPECT_EQ(ui.tabWidget->widget(1), ui.graphicsTab);
  EXPECT_EQ(ui.tabWidget->widget(2), ui.editorTab);
  EXPECT_EQ(ui.tabWidget->tabText(0), QStringLiteral("General"));
  EXPECT_EQ(ui.tabWidget->tabText(1), QStringLiteral("Graphics"));
  EXPECT_EQ(ui.tabWidget->tabText(2), QStringLiteral("Editor"));

  ASSERT_EQ(ui.generalLayout->count(), 3);
  EXPECT_EQ(ui.generalLayout->itemAt(0)->widget(), ui.coreGroup);
  EXPECT_EQ(ui.generalLayout->itemAt(1)->widget(), ui.stereoscopyGroup);
  EXPECT_NE(ui.generalLayout->itemAt(2)->spacerItem(), nullptr);

  const std::array core_check_boxes{
      ui.enableDualCoreCheckBox, ui.enableMmuCheckBox,        ui.enableFprfCheckBox,
      ui.syncGpuCheckBox,        ui.emulateDiscSpeedCheckBox, ui.dspHleCheckBox,
  };
  for (int row = 0; row < static_cast<int>(core_check_boxes.size()); ++row)
    EXPECT_EQ(ui.coreLayout->itemAtPosition(row, 0)->widget(), core_check_boxes[row]);
  EXPECT_EQ(ui.coreLayout->itemAtPosition(6, 0)->widget(), ui.deterministicDualCoreLabel);
  EXPECT_EQ(ui.coreLayout->itemAtPosition(6, 1)->widget(), ui.deterministicDualCoreComboBox);
  EXPECT_EQ(ui.deterministicDualCoreLabel->buddy(), ui.deterministicDualCoreComboBox);

  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(0, 0)->widget(), ui.depthLabel);
  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(0, 1)->widget(), ui.depthSlider);
  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(0, 2)->widget(), ui.depthValueLabel);
  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(1, 0)->widget(), ui.convergenceLabel);
  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(1, 1)->widget(), ui.convergenceSlider);
  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(1, 2)->widget(), ui.convergenceValueLabel);
  EXPECT_EQ(ui.stereoscopyLayout->itemAtPosition(2, 0)->widget(), ui.monoscopicShadowsCheckBox);
  EXPECT_EQ(ui.depthLabel->buddy(), ui.depthSlider);
  EXPECT_EQ(ui.convergenceLabel->buddy(), ui.convergenceSlider);
  EXPECT_EQ(ui.depthSlider->minimum(), 0);
  EXPECT_EQ(ui.depthSlider->maximum(), 100);
  EXPECT_EQ(ui.convergenceSlider->minimum(), 0);
  EXPECT_EQ(ui.convergenceSlider->maximum(), 100000);

  EXPECT_EQ(ui.graphicsLayout->count(), 0);
  ASSERT_EQ(ui.editorLayout->count(), 2);
  EXPECT_EQ(ui.editorLayout->itemAt(0)->widget(), ui.defaultConfigGroup);
  EXPECT_EQ(ui.editorLayout->itemAt(1)->widget(), ui.userConfigGroup);
  EXPECT_EQ(ui.defaultConfigLayout->itemAt(0)->widget(), ui.defaultConfigTabWidget);
  EXPECT_EQ(ui.userConfigLayout->itemAt(0)->widget(), ui.userConfigTabWidget);

  EXPECT_EQ(NextTabFocusWidget(ui.enableDualCoreCheckBox), ui.enableMmuCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.enableMmuCheckBox), ui.enableFprfCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.enableFprfCheckBox), ui.syncGpuCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.syncGpuCheckBox), ui.emulateDiscSpeedCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.emulateDiscSpeedCheckBox), ui.dspHleCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.dspHleCheckBox), ui.deterministicDualCoreComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.deterministicDualCoreComboBox), ui.depthSlider);
  EXPECT_EQ(NextTabFocusWidget(ui.depthSlider), ui.convergenceSlider);
  EXPECT_EQ(NextTabFocusWidget(ui.convergenceSlider), ui.monoscopicShadowsCheckBox);

  page.resize(440, 520);
  ui.rootLayout->setGeometry(page.rect());
  ui.generalLayout->setGeometry(ui.generalTab->contentsRect());
  ui.coreLayout->setGeometry(ui.coreGroup->contentsRect());
  ui.stereoscopyLayout->setGeometry(ui.stereoscopyGroup->contentsRect());
  EXPECT_LE(ui.deterministicDualCoreComboBox->geometry().right(),
            ui.coreGroup->contentsRect().right());
  EXPECT_LE(ui.depthValueLabel->geometry().right(), ui.stereoscopyGroup->contentsRect().right());
  EXPECT_LE(ui.convergenceValueLabel->geometry().right(),
            ui.stereoscopyGroup->contentsRect().right());
}

TEST(GameConfigUiTest, FormOwnsTheIniEditorStructure)
{
  QWidget editor;
  Ui::GameConfigEdit ui;
  ui.setupUi(&editor);

  ASSERT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->layout(), ui.buttonLayout);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.textEdit);
  ASSERT_EQ(ui.buttonLayout->count(), 3);
  EXPECT_EQ(ui.buttonLayout->itemAt(0)->widget(), ui.refreshButton);
  EXPECT_EQ(ui.buttonLayout->itemAt(1)->widget(), ui.externalEditorButton);
  EXPECT_NE(ui.buttonLayout->itemAt(2)->spacerItem(), nullptr);
  EXPECT_FALSE(ui.textEdit->acceptRichText());
  EXPECT_EQ(NextTabFocusWidget(ui.refreshButton), ui.externalEditorButton);
  EXPECT_EQ(NextTabFocusWidget(ui.externalEditorButton), ui.textEdit);
}
