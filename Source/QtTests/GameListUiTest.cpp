// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QLineEdit>
#include <QMargins>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QToolButton>
#include <QWidget>

#include <gtest/gtest.h>

#include "Core/Config/UISettings.h"

#include "DolphinQt/GameList/GridProxyModel.h"
#include "DolphinQt/Settings.h"

#include "ui_GameListWidget.h"

TEST(GameListUiTest, NewProfilesDefaultToGridWithCoverDownloads)
{
  EXPECT_FALSE(Settings::DEFAULT_PREFERRED_VIEW_IS_LIST);
  EXPECT_TRUE(Config::MAIN_USE_GAME_COVERS.GetDefaultValue());
}

TEST(GameListUiTest, DefaultViewportFitsSevenColumnsByThreeRows)
{
  EXPECT_EQ(GameListGrid::CalculateViewportSize(QSize(166, 242), 16, 7, 3, 15), QSize(1305, 790));
}

TEST(GameListUiTest, FormOwnsThePermanentLibraryControlsAndViewStack)
{
  QWidget widget;
  Ui::GameListWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.rootLayout->spacing(), 0);
  EXPECT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.controlBar);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.viewStack);

  EXPECT_FALSE(ui.controlBar->isHidden());
  EXPECT_EQ(ui.controlBar->minimumHeight(), 40);
  EXPECT_EQ(ui.controlBar->maximumHeight(), 40);
  ASSERT_EQ(ui.controlsLayout->count(), 6);
  EXPECT_EQ(ui.controlsLayout->itemAt(0)->widget(), ui.listViewButton);
  EXPECT_EQ(ui.controlsLayout->itemAt(1)->widget(), ui.gridViewButton);
  EXPECT_EQ(ui.controlsLayout->itemAt(2)->widget(), ui.gridScaleSlider);
  EXPECT_EQ(ui.controlsLayout->itemAt(3)->widget(), ui.platformFilter);
  EXPECT_EQ(ui.controlsLayout->itemAt(4)->widget(), ui.regionFilter);
  EXPECT_EQ(ui.controlsLayout->itemAt(5)->widget(), ui.searchEdit);

  EXPECT_TRUE(ui.listViewButton->isCheckable());
  EXPECT_TRUE(ui.gridViewButton->isCheckable());
  EXPECT_TRUE(ui.listViewButton->autoExclusive());
  EXPECT_TRUE(ui.gridViewButton->autoExclusive());
  EXPECT_EQ(ui.listViewButton->accessibleName(), QStringLiteral("List View"));
  EXPECT_EQ(ui.gridViewButton->accessibleName(), QStringLiteral("Grid View"));
  EXPECT_EQ(ui.listViewButton->toolTip(), QStringLiteral("List View"));
  EXPECT_EQ(ui.gridViewButton->toolTip(), QStringLiteral("Grid View"));
  EXPECT_EQ(ui.listViewButton->minimumSize(), QSize(32, 32));
  EXPECT_EQ(ui.gridViewButton->minimumSize(), QSize(32, 32));

  EXPECT_EQ(ui.gridScaleSlider->minimum(), 10);
  EXPECT_EQ(ui.gridScaleSlider->maximum(), 200);
  EXPECT_EQ(ui.gridScaleSlider->singleStep(), 10);
  EXPECT_EQ(ui.gridScaleSlider->value(), 100);
  EXPECT_EQ(ui.gridScaleSlider->accessibleName(), QStringLiteral("Grid size"));
  EXPECT_EQ(ui.platformFilter->currentText(), QStringLiteral("All Platforms"));
  EXPECT_EQ(ui.regionFilter->currentText(), QStringLiteral("All Regions"));
  EXPECT_EQ(ui.platformFilter->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);
  EXPECT_EQ(ui.regionFilter->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);

  EXPECT_EQ(ui.searchEdit->placeholderText(), QStringLiteral("Search games..."));
  EXPECT_EQ(ui.searchEdit->accessibleName(), QStringLiteral("Search games"));
  EXPECT_TRUE(ui.searchEdit->isClearButtonEnabled());
  EXPECT_EQ(ui.searchEdit->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);

  EXPECT_EQ(ui.listViewButton->nextInFocusChain(), ui.gridViewButton);
  EXPECT_EQ(ui.gridViewButton->nextInFocusChain(), ui.gridScaleSlider);
  EXPECT_EQ(ui.gridScaleSlider->nextInFocusChain(), ui.platformFilter);
  EXPECT_EQ(ui.platformFilter->nextInFocusChain(), ui.regionFilter);
  EXPECT_EQ(ui.regionFilter->nextInFocusChain(), ui.searchEdit);

  widget.resize(360, 240);
  ui.rootLayout->setGeometry(widget.rect());
  ui.controlsLayout->setGeometry(ui.controlBar->rect());
  EXPECT_EQ(ui.listViewButton->width(), 32);
  EXPECT_EQ(ui.gridViewButton->width(), 32);
  EXPECT_GE(ui.gridScaleSlider->width(), 80);
  EXPECT_GT(ui.platformFilter->width(), 0);
  EXPECT_GT(ui.regionFilter->width(), 0);
  EXPECT_GT(ui.searchEdit->width(), 0);
}
