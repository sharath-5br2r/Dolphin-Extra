// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_AboutDialog.h"
#include "ui_DiscordJoinRequestDialog.h"
#include "ui_GCMemcardCreateNewDialog.h"
#include "ui_NANDRepairDialog.h"
#include "ui_NKitWarningDialog.h"

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

TEST(RootUtilityDialogsUiTest, AboutFormOwnsFixedTwoPartLayout)
{
  QDialog dialog;
  Ui::AboutDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("About Dolphin"));
  EXPECT_EQ(ui.mainLayout->sizeConstraint(), QLayout::SetFixedSize);
  ASSERT_EQ(ui.mainLayout->count(), 2);
  EXPECT_EQ(ui.mainLayout->itemAt(0)->layout(), ui.contentLayout);
  EXPECT_EQ(ui.mainLayout->itemAt(1)->layout(), ui.copyrightLayout);
  EXPECT_EQ(ui.contentLayout->alignment(), Qt::AlignLeft);
  EXPECT_EQ(ui.logoLayout->contentsMargins(), QMargins(30, 0, 30, 0));
  EXPECT_EQ(ui.copyrightLayout->contentsMargins(), QMargins(0, 15, 0, 0));
  EXPECT_TRUE(ui.textLabel->openExternalLinks());
  EXPECT_EQ(ui.textLabel->textInteractionFlags(), Qt::TextBrowserInteraction);
  EXPECT_EQ(ui.copyrightLabel->alignment(), Qt::AlignCenter);
}

TEST(RootUtilityDialogsUiTest, DiscordFormOwnsOptionalAvatarAndReplyButtons)
{
  QDialog dialog;
  Ui::DiscordJoinRequestDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Request to Join Your Party"));
  EXPECT_TRUE(ui.avatarLabel->isHidden());
  EXPECT_EQ(ui.mainLayout->itemAtPosition(0, 0)->widget(), ui.avatarLabel);
  EXPECT_EQ(ui.mainLayout->itemAtPosition(1, 0)->widget(), ui.requestLabel);
  EXPECT_EQ(ui.mainLayout->itemAtPosition(2, 0)->widget(), ui.inviteButton);
  EXPECT_EQ(ui.mainLayout->itemAtPosition(2, 1)->widget(), ui.declineButton);
  EXPECT_EQ(ui.mainLayout->itemAtPosition(2, 2)->widget(), ui.ignoreButton);
  EXPECT_EQ(NextTabFocusWidget(ui.inviteButton), ui.declineButton);
  EXPECT_EQ(NextTabFocusWidget(ui.declineButton), ui.ignoreButton);
}

TEST(RootUtilityDialogsUiTest, MemoryCardFormOwnsChoicesDefaultsAndButtonBox)
{
  QDialog dialog;
  Ui::GCMemcardCreateNewDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Create New Memory Card"));
  EXPECT_EQ(ui.cardSizeLabel->buddy(), ui.cardSizeComboBox);
  EXPECT_EQ(ui.encodingLabel->buddy(), ui.westernRadioButton);
  ASSERT_EQ(ui.cardSizeComboBox->count(), 6);
  EXPECT_EQ(ui.cardSizeComboBox->itemText(0), QStringLiteral("4 Mbit (59 blocks)"));
  EXPECT_EQ(ui.cardSizeComboBox->itemText(5), QStringLiteral("128 Mbit (2043 blocks)"));
  EXPECT_EQ(ui.cardSizeComboBox->currentIndex(), 5);
  EXPECT_TRUE(ui.westernRadioButton->isChecked());
  EXPECT_FALSE(ui.shiftJisRadioButton->isChecked());
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  EXPECT_EQ(NextTabFocusWidget(ui.cardSizeComboBox), ui.westernRadioButton);
  EXPECT_EQ(NextTabFocusWidget(ui.westernRadioButton), ui.shiftJisRadioButton);
}

TEST(RootUtilityDialogsUiTest, NandRepairFormOwnsOptionalRemovalSection)
{
  QDialog dialog;
  Ui::NANDRepairDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("NAND Check"));
  EXPECT_EQ(dialog.size(), QSize(600, 400));
  ASSERT_EQ(ui.topLayout->count(), 3);
  ASSERT_EQ(ui.mainLayout->count(), 6);
  EXPECT_TRUE(ui.damagedLabel->wordWrap());
  EXPECT_TRUE(ui.warningLabel->wordWrap());
  EXPECT_TRUE(ui.titleBox->isReadOnly());
  EXPECT_TRUE(ui.maybeFixLabel->wordWrap());
  EXPECT_TRUE(ui.questionLabel->wordWrap());
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Yes | QDialogButtonBox::No);
}

TEST(RootUtilityDialogsUiTest, NkitFormOwnsWarningDecisionAndButtonOrder)
{
  QDialog dialog;
  Ui::NKitWarningDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("NKit Warning"));
  ASSERT_EQ(ui.topLayout->count(), 3);
  ASSERT_EQ(ui.mainLayout->count(), 4);
  EXPECT_TRUE(ui.warningLabel->wordWrap());
  EXPECT_FALSE(ui.acceptCheckBox->isChecked());
  EXPECT_FALSE(ui.skipCheckBox->isChecked());
  EXPECT_FALSE(ui.okButton->isEnabled());
  EXPECT_EQ(ui.buttonLayout->itemAt(0)->widget(), ui.okButton);
  EXPECT_EQ(ui.buttonLayout->itemAt(1)->widget(), ui.cancelButton);
  EXPECT_EQ(NextTabFocusWidget(ui.acceptCheckBox), ui.skipCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.skipCheckBox), ui.okButton);
  EXPECT_EQ(NextTabFocusWidget(ui.okButton), ui.cancelButton);
}
