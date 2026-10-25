// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_FIFOAnalyzer.h"
#include "ui_FIFOPlayerWindow.h"

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

TEST(FifoUiTest, PlayerFormOwnsThePlayAndRecordStructure)
{
  QWidget widget;
  Ui::FIFOPlayerWindow ui;
  ui.setupUi(&widget);

  EXPECT_EQ(widget.windowTitle(), QStringLiteral("FIFO Player"));
  EXPECT_EQ(widget.size(), QSize(600, 580));
  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.tabWidget);

  ASSERT_EQ(ui.tabWidget->count(), 1);
  EXPECT_EQ(ui.tabWidget->widget(0), ui.playRecordTab);
  EXPECT_EQ(ui.tabWidget->tabText(0), QStringLiteral("Play / Record"));

  ASSERT_EQ(ui.playRecordLayout->count(), 5);
  EXPECT_EQ(ui.playRecordLayout->itemAt(0)->widget(), ui.fileInfoGroup);
  EXPECT_EQ(ui.playRecordLayout->itemAt(1)->widget(), ui.playbackGroup);
  EXPECT_EQ(ui.playRecordLayout->itemAt(2)->widget(), ui.recordingGroup);
  EXPECT_NE(ui.playRecordLayout->itemAt(3)->spacerItem(), nullptr);
  EXPECT_EQ(ui.playRecordLayout->itemAt(4)->widget(), ui.buttonBox);
  EXPECT_EQ(ui.fileInfoLayout->itemAt(0)->widget(), ui.infoLabel);

  EXPECT_EQ(ui.playbackLayout->itemAtPosition(0, 0)->widget(), ui.objectRangeGroup);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(0, 1)->widget(), ui.frameRangeGroup);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(1, 0)->widget(), ui.earlyMemoryUpdatesCheckBox);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(1, 1)->widget(), ui.loopCheckBox);

  EXPECT_EQ(ui.objectRangeLayout->itemAt(0)->widget(), ui.objectRangeFromLabel);
  EXPECT_EQ(ui.objectRangeLayout->itemAt(1)->widget(), ui.objectRangeFromSpinBox);
  EXPECT_EQ(ui.objectRangeLayout->itemAt(2)->widget(), ui.objectRangeToLabel);
  EXPECT_EQ(ui.objectRangeLayout->itemAt(3)->widget(), ui.objectRangeToSpinBox);
  EXPECT_EQ(ui.frameRangeLayout->itemAt(0)->widget(), ui.frameRangeFromLabel);
  EXPECT_EQ(ui.frameRangeLayout->itemAt(1)->widget(), ui.frameRangeFromSpinBox);
  EXPECT_EQ(ui.frameRangeLayout->itemAt(2)->widget(), ui.frameRangeToLabel);
  EXPECT_EQ(ui.frameRangeLayout->itemAt(3)->widget(), ui.frameRangeToSpinBox);
  EXPECT_EQ(ui.objectRangeFromLabel->buddy(), ui.objectRangeFromSpinBox);
  EXPECT_EQ(ui.objectRangeToLabel->buddy(), ui.objectRangeToSpinBox);
  EXPECT_EQ(ui.frameRangeFromLabel->buddy(), ui.frameRangeFromSpinBox);
  EXPECT_EQ(ui.frameRangeToLabel->buddy(), ui.frameRangeToSpinBox);

  EXPECT_EQ(ui.recordingLayout->itemAt(0)->widget(), ui.frameRecordCountLabel);
  EXPECT_EQ(ui.recordingLayout->itemAt(1)->widget(), ui.frameRecordCountSpinBox);
  EXPECT_EQ(ui.frameRecordCountLabel->buddy(), ui.frameRecordCountSpinBox);
  EXPECT_EQ(ui.frameRecordCountSpinBox->minimum(), 1);
  EXPECT_EQ(ui.frameRecordCountSpinBox->maximum(), 3600);
  EXPECT_EQ(ui.frameRecordCountSpinBox->value(), 3);
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Close);

  EXPECT_EQ(NextTabFocusWidget(ui.objectRangeFromSpinBox), ui.objectRangeToSpinBox);
  EXPECT_EQ(NextTabFocusWidget(ui.objectRangeToSpinBox), ui.frameRangeFromSpinBox);
  EXPECT_EQ(NextTabFocusWidget(ui.frameRangeFromSpinBox), ui.frameRangeToSpinBox);
  EXPECT_EQ(NextTabFocusWidget(ui.frameRangeToSpinBox), ui.earlyMemoryUpdatesCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.earlyMemoryUpdatesCheckBox), ui.loopCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.loopCheckBox), ui.frameRecordCountSpinBox);
}

TEST(FifoUiTest, AnalyzerFormOwnsSplittersDetailsAndSearchControls)
{
  QWidget widget;
  Ui::FIFOAnalyzer ui;
  ui.setupUi(&widget);

  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.searchSplitter);
  EXPECT_EQ(ui.searchSplitter->orientation(), Qt::Vertical);
  ASSERT_EQ(ui.searchSplitter->count(), 3);
  EXPECT_EQ(ui.searchSplitter->widget(0), ui.objectSplitter);
  EXPECT_EQ(ui.searchSplitter->widget(1), ui.entryDetailBrowser);
  EXPECT_EQ(ui.searchSplitter->widget(2), ui.searchBox);

  EXPECT_EQ(ui.objectSplitter->orientation(), Qt::Horizontal);
  ASSERT_EQ(ui.objectSplitter->count(), 2);
  EXPECT_EQ(ui.objectSplitter->widget(0), ui.treeWidget);
  EXPECT_EQ(ui.objectSplitter->widget(1), ui.detailList);
  EXPECT_EQ(ui.searchBox->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);

  ASSERT_EQ(ui.searchLayout->count(), 5);
  EXPECT_EQ(ui.searchLayout->itemAt(0)->widget(), ui.searchEdit);
  EXPECT_EQ(ui.searchLayout->itemAt(1)->widget(), ui.searchButton);
  EXPECT_EQ(ui.searchLayout->itemAt(2)->widget(), ui.nextMatchButton);
  EXPECT_EQ(ui.searchLayout->itemAt(3)->widget(), ui.previousMatchButton);
  EXPECT_EQ(ui.searchLayout->itemAt(4)->widget(), ui.searchLabel);
  EXPECT_FALSE(ui.searchButton->autoDefault());
  EXPECT_FALSE(ui.nextMatchButton->autoDefault());
  EXPECT_FALSE(ui.previousMatchButton->autoDefault());
  EXPECT_FALSE(ui.nextMatchButton->isEnabled());
  EXPECT_FALSE(ui.previousMatchButton->isEnabled());

  EXPECT_EQ(NextTabFocusWidget(ui.treeWidget), ui.detailList);
  EXPECT_EQ(NextTabFocusWidget(ui.detailList), ui.entryDetailBrowser);
  EXPECT_EQ(NextTabFocusWidget(ui.entryDetailBrowser), ui.searchEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.searchEdit), ui.searchButton);
  EXPECT_EQ(NextTabFocusWidget(ui.searchButton), ui.nextMatchButton);
  EXPECT_EQ(NextTabFocusWidget(ui.nextMatchButton), ui.previousMatchButton);
}
