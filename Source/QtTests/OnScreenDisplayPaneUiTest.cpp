// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_OnScreenDisplayPane.h"

namespace
{
QWidget* NextTabFocusWidget(QWidget* widget)
{
  QWidget* next = widget;
  do
  {
    next = next->nextInFocusChain();
  } while (!(next->focusPolicy() & Qt::TabFocus) || widget->isAncestorOf(next));
  return next;
}
}  // namespace

TEST(OnScreenDisplayPaneUiTest, FormOwnsTheOnScreenDisplayStructure)
{
  QWidget pane;
  Ui::OnScreenDisplayPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 6);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.generalGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.performanceGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.movieGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(3)->widget(), ui.netplayGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(4)->widget(), ui.debugGroup);
  EXPECT_NE(ui.rootLayout->itemAt(5)->spacerItem(), nullptr);

  EXPECT_EQ(ui.generalLayout->itemAtPosition(0, 0)->widget(), ui.showMessagesCheckBox);
  EXPECT_EQ(ui.generalLayout->itemAtPosition(1, 0)->widget(), ui.fontSizeLabel);
  EXPECT_EQ(ui.generalLayout->itemAtPosition(1, 1)->widget(), ui.fontSizeSpinBox);
  EXPECT_EQ(ui.fontSizeLabel->buddy(), ui.fontSizeSpinBox);
  EXPECT_EQ(ui.fontSizeSpinBox->minimum(), 12);
  EXPECT_EQ(ui.fontSizeSpinBox->maximum(), 40);

  EXPECT_EQ(ui.performanceLayout->itemAtPosition(0, 0)->widget(), ui.showFpsCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(0, 1)->widget(), ui.showFrameTimesCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(1, 0)->widget(), ui.showVpsCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(1, 1)->widget(), ui.showVblankTimesCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(2, 0)->widget(), ui.showSpeedCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(2, 1)->widget(), ui.showGraphsCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(3, 0)->widget(), ui.showSpeedColorsCheckBox);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(4, 0)->widget(), ui.performanceSampleWindowLabel);
  EXPECT_EQ(ui.performanceLayout->itemAtPosition(4, 1)->widget(),
            ui.performanceSampleWindowSpinBox);
  EXPECT_EQ(ui.performanceSampleWindowLabel->buddy(), ui.performanceSampleWindowSpinBox);
  EXPECT_EQ(ui.performanceSampleWindowSpinBox->minimum(), 0);
  EXPECT_EQ(ui.performanceSampleWindowSpinBox->maximum(), 10000);
  EXPECT_EQ(ui.performanceSampleWindowSpinBox->singleStep(), 100);

  EXPECT_EQ(ui.movieLayout->itemAtPosition(0, 0)->widget(), ui.showMovieWindowCheckBox);
  EXPECT_EQ(ui.movieLayout->itemAtPosition(1, 0)->widget(), ui.showRerecordCounterCheckBox);
  EXPECT_EQ(ui.movieLayout->itemAtPosition(1, 1)->widget(), ui.showLagCounterCheckBox);
  EXPECT_EQ(ui.movieLayout->itemAtPosition(2, 0)->widget(), ui.showFrameCounterCheckBox);
  EXPECT_EQ(ui.movieLayout->itemAtPosition(2, 1)->widget(), ui.showInputDisplayCheckBox);
  EXPECT_EQ(ui.movieLayout->itemAtPosition(3, 0)->widget(), ui.showSystemClockCheckBox);

  EXPECT_EQ(ui.netplayLayout->itemAtPosition(0, 0)->widget(), ui.showNetplayPingCheckBox);
  EXPECT_EQ(ui.netplayLayout->itemAtPosition(0, 1)->widget(), ui.showNetplayChatCheckBox);
  EXPECT_EQ(ui.debugLayout->itemAtPosition(0, 0)->widget(), ui.showStatisticsCheckBox);
  EXPECT_EQ(ui.debugLayout->itemAtPosition(0, 1)->widget(), ui.showProjectionStatisticsCheckBox);
  EXPECT_EQ(ui.debugLayout->itemAtPosition(1, 0)->widget(), ui.showXfbResolutionCheckBox);

  pane.resize(520, 760);
  ui.rootLayout->setGeometry(pane.rect());
  for (QGroupBox* group :
       {ui.generalGroup, ui.performanceGroup, ui.movieGroup, ui.netplayGroup, ui.debugGroup})
  {
    EXPECT_LE(group->geometry().right(), pane.rect().right());
  }

  EXPECT_EQ(NextTabFocusWidget(ui.showMessagesCheckBox), ui.fontSizeSpinBox);
  EXPECT_EQ(NextTabFocusWidget(ui.fontSizeSpinBox), ui.showFpsCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.showSpeedColorsCheckBox), ui.performanceSampleWindowSpinBox);
  EXPECT_EQ(NextTabFocusWidget(ui.performanceSampleWindowSpinBox), ui.showMovieWindowCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.showMovieWindowCheckBox), ui.showRerecordCounterCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.showSystemClockCheckBox), ui.showNetplayPingCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.showNetplayChatCheckBox), ui.showStatisticsCheckBox);
}
