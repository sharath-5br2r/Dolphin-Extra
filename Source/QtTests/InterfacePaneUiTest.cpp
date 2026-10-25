// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_InterfacePane.h"

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

TEST(InterfacePaneUiTest, FormOwnsTheInterfaceSettingsStructure)
{
  QWidget pane;
  Ui::InterfacePane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 3);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.userInterfaceGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.renderWindowGroup);
  EXPECT_NE(ui.rootLayout->itemAt(2)->spacerItem(), nullptr);

  ASSERT_EQ(ui.userInterfaceLayout->count(), 7);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(0)->layout(), ui.selectionLayout);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(1)->widget(), ui.useBuiltinTitleDatabaseCheckBox);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(2)->widget(), ui.useCoversCheckBox);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(3)->widget(), ui.showDebuggingUiCheckBox);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(4)->widget(), ui.focusedHotkeysCheckBox);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(5)->widget(), ui.disableScreensaverCheckBox);
  EXPECT_EQ(ui.userInterfaceLayout->itemAt(6)->widget(), ui.timeTrackingCheckBox);

  EXPECT_EQ(ui.selectionLayout->rowCount(), 3);
  EXPECT_EQ(ui.selectionLayout->fieldGrowthPolicy(),
            QFormLayout::FieldGrowthPolicy::AllNonFixedFieldsGrow);
  EXPECT_EQ(ui.languageLabel->buddy(), ui.languageComboBox);
  EXPECT_EQ(ui.themeLabel->buddy(), ui.themeComboBox);
  EXPECT_EQ(ui.styleLabel->buddy(), ui.styleComboBox);
  EXPECT_EQ(ui.languageComboBox->sizeAdjustPolicy(), QComboBox::AdjustToContents);
  EXPECT_EQ(ui.languageComboBox->count(), 0);
  EXPECT_EQ(ui.themeComboBox->count(), 0);
  EXPECT_EQ(ui.styleComboBox->count(), 0);

  ASSERT_EQ(ui.renderWindowLayout->count(), 7);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(0)->widget(), ui.keepWindowOnTopCheckBox);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(1)->widget(), ui.confirmOnStopCheckBox);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(2)->widget(), ui.usePanicHandlersCheckBox);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(3)->widget(), ui.showActiveTitleCheckBox);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(4)->widget(), ui.pauseOnFocusLossCheckBox);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(5)->widget(), ui.cursorVisibilityGroup);
  EXPECT_EQ(ui.renderWindowLayout->itemAt(6)->widget(), ui.lockMouseCursorCheckBox);

  ASSERT_EQ(ui.cursorVisibilityLayout->count(), 3);
  EXPECT_EQ(ui.cursorVisibilityLayout->itemAt(0)->widget(), ui.cursorOnMovementRadioButton);
  EXPECT_EQ(ui.cursorVisibilityLayout->itemAt(1)->widget(), ui.cursorNeverRadioButton);
  EXPECT_EQ(ui.cursorVisibilityLayout->itemAt(2)->widget(), ui.cursorAlwaysRadioButton);

  pane.resize(520, 760);
  ui.rootLayout->activate();
  ui.userInterfaceLayout->activate();
  ui.selectionLayout->activate();
  EXPECT_LT(ui.languageLabel->geometry().right(), ui.languageComboBox->geometry().left());
  EXPECT_LE(ui.languageComboBox->geometry().right(), ui.userInterfaceGroup->contentsRect().right());
  EXPECT_LE(ui.useCoversCheckBox->geometry().right(),
            ui.userInterfaceGroup->contentsRect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.languageComboBox), ui.themeComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.themeComboBox), ui.styleComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.styleComboBox), ui.useBuiltinTitleDatabaseCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.useBuiltinTitleDatabaseCheckBox), ui.useCoversCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.timeTrackingCheckBox), ui.keepWindowOnTopCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.pauseOnFocusLossCheckBox), ui.cursorOnMovementRadioButton);
  EXPECT_EQ(NextTabFocusWidget(ui.cursorOnMovementRadioButton), ui.cursorNeverRadioButton);
  EXPECT_EQ(NextTabFocusWidget(ui.cursorNeverRadioButton), ui.cursorAlwaysRadioButton);
  EXPECT_EQ(NextTabFocusWidget(ui.cursorAlwaysRadioButton), ui.lockMouseCursorCheckBox);
}
