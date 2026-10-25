// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <gtest/gtest.h>

#include "ui_ChunkedProgressDialog.h"
#include "ui_GameDigestDialog.h"
#include "ui_GameListDialog.h"
#include "ui_NetPlayBrowser.h"
#include "ui_NetPlayDialog.h"
#include "ui_NetPlaySetupDialog.h"
#include "ui_PadMappingDialog.h"

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

TEST(NetPlayUiTest, SetupFormOwnsConnectionAndHostTabs)
{
  QDialog dialog;
  Ui::NetPlaySetupDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("NetPlay Setup"));
  EXPECT_EQ(dialog.size(), QSize(768, 540));
  EXPECT_EQ(ui.rootLayout->itemAtPosition(0, 0)->widget(), ui.connectionTypeLabel);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(0, 1)->widget(), ui.connectionTypeCombo);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(0, 2)->widget(), ui.resetTraversalButton);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(1, 1)->widget(), ui.nicknameEdit);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(2, 0)->widget(), ui.tabWidget);
  EXPECT_EQ(ui.rootLayout->itemAtPosition(3, 0)->widget(), ui.buttonBox);

  ASSERT_EQ(ui.connectionTypeCombo->count(), 2);
  EXPECT_EQ(ui.connectionTypeCombo->itemText(0), QStringLiteral("Direct Connection"));
  EXPECT_EQ(ui.connectionTypeCombo->itemText(1), QStringLiteral("Traversal Server"));
  EXPECT_EQ(ui.connectionTypeLabel->buddy(), ui.connectionTypeCombo);
  EXPECT_EQ(ui.nicknameLabel->buddy(), ui.nicknameEdit);

  ASSERT_EQ(ui.tabWidget->count(), 2);
  EXPECT_EQ(ui.tabWidget->widget(0), ui.connectTab);
  EXPECT_EQ(ui.tabWidget->widget(1), ui.hostTab);
  EXPECT_EQ(ui.tabWidget->tabText(0), QStringLiteral("Connect"));
  EXPECT_EQ(ui.tabWidget->tabText(1), QStringLiteral("Host"));
  EXPECT_EQ(ui.connectLayout->itemAtPosition(0, 0)->widget(), ui.ipLabel);
  EXPECT_EQ(ui.connectLayout->itemAtPosition(0, 1)->widget(), ui.ipEdit);
  EXPECT_EQ(ui.connectLayout->itemAtPosition(0, 2)->widget(), ui.connectPortLabel);
  EXPECT_EQ(ui.connectLayout->itemAtPosition(0, 3)->widget(), ui.connectPortSpinBox);
  EXPECT_EQ(ui.connectLayout->itemAtPosition(1, 0)->widget(), ui.alertLabel);
  EXPECT_EQ(ui.connectLayout->itemAtPosition(3, 3)->widget(), ui.connectButton);
  EXPECT_EQ(ui.alertLabel->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
  EXPECT_EQ(ui.connectPortSpinBox->maximum(), 65535);

  EXPECT_EQ(ui.hostLayout->itemAtPosition(0, 0)->widget(), ui.hostPortLabel);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(0, 1)->widget(), ui.hostPortSpinBox);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(0, 2)->widget(), ui.hostUpnpCheckBox);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(1, 0)->widget(), ui.showInBrowserCheckBox);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(1, 1)->widget(), ui.hostRegionCombo);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(1, 2)->widget(), ui.hostNameEdit);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(1, 3)->widget(), ui.hostPasswordEdit);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(2, 0)->widget(), ui.hostGamesList);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(3, 0)->widget(), ui.forceListenPortCheckBox);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(4, 0)->widget(), ui.chunkedUploadLimitCheckBox);
  EXPECT_EQ(ui.hostLayout->itemAtPosition(4, 3)->widget(), ui.hostButton);
  EXPECT_EQ(ui.chunkedUploadLimitSpinBox->minimum(), 1);
  EXPECT_EQ(ui.chunkedUploadLimitSpinBox->maximum(), 1000000);
  EXPECT_EQ(ui.chunkedUploadLimitSpinBox->singleStep(), 100);
  EXPECT_FALSE(ui.resetTraversalButton->autoDefault());
  EXPECT_FALSE(ui.connectButton->autoDefault());
  EXPECT_FALSE(ui.hostButton->autoDefault());

  EXPECT_EQ(NextTabFocusWidget(ui.connectionTypeCombo), ui.resetTraversalButton);
  EXPECT_EQ(NextTabFocusWidget(ui.resetTraversalButton), ui.nicknameEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.nicknameEdit), ui.tabWidget);
}

TEST(NetPlayUiTest, BrowserFormOwnsSessionTableAndFilters)
{
  QDialog dialog;
  Ui::NetPlayBrowser ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("NetPlay Session Browser"));
  EXPECT_EQ(dialog.size(), QSize(750, 500));
  ASSERT_EQ(ui.rootLayout->count(), 4);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.tableWidget);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.filterBox);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.statusLabel);
  EXPECT_EQ(ui.rootLayout->itemAt(3)->layout(), ui.buttonLayout);
  EXPECT_EQ(ui.tableWidget->selectionMode(), QAbstractItemView::SingleSelection);
  EXPECT_EQ(ui.tableWidget->selectionBehavior(), QAbstractItemView::SelectRows);
  EXPECT_FALSE(ui.tableWidget->wordWrap());
  EXPECT_FALSE(ui.tableWidget->tabKeyNavigation());

  EXPECT_EQ(ui.filterLayout->itemAtPosition(0, 0)->widget(), ui.regionLabel);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(0, 1)->widget(), ui.regionCombo);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(1, 1)->widget(), ui.nameEdit);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(2, 1)->widget(), ui.gameIdEdit);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(3, 1)->widget(), ui.radioAll);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(3, 2)->widget(), ui.radioPublic);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(3, 3)->widget(), ui.radioPrivate);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(4, 1)->widget(), ui.hideIncompatibleCheckBox);
  EXPECT_EQ(ui.filterLayout->itemAtPosition(5, 1)->widget(), ui.hideInGameCheckBox);
  EXPECT_EQ(ui.regionCombo->currentText(), QStringLiteral("Any Region"));
  EXPECT_EQ(ui.regionCombo->sizePolicy().horizontalPolicy(), QSizePolicy::Maximum);
  EXPECT_TRUE(ui.radioAll->isChecked());
  EXPECT_TRUE(ui.hideIncompatibleCheckBox->isChecked());
  EXPECT_FALSE(ui.hideInGameCheckBox->isChecked());

  ASSERT_EQ(ui.buttonLayout->count(), 3);
  EXPECT_EQ(ui.buttonLayout->itemAt(0)->widget(), ui.refreshButton);
  EXPECT_NE(ui.buttonLayout->itemAt(1)->spacerItem(), nullptr);
  EXPECT_EQ(ui.buttonLayout->itemAt(2)->widget(), ui.buttonBox);
  EXPECT_FALSE(ui.refreshButton->autoDefault());
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

  EXPECT_EQ(NextTabFocusWidget(ui.tableWidget), ui.regionCombo);
  EXPECT_EQ(NextTabFocusWidget(ui.regionCombo), ui.nameEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.nameEdit), ui.gameIdEdit);
}

TEST(NetPlayUiTest, RoomFormOwnsMenusChatPlayersAndControls)
{
  QDialog dialog;
  Ui::NetPlayDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("NetPlay"));
  ASSERT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.rootLayout->spacing(), 0);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.menuBar);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.contentWidget);
  EXPECT_EQ(ui.menuBar->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);

  ASSERT_EQ(ui.menuBar->actions().size(), 4);
  EXPECT_EQ(ui.menuBar->actions()[0]->menu(), ui.dataMenu);
  EXPECT_EQ(ui.menuBar->actions()[1]->menu(), ui.networkMenu);
  EXPECT_EQ(ui.menuBar->actions()[2]->menu(), ui.gameDigestMenu);
  EXPECT_EQ(ui.menuBar->actions()[3]->menu(), ui.otherMenu);
  EXPECT_EQ(ui.dataMenu->actions().size(), 8);
  EXPECT_EQ(ui.networkMenu->actions().size(), 3);
  EXPECT_EQ(ui.gameDigestMenu->actions().size(), 3);
  EXPECT_EQ(ui.otherMenu->actions().size(), 3);
  EXPECT_TRUE(ui.actionSavedataLoadOnly->isChecked());
  EXPECT_TRUE(ui.actionSyncCodes->isChecked());
  EXPECT_TRUE(ui.actionFixedDelay->isChecked());
  EXPECT_TRUE(ui.actionSavedataNone->isCheckable());
  EXPECT_TRUE(ui.actionHostInputAuthority->isCheckable());
  EXPECT_TRUE(ui.actionRecordInputs->isCheckable());

  ASSERT_EQ(ui.contentLayout->count(), 3);
  EXPECT_EQ(ui.contentLayout->itemAt(0)->widget(), ui.gameButton);
  EXPECT_EQ(ui.contentLayout->itemAt(1)->widget(), ui.splitter);
  EXPECT_EQ(ui.contentLayout->itemAt(2)->layout(), ui.optionsLayout);
  ASSERT_EQ(ui.splitter->count(), 2);
  EXPECT_EQ(ui.splitter->widget(0), ui.chatBox);
  EXPECT_EQ(ui.splitter->widget(1), ui.playersBox);

  EXPECT_EQ(ui.chatLayout->itemAtPosition(0, 0)->widget(), ui.chatEdit);
  EXPECT_EQ(ui.chatLayout->itemAtPosition(1, 0)->widget(), ui.chatTypeEdit);
  EXPECT_EQ(ui.chatLayout->itemAtPosition(1, 1)->widget(), ui.chatSendButton);
  EXPECT_FALSE(ui.chatSendButton->isEnabled());
  EXPECT_FALSE(ui.chatSendButton->autoDefault());

  EXPECT_EQ(ui.playersLayout->itemAtPosition(0, 0)->widget(), ui.roomBox);
  EXPECT_EQ(ui.playersLayout->itemAtPosition(0, 1)->widget(), ui.hostcodeLabel);
  EXPECT_EQ(ui.playersLayout->itemAtPosition(0, 2)->widget(), ui.hostcodeActionButton);
  EXPECT_EQ(ui.playersLayout->itemAtPosition(1, 0)->widget(), ui.playersList);
  EXPECT_EQ(ui.playersLayout->itemAtPosition(2, 0)->widget(), ui.kickButton);
  EXPECT_EQ(ui.playersLayout->itemAtPosition(3, 0)->widget(), ui.assignPortsButton);
  EXPECT_EQ(ui.playersList->columnCount(), 5);
  EXPECT_EQ(ui.playersList->selectionBehavior(), QAbstractItemView::SelectRows);
  EXPECT_FALSE(ui.playersList->tabKeyNavigation());

  ASSERT_EQ(ui.optionsLayout->count(), 5);
  EXPECT_EQ(ui.optionsLayout->itemAt(0)->widget(), ui.startButton);
  EXPECT_EQ(ui.optionsLayout->itemAt(1)->widget(), ui.bufferLabel);
  EXPECT_EQ(ui.optionsLayout->itemAt(2)->widget(), ui.bufferSizeSpinBox);
  EXPECT_NE(ui.optionsLayout->itemAt(3)->spacerItem(), nullptr);
  EXPECT_EQ(ui.optionsLayout->itemAt(4)->widget(), ui.quitButton);
}

TEST(NetPlayUiTest, SelectionAndMappingFormsOwnTheirStaticRows)
{
  QDialog game_dialog;
  Ui::GameListDialog game_list;
  game_list.setupUi(&game_dialog);
  EXPECT_EQ(game_dialog.windowTitle(), QStringLiteral("Select a game"));
  ASSERT_EQ(game_list.rootLayout->count(), 2);
  EXPECT_EQ(game_list.rootLayout->itemAt(0)->widget(), game_list.gameList);
  EXPECT_EQ(game_list.rootLayout->itemAt(1)->widget(), game_list.buttonBox);
  EXPECT_FALSE(game_list.buttonBox->isEnabled());
  EXPECT_EQ(game_list.buttonBox->standardButtons(), QDialogButtonBox::Ok);

  QDialog mapping_dialog;
  Ui::PadMappingDialog mapping;
  mapping.setupUi(&mapping_dialog);
  EXPECT_EQ(mapping_dialog.windowTitle(), QStringLiteral("Assign Controllers"));
  EXPECT_EQ(mapping.rootLayout->itemAtPosition(0, 0)->widget(), mapping.gcPort1Label);
  EXPECT_EQ(mapping.rootLayout->itemAtPosition(1, 0)->widget(), mapping.gcBox1);
  EXPECT_EQ(mapping.rootLayout->itemAtPosition(2, 0)->widget(), mapping.gbaBox1);
  EXPECT_EQ(mapping.rootLayout->itemAtPosition(3, 0)->widget(), mapping.wiiRemote1Label);
  EXPECT_EQ(mapping.rootLayout->itemAtPosition(4, 0)->widget(), mapping.wiiBox1);
  EXPECT_EQ(mapping.rootLayout->itemAtPosition(5, 0)->widget(), mapping.buttonBox);
  EXPECT_EQ(mapping.buttonBox->standardButtons(), QDialogButtonBox::Ok);
}

TEST(NetPlayUiTest, ProgressFormsOwnDynamicContentContainers)
{
  QDialog transfer_dialog;
  Ui::ChunkedProgressDialog transfer;
  transfer.setupUi(&transfer_dialog);
  EXPECT_EQ(transfer_dialog.windowTitle(), QStringLiteral("Data Transfer"));
  ASSERT_EQ(transfer.rootLayout->count(), 2);
  EXPECT_EQ(transfer.rootLayout->itemAt(0)->widget(), transfer.progressBox);
  EXPECT_EQ(transfer.rootLayout->itemAt(1)->widget(), transfer.buttonBox);
  EXPECT_EQ(transfer.progressBox->layout(), transfer.progressLayout);
  EXPECT_EQ(transfer.progressLayout->count(), 0);
  EXPECT_EQ(transfer.buttonBox->standardButtons(), QDialogButtonBox::NoButton);

  QDialog digest_dialog;
  Ui::GameDigestDialog digest;
  digest.setupUi(&digest_dialog);
  EXPECT_EQ(digest_dialog.windowTitle(), QStringLiteral("SHA1 Digest"));
  ASSERT_EQ(digest.rootLayout->count(), 3);
  EXPECT_EQ(digest.rootLayout->itemAt(0)->widget(), digest.progressBox);
  EXPECT_EQ(digest.rootLayout->itemAt(1)->widget(), digest.checkLabel);
  EXPECT_EQ(digest.rootLayout->itemAt(2)->widget(), digest.buttonBox);
  EXPECT_EQ(digest.progressBox->layout(), digest.progressLayout);
  EXPECT_EQ(digest.progressLayout->count(), 0);
  EXPECT_TRUE(digest.checkLabel->text().isEmpty());
  EXPECT_EQ(digest.buttonBox->standardButtons(), QDialogButtonBox::NoButton);
}
