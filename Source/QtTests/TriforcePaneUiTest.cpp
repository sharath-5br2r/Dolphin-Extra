// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_TriforcePane.h"

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

TEST(TriforcePaneUiTest, FormOwnsTheTriforceSettingsStructure)
{
  QWidget pane;
  Ui::TriforcePane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 3);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.controllersGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.ipRedirectionGroup);
  EXPECT_NE(ui.rootLayout->itemAt(2)->spacerItem(), nullptr);

  ASSERT_EQ(ui.controllersLayout->count(), 1);
  EXPECT_EQ(ui.controllersLayout->itemAt(0)->widget(), ui.configureControllersButton);
  ASSERT_EQ(ui.ipRedirectionLayout->count(), 1);
  EXPECT_EQ(ui.ipRedirectionLayout->itemAt(0)->widget(), ui.configureIpRedirectionsButton);

  EXPECT_EQ(ui.controllersGroup->title(), QStringLiteral("Controllers"));
  EXPECT_EQ(ui.ipRedirectionGroup->title(), QStringLiteral("IP Address Redirections"));
  EXPECT_EQ(ui.configureControllersButton->text(), QStringLiteral("Configure"));
  EXPECT_EQ(ui.configureIpRedirectionsButton->text(), QStringLiteral("Configure"));
  EXPECT_FALSE(ui.configureControllersButton->autoDefault());
  EXPECT_FALSE(ui.configureControllersButton->isDefault());
  EXPECT_FALSE(ui.configureIpRedirectionsButton->autoDefault());
  EXPECT_FALSE(ui.configureIpRedirectionsButton->isDefault());

  pane.resize(360, 220);
  ui.rootLayout->setGeometry(pane.rect());
  ui.controllersLayout->setGeometry(ui.controllersGroup->contentsRect());
  ui.ipRedirectionLayout->setGeometry(ui.ipRedirectionGroup->contentsRect());
  EXPECT_GE(ui.configureControllersButton->geometry().left(), 0);
  EXPECT_LE(ui.configureControllersButton->geometry().right(),
            ui.controllersGroup->contentsRect().right());
  EXPECT_GE(ui.configureIpRedirectionsButton->geometry().left(), 0);
  EXPECT_LE(ui.configureIpRedirectionsButton->geometry().right(),
            ui.ipRedirectionGroup->contentsRect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.configureControllersButton), ui.configureIpRedirectionsButton);
}
