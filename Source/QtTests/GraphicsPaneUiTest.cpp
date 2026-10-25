// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QMargins>
#include <QTabWidget>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_GraphicsPane.h"

TEST(GraphicsPaneUiTest, FormOwnsTheGraphicsTabShell)
{
  QWidget pane;
  Ui::GraphicsPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.tabWidget);
  ASSERT_EQ(ui.tabWidget->count(), 4);
  EXPECT_EQ(ui.tabWidget->widget(0), ui.generalTab);
  EXPECT_EQ(ui.tabWidget->widget(1), ui.enhancementsTab);
  EXPECT_EQ(ui.tabWidget->widget(2), ui.hacksTab);
  EXPECT_EQ(ui.tabWidget->widget(3), ui.advancedTab);
  EXPECT_EQ(ui.tabWidget->tabText(0), QStringLiteral("General"));
  EXPECT_EQ(ui.tabWidget->tabText(1), QStringLiteral("Enhancements"));
  EXPECT_EQ(ui.tabWidget->tabText(2), QStringLiteral("Hacks"));
  EXPECT_EQ(ui.tabWidget->tabText(3), QStringLiteral("Advanced"));

  for (QLayout* const layout :
       {ui.generalLayout, ui.enhancementsLayout, ui.hacksLayout, ui.advancedLayout})
  {
    EXPECT_EQ(layout->contentsMargins(), QMargins());
    EXPECT_EQ(layout->spacing(), 0);
    EXPECT_EQ(layout->count(), 0);
  }
}
