// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractScrollArea>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QListWidget>
#include <QMargins>
#include <QSize>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_SettingsWindow.h"

TEST(SettingsWindowUiTest, FormOwnsTheSharedSettingsShell)
{
  QDialog dialog;
  Ui::SettingsWindow ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.rootLayout->spacing(), 0);
  ASSERT_EQ(ui.rootLayout->count(), 4);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(0, 0)->widget(), ui.navigationFrame);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(0, 1)->widget(), ui.contentWidget);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(1, 1)->widget(), ui.helpFrame);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(2, 1)->widget(), ui.footerFrame);

  int row = 0;
  int column = 0;
  int row_span = 0;
  int column_span = 0;
  ui.rootLayout->getItemPosition(ui.rootLayout->indexOf(ui.navigationFrame), &row, &column,
                                 &row_span, &column_span);
  EXPECT_EQ(row, 0);
  EXPECT_EQ(column, 0);
  EXPECT_EQ(row_span, 3);
  EXPECT_EQ(column_span, 1);
  ui.rootLayout->getItemPosition(ui.rootLayout->indexOf(ui.helpFrame), &row, &column, &row_span,
                                 &column_span);
  EXPECT_EQ(row, 1);
  EXPECT_EQ(column, 1);
  EXPECT_EQ(row_span, 1);
  EXPECT_EQ(column_span, 1);
  ui.rootLayout->getItemPosition(ui.rootLayout->indexOf(ui.footerFrame), &row, &column, &row_span,
                                 &column_span);
  EXPECT_EQ(row, 2);
  EXPECT_EQ(column, 1);
  EXPECT_EQ(row_span, 1);
  EXPECT_EQ(column_span, 1);

  EXPECT_EQ(ui.navigationFrame->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
  EXPECT_EQ(ui.navigationFrame->frameShape(), QFrame::NoFrame);
  EXPECT_EQ(ui.navigationLayout->contentsMargins(), QMargins(12, 0, 12, 0));
  EXPECT_EQ(ui.navigationLayout->spacing(), 0);
  ASSERT_EQ(ui.navigationLayout->count(), 1);
  EXPECT_EQ(ui.navigationLayout->itemAt(0)->widget(), ui.navigationList);

  EXPECT_EQ(ui.navigationList->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
  EXPECT_EQ(ui.navigationList->sizeAdjustPolicy(),
            QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents);
  EXPECT_EQ(ui.navigationList->iconSize(), QSize(32, 32));
  EXPECT_TRUE(ui.navigationList->wordWrap());
  EXPECT_EQ(ui.navigationList->accessibleName(), QStringLiteral("Settings categories"));

  EXPECT_EQ(ui.contentLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.contentLayout->spacing(), 0);
  ASSERT_EQ(ui.contentLayout->count(), 1);
  EXPECT_EQ(ui.contentLayout->itemAt(0)->widget(), ui.stackedPanes);

  EXPECT_EQ(ui.helpFrame->frameShape(), QFrame::NoFrame);
  EXPECT_TRUE(ui.helpFrame->isHidden());
  EXPECT_EQ(ui.helpLayout->contentsMargins(), QMargins(12, 8, 12, 0));
  ASSERT_EQ(ui.helpLayout->count(), 1);
  EXPECT_EQ(ui.helpLayout->itemAt(0)->widget(), ui.helpText);
  EXPECT_TRUE(ui.helpText->isReadOnly());
  EXPECT_EQ(ui.helpText->minimumHeight(), 122);
  EXPECT_EQ(ui.helpText->maximumHeight(), 122);
  EXPECT_FALSE(ui.helpText->isVisible());
  EXPECT_EQ(ui.helpText->accessibleName(), QStringLiteral("Setting description"));

  EXPECT_EQ(ui.footerLayout->contentsMargins(), QMargins(12, 8, 12, 8));
  EXPECT_NE(ui.buttonBox->button(QDialogButtonBox::StandardButton::Close), nullptr);
}
