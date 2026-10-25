// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMargins>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_AchievementBox.h"
#include "ui_AchievementHeaderWidget.h"
#include "ui_AchievementLeaderboardCell.h"
#include "ui_AchievementLeaderboardWidget.h"
#include "ui_AchievementProgressWidget.h"
#include "ui_AchievementSettingsWidget.h"
#include "ui_AchievementsWindow.h"

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

TEST(AchievementsUiTest, WindowFormOwnsTheStandaloneDialogShell)
{
  QDialog dialog;
  Ui::AchievementsWindow ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Achievements"));
  ASSERT_EQ(ui.rootLayout->count(), 3);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->layout(), ui.headerLayout);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.tabWidget);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.buttonBox);
  EXPECT_EQ(ui.headerLayout->contentsMargins(), QMargins());

  ASSERT_EQ(ui.tabWidget->count(), 3);
  EXPECT_EQ(ui.tabWidget->widget(0), ui.settingsTab);
  EXPECT_EQ(ui.tabWidget->widget(1), ui.progressTab);
  EXPECT_EQ(ui.tabWidget->widget(2), ui.leaderboardsTab);
  EXPECT_EQ(ui.tabWidget->tabText(0), QStringLiteral("Settings"));
  EXPECT_EQ(ui.tabWidget->tabText(1), QStringLiteral("Progress"));
  EXPECT_EQ(ui.tabWidget->tabText(2), QStringLiteral("Leaderboards"));
  EXPECT_EQ(ui.settingsLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.progressLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.leaderboardsLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Close);
}

TEST(AchievementsUiTest, SettingsFormOwnsStockAccountAndDisplayControls)
{
  QWidget widget;
  Ui::AchievementSettingsWidget ui;
  ui.setupUi(&widget);

  ASSERT_EQ(ui.common_layout->count(), 18);
  EXPECT_EQ(ui.common_layout->alignment(), Qt::AlignTop);
  EXPECT_EQ(ui.common_layout->itemAt(0)->widget(), ui.common_integration_enabled_input);
  EXPECT_EQ(ui.common_layout->itemAt(1)->widget(), ui.common_username_label);
  EXPECT_EQ(ui.common_layout->itemAt(2)->widget(), ui.common_username_input);
  EXPECT_EQ(ui.common_layout->itemAt(3)->widget(), ui.common_password_label);
  EXPECT_EQ(ui.common_layout->itemAt(4)->widget(), ui.common_password_input);
  EXPECT_EQ(ui.common_layout->itemAt(8)->widget(), ui.function_settings_label);
  EXPECT_EQ(ui.common_layout->itemAt(13)->widget(), ui.display_settings_label);
  EXPECT_EQ(ui.common_layout->itemAt(17)->widget(), ui.common_progress_enabled_input);

  EXPECT_EQ(ui.common_username_label->buddy(), ui.common_username_input);
  EXPECT_EQ(ui.common_password_label->buddy(), ui.common_password_input);
  EXPECT_EQ(ui.common_password_input->echoMode(), QLineEdit::Password);
  EXPECT_TRUE(ui.common_login_failed->isHidden());
  EXPECT_TRUE(ui.common_login_failed->styleSheet().contains(QStringLiteral("red")));

  for (QCheckBox* checkbox :
       {ui.common_integration_enabled_input, ui.common_hardcore_enabled_input,
        ui.common_unofficial_enabled_input, ui.common_encore_enabled_input,
        ui.common_spectator_enabled_input, ui.common_discord_presence_enabled_input,
        ui.common_leaderboard_tracker_enabled_input, ui.common_challenge_indicators_enabled_input,
        ui.common_progress_enabled_input})
  {
    EXPECT_STREQ(checkbox->metaObject()->className(), "QCheckBox");
  }

  EXPECT_EQ(NextTabFocusWidget(ui.common_integration_enabled_input), ui.common_username_input);
  EXPECT_EQ(NextTabFocusWidget(ui.common_username_input), ui.common_password_input);
  EXPECT_EQ(NextTabFocusWidget(ui.common_password_input), ui.common_login_button);
  EXPECT_EQ(NextTabFocusWidget(ui.common_login_button), ui.common_logout_button);
  EXPECT_EQ(NextTabFocusWidget(ui.common_logout_button), ui.common_hardcore_enabled_input);
  EXPECT_EQ(NextTabFocusWidget(ui.common_progress_enabled_input),
            ui.common_integration_enabled_input);
}

TEST(AchievementsUiTest, HeaderFormOwnsTheSummaryAndProgressOverlay)
{
  QWidget widget;
  Ui::AchievementHeaderWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.rootLayout->alignment(), Qt::AlignTop);
  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.headerBox);
  ASSERT_EQ(ui.headerLayout->count(), 2);
  EXPECT_EQ(ui.headerLayout->itemAt(0)->layout(), ui.iconColumn);
  EXPECT_EQ(ui.headerLayout->itemAt(1)->layout(), ui.textColumn);
  ASSERT_EQ(ui.iconColumn->count(), 2);
  ASSERT_EQ(ui.textColumn->count(), 4);
  EXPECT_EQ(ui.textColumn->itemAt(2)->widget(), ui.gameProgress);
  EXPECT_EQ(ui.gameProgress->layout(), ui.progressOverlayLayout);
  EXPECT_EQ(ui.progressOverlayLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.progressOverlayLayout->itemAt(0)->widget(), ui.progressLabel);
  EXPECT_EQ(ui.progressLabel->alignment(), Qt::AlignCenter);
  EXPECT_FALSE(ui.gameProgress->isTextVisible());
}

TEST(AchievementsUiTest, AchievementBoxFormOwnsBadgeDetailsAndProgressOverlay)
{
  QGroupBox group;
  Ui::AchievementBox ui;
  ui.setupUi(&group);

  ASSERT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->layout(), ui.badgeColumn);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->layout(), ui.detailsColumn);
  EXPECT_EQ(ui.badgeColumn->sizeConstraint(), QLayout::SetFixedSize);
  EXPECT_EQ(ui.badgeColumn->alignment(), Qt::AlignCenter);
  ASSERT_EQ(ui.badgeColumn->count(), 3);
  EXPECT_NE(ui.badgeColumn->itemAt(0)->spacerItem(), nullptr);
  EXPECT_EQ(ui.badgeColumn->itemAt(1)->widget(), ui.badgeLabel);
  EXPECT_NE(ui.badgeColumn->itemAt(2)->spacerItem(), nullptr);

  ASSERT_EQ(ui.detailsColumn->count(), 5);
  EXPECT_EQ(ui.detailsColumn->itemAt(0)->widget(), ui.titleLabel);
  EXPECT_EQ(ui.detailsColumn->itemAt(1)->widget(), ui.descriptionLabel);
  EXPECT_EQ(ui.detailsColumn->itemAt(2)->widget(), ui.pointsLabel);
  EXPECT_EQ(ui.detailsColumn->itemAt(3)->widget(), ui.statusLabel);
  EXPECT_EQ(ui.detailsColumn->itemAt(4)->widget(), ui.progressBar);
  EXPECT_EQ(ui.progressBar->layout(), ui.progressOverlayLayout);
  EXPECT_EQ(ui.progressOverlayLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.progressOverlayLayout->itemAt(0)->widget(), ui.progressLabel);
  EXPECT_EQ(ui.progressLabel->alignment(), Qt::AlignCenter);
  EXPECT_FALSE(ui.progressBar->isTextVisible());
}

TEST(AchievementsUiTest, DynamicContentFormsOwnTheirContainersAndReusableCell)
{
  QWidget progress_widget;
  Ui::AchievementProgressWidget progress;
  progress.setupUi(&progress_widget);
  EXPECT_EQ(progress.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(progress.rootLayout->sizeConstraint(), QLayout::SetFixedSize);
  EXPECT_EQ(progress.rootLayout->alignment(), Qt::AlignTop);
  ASSERT_EQ(progress.rootLayout->count(), 1);
  EXPECT_EQ(progress.rootLayout->itemAt(0)->widget(), progress.commonBox);
  EXPECT_EQ(progress.commonBox->layout(), progress.commonLayout);
  EXPECT_EQ(progress.commonLayout->count(), 0);

  QWidget leaderboard_widget;
  Ui::AchievementLeaderboardWidget leaderboard;
  leaderboard.setupUi(&leaderboard_widget);
  EXPECT_EQ(leaderboard.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(leaderboard.rootLayout->sizeConstraint(), QLayout::SetFixedSize);
  EXPECT_EQ(leaderboard.rootLayout->alignment(), Qt::AlignTop);
  ASSERT_EQ(leaderboard.rootLayout->count(), 1);
  EXPECT_EQ(leaderboard.rootLayout->itemAt(0)->widget(), leaderboard.commonBox);
  EXPECT_EQ(leaderboard.commonBox->layout(), leaderboard.commonLayout);
  EXPECT_EQ(leaderboard.commonLayout->count(), 0);

  QWidget cell_widget;
  Ui::AchievementLeaderboardCell cell;
  cell.setupUi(&cell_widget);
  EXPECT_EQ(cell.rootLayout->contentsMargins(), QMargins());
  ASSERT_EQ(cell.rootLayout->count(), 3);
  EXPECT_EQ(cell.rootLayout->itemAt(0)->widget(), cell.primaryLabel);
  EXPECT_EQ(cell.rootLayout->itemAt(1)->widget(), cell.secondaryLabel);
  EXPECT_EQ(cell.rootLayout->itemAt(2)->widget(), cell.tertiaryLabel);
  EXPECT_EQ(cell.primaryLabel->text(), QStringLiteral("---"));
  EXPECT_EQ(cell.secondaryLabel->text(), QStringLiteral("---"));
  EXPECT_EQ(cell.tertiaryLabel->text(), QStringLiteral("---"));
}
