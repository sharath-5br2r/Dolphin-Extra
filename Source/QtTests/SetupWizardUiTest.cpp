// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMargins>
#include <QPushButton>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <gtest/gtest.h>

#include "ui_SetupWizardDialog.h"

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

TEST(SetupWizardUiTest, FormOwnsTheFivePageSetupFlow)
{
  QDialog dialog;
  Ui::SetupWizardDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.minimumSize(), QSize(760, 540));
  EXPECT_TRUE(dialog.isSizeGripEnabled());
  EXPECT_EQ(ui.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.rootLayout->spacing(), 0);
  ASSERT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->layout(), ui.contentLayout);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.footerFrame);

  EXPECT_EQ(ui.contentLayout->spacing(), 0);
  ASSERT_EQ(ui.contentLayout->count(), 2);
  EXPECT_EQ(ui.contentLayout->itemAt(0)->widget(), ui.sidebarFrame);
  EXPECT_EQ(ui.contentLayout->itemAt(1)->widget(), ui.pages);

  EXPECT_EQ(ui.sidebarFrame->minimumWidth(), 190);
  EXPECT_EQ(ui.sidebarFrame->maximumWidth(), 220);
  EXPECT_EQ(ui.sidebarLayout->contentsMargins(), QMargins(24, 24, 24, 24));
  EXPECT_EQ(ui.logoLabel->minimumSize(), QSize(112, 112));
  EXPECT_EQ(ui.logoLabel->maximumSize(), QSize(112, 112));
  EXPECT_EQ(ui.logoLabel->accessibleName(), QStringLiteral("Dolphin logo"));
  EXPECT_EQ(ui.appearancePageLabel->text(), QStringLiteral("Appearance"));
  EXPECT_EQ(ui.gameFoldersPageLabel->text(), QStringLiteral("Game Folders"));
  EXPECT_EQ(ui.controllersPageLabel->text(), QStringLiteral("Controllers"));
  EXPECT_EQ(ui.privacyPageLabel->text(), QStringLiteral("Privacy"));
  EXPECT_EQ(ui.completePageLabel->text(), QStringLiteral("Complete"));

  ASSERT_EQ(ui.pages->count(), 5);
  EXPECT_EQ(ui.pages->widget(0), ui.appearancePage);
  EXPECT_EQ(ui.pages->widget(1), ui.gameFoldersPage);
  EXPECT_EQ(ui.pages->widget(2), ui.controllersPage);
  EXPECT_EQ(ui.pages->widget(3), ui.privacyPage);
  EXPECT_EQ(ui.pages->widget(4), ui.completePage);
  EXPECT_EQ(ui.pages->currentWidget(), ui.appearancePage);

  for (QVBoxLayout* layout : {ui.appearanceLayout, ui.gameFoldersLayout, ui.controllersLayout,
                              ui.privacyLayout, ui.completeLayout})
  {
    EXPECT_EQ(layout->contentsMargins(), QMargins(32, 28, 32, 28));
    EXPECT_EQ(layout->spacing(), 16);
  }
}

TEST(SetupWizardUiTest, ControlsAreCompactAccessibleAndKeyboardOrdered)
{
  QDialog dialog;
  Ui::SetupWizardDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.languageComboBox->accessibleName(), QStringLiteral("Interface language"));
  EXPECT_EQ(ui.languageComboBox->sizeAdjustPolicy(),
            QComboBox::SizeAdjustPolicy::AdjustToMinimumContentsLengthWithIcon);
  EXPECT_EQ(ui.languageComboBox->minimumContentsLength(), 20);
  EXPECT_EQ(ui.styleComboBox->accessibleName(), QStringLiteral("Interface style"));

  EXPECT_EQ(ui.gameFoldersListWidget->accessibleName(), QStringLiteral("Game folders"));
  EXPECT_EQ(ui.gameFoldersListWidget->selectionMode(),
            QAbstractItemView::SelectionMode::SingleSelection);
  EXPECT_FALSE(ui.removeGameFolderButton->isEnabled());
  EXPECT_TRUE(ui.openControllerSettingsCheckBox->isChecked());
  EXPECT_FALSE(ui.analyticsCheckBox->isChecked());

  EXPECT_FALSE(ui.backButton->autoDefault());
  EXPECT_TRUE(ui.nextButton->isDefault());
  EXPECT_FALSE(ui.cancelButton->autoDefault());
  EXPECT_EQ(ui.footerLayout->contentsMargins(), QMargins(12, 10, 12, 10));

  EXPECT_EQ(NextTabFocusWidget(ui.languageComboBox), ui.styleComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.styleComboBox), ui.gameFoldersListWidget);
  EXPECT_EQ(NextTabFocusWidget(ui.gameFoldersListWidget), ui.recursivePathsCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.recursivePathsCheckBox), ui.addGameFolderButton);
  EXPECT_EQ(NextTabFocusWidget(ui.addGameFolderButton), ui.removeGameFolderButton);
  EXPECT_EQ(NextTabFocusWidget(ui.removeGameFolderButton), ui.openControllerSettingsCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.openControllerSettingsCheckBox), ui.analyticsCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.analyticsCheckBox), ui.backButton);
  EXPECT_EQ(NextTabFocusWidget(ui.backButton), ui.nextButton);
  EXPECT_EQ(NextTabFocusWidget(ui.nextButton), ui.cancelButton);
}
