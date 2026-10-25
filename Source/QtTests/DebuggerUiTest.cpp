// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_AssembleInstructionDialog.h"
#include "ui_AssemblerWidget.h"
#include "ui_BranchWatchDialog.h"
#include "ui_BreakpointDialog.h"
#include "ui_BreakpointWidget.h"
#include "ui_CodeWidget.h"
#include "ui_EditSymbolDialog.h"
#include "ui_JITWidget.h"
#include "ui_MemoryViewWidget.h"
#include "ui_MemoryWidget.h"
#include "ui_NetworkWidget.h"
#include "ui_PatchInstructionDialog.h"
#include "ui_RegisterWidget.h"
#include "ui_ThreadWidget.h"
#include "ui_WatchWidget.h"

TEST(DebuggerUiTest, InstructionDialogsOwnPermanentControls)
{
  QDialog assemble_dialog;
  Ui::AssembleInstructionDialog assemble_ui;
  assemble_ui.setupUi(&assemble_dialog);

  EXPECT_EQ(assemble_ui.mainLayout->count(), 5);
  EXPECT_EQ(assemble_ui.messageLabel->text(), QStringLiteral("No input"));
  EXPECT_EQ(assemble_ui.buttonBox->standardButtons(),
            QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

  QDialog patch_dialog;
  Ui::PatchInstructionDialog patch_ui;
  patch_ui.setupUi(&patch_dialog);

  EXPECT_EQ(patch_ui.mainLayout->count(), 4);
  EXPECT_EQ(patch_ui.instructionLabel->text(), QStringLiteral("New instruction:"));
  EXPECT_EQ(patch_ui.instructionLabel->buddy(), patch_ui.inputEdit);
  EXPECT_EQ(patch_ui.buttonBox->standardButtons(), QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
}

TEST(DebuggerUiTest, EditSymbolFormOwnsSizeRow)
{
  QDialog dialog;
  Ui::EditSymbolDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.mainLayout->count(), 4);
  EXPECT_EQ(ui.addressEndLabel->buddy(), ui.addressEndEdit);
  EXPECT_EQ(ui.sizeHexLabel->buddy(), ui.sizeHexEdit);
  EXPECT_EQ(ui.sizeLinesLabel->buddy(), ui.sizeLinesSpinBox);
  EXPECT_EQ(ui.sizeHexEdit->maxLength(), 7);
  EXPECT_EQ(ui.sizeLinesSpinBox->maximum(), 99999);
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
}

TEST(DebuggerUiTest, BreakpointFormPreservesGroupsAndDefaults)
{
  QDialog dialog;
  Ui::BreakpointDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.mainLayout->count(), 4);
  EXPECT_EQ(ui.instructionAddressLabel->buddy(), ui.instructionAddressEdit);
  EXPECT_TRUE(ui.memoryUseAddressRadio->isChecked());
  EXPECT_TRUE(ui.memoryOnWriteRadio->isChecked());
  EXPECT_TRUE(ui.writeLogAndBreakRadio->isChecked());
  EXPECT_EQ(ui.memoryConditionBox->title(), QStringLiteral("Condition"));
  EXPECT_EQ(ui.actionBox->title(), QStringLiteral("Action"));
  EXPECT_EQ(ui.buttonBox->standardButtons(),
            QDialogButtonBox::Cancel | QDialogButtonBox::Help | QDialogButtonBox::Ok);
}

TEST(DebuggerUiTest, TableDockFormsOwnShellProperties)
{
  QWidget breakpoint;
  Ui::BreakpointWidget breakpoint_ui;
  breakpoint_ui.setupUi(&breakpoint);
  EXPECT_EQ(breakpoint_ui.mainLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_EQ(breakpoint_ui.mainLayout->spacing(), 0);
  EXPECT_EQ(breakpoint_ui.toolbar->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
  EXPECT_FALSE(breakpoint_ui.table->tabKeyNavigation());
  EXPECT_EQ(breakpoint_ui.table->selectionMode(), QAbstractItemView::NoSelection);

  QWidget watch;
  Ui::WatchWidget watch_ui;
  watch_ui.setupUi(&watch);
  EXPECT_EQ(watch_ui.mainLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_EQ(watch_ui.mainLayout->spacing(), 0);
  EXPECT_EQ(watch_ui.table->selectionMode(), QAbstractItemView::ExtendedSelection);
  EXPECT_EQ(watch_ui.table->selectionBehavior(), QAbstractItemView::SelectRows);

  QWidget registers;
  Ui::RegisterWidget register_ui;
  register_ui.setupUi(&registers);
  EXPECT_EQ(register_ui.mainLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_FALSE(register_ui.table->tabKeyNavigation());
  EXPECT_EQ(register_ui.table->selectionMode(), QAbstractItemView::NoSelection);
}

TEST(DebuggerUiTest, MemoryViewFormOwnsZeroMarginInsertionLayout)
{
  QWidget widget;
  Ui::MemoryViewWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.mainLayout->count(), 0);
}

TEST(DebuggerUiTest, AssemblerFormOwnsInputOutputAndErrorRegions)
{
  QWidget widget;
  Ui::AssemblerWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins(5, 0, 5, 5));
  EXPECT_EQ(ui.mainLayout->spacing(), 0);
  EXPECT_TRUE(ui.assemblerTabs->tabsClosable());
  EXPECT_EQ(ui.baseAddressLabel->buddy(), ui.addressEdit);
  EXPECT_EQ(ui.outputTypeCombo->count(), 5);
  EXPECT_TRUE(ui.outputEdit->isReadOnly());
  EXPECT_TRUE(ui.errorEdit->isReadOnly());
}

TEST(DebuggerUiTest, CodeFormOwnsSplittersAndSearchPanels)
{
  QWidget widget;
  Ui::CodeWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_EQ(ui.mainLayout->spacing(), 0);
  EXPECT_EQ(ui.codeSplitter->count(), 2);
  EXPECT_EQ(ui.boxSplitter->count(), 4);
  EXPECT_EQ(ui.boxSplitter->orientation(), Qt::Vertical);
  EXPECT_EQ(ui.callstackLabel->buddy(), ui.searchCallstackEdit);
  EXPECT_EQ(ui.symbolsTabs->count(), 2);
  EXPECT_TRUE(ui.lockButton->isCheckable());
  EXPECT_EQ(ui.codeViewLayout->count(), 0);
}

TEST(DebuggerUiTest, JitFormOwnsFiltersAndDisassemblySplitters)
{
  QWidget widget;
  Ui::JITWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_EQ(ui.mainLayout->spacing(), 0);
  EXPECT_EQ(ui.controlsLayout->count(), 7);
  EXPECT_TRUE(ui.toggleProfilingButton->isCheckable());
  EXPECT_EQ(ui.tableView->selectionMode(), QAbstractItemView::ExtendedSelection);
  EXPECT_EQ(ui.tableSplitter->count(), 2);
  EXPECT_EQ(ui.disassemblySplitter->count(), 3);
  EXPECT_TRUE(ui.ppcAssemblyEdit->isReadOnly());
  EXPECT_EQ(ui.statusLayout->count(), 0);
}

TEST(DebuggerUiTest, NetworkFormOwnsTablesAndOptions)
{
  QWidget widget;
  Ui::NetworkWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->count(), 5);
  EXPECT_EQ(ui.socketTableLayout->spacing(), 1);
  EXPECT_EQ(ui.socketTable->selectionMode(), QAbstractItemView::NoSelection);
  EXPECT_EQ(ui.sslTable->selectionMode(), QAbstractItemView::NoSelection);
  EXPECT_EQ(ui.dumpFormatLabel->buddy(), ui.dumpFormatCombo);
  EXPECT_EQ(ui.dumpFormatCombo->count(), 5);
  EXPECT_EQ(ui.dumpOptionsLayout->spacing(), 1);
  EXPECT_EQ(ui.securityOptionsLayout->spacing(), 1);
}

TEST(DebuggerUiTest, ThreadFormOwnsStateAndTableSections)
{
  QWidget widget;
  Ui::ThreadWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->count(), 5);
  EXPECT_EQ(ui.stateLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_EQ(ui.stateLayout->spacing(), 1);
  EXPECT_EQ(ui.contextLayout->columnStretch(0), 1);
  EXPECT_EQ(ui.contextLayout->columnStretch(1), 1);
  EXPECT_EQ(ui.queueLayout->columnStretch(0), 1);
  EXPECT_EQ(ui.queueLayout->columnStretch(1), 1);
  EXPECT_EQ(ui.currentContextLabel->buddy(), ui.currentContextEdit);
  EXPECT_TRUE(ui.currentContextEdit->isReadOnly());
  EXPECT_EQ(ui.threadTable->selectionBehavior(), QAbstractItemView::SelectRows);
  EXPECT_EQ(ui.contextTable->rowCount(), 32);
  EXPECT_EQ(ui.contextTable->columnCount(), 8);
}

TEST(DebuggerUiTest, MemoryFormOwnsSidebarAndCustomViewInsertionPoint)
{
  QWidget widget;
  Ui::MemoryWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins(2, 2, 2, 2));
  EXPECT_EQ(ui.mainLayout->spacing(), 0);
  EXPECT_EQ(ui.splitter->count(), 2);
  EXPECT_EQ(ui.memoryViewLayout->count(), 0);
  EXPECT_TRUE(ui.sidebarScrollArea->widgetResizable());
  EXPECT_FALSE(ui.menuBar->isNativeMenuBar());
  EXPECT_EQ(ui.sidebarLayout->spacing(), 1);
  EXPECT_EQ(ui.addressSplitter->handleWidth(), 1);
  EXPECT_EQ(ui.labelsTabs->count(), 3);
}

TEST(DebuggerUiTest, BranchWatchFormOwnsToolbarGroupsTableAndStatus)
{
  QDialog dialog;
  Ui::BranchWatchDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.mainLayout->count(), 4);
  EXPECT_FALSE(ui.menuBar->isNativeMenuBar());
  EXPECT_EQ(ui.tableView->selectionMode(), QAbstractItemView::ExtendedSelection);
  EXPECT_EQ(ui.tableView->selectionBehavior(), QAbstractItemView::SelectRows);
  EXPECT_FALSE(ui.statusBar->isSizeGripEnabled());
  EXPECT_EQ(ui.toolControlsLayout->count(), 4);
  EXPECT_EQ(ui.branchTypeLayout->count(), 12);
  EXPECT_TRUE(ui.branchBCheckBox->isChecked());
  EXPECT_TRUE(ui.branchBcctrlCheckBox->isChecked());
  EXPECT_EQ(ui.originMinEdit->maxLength(), 8);
  EXPECT_EQ(ui.conditionLayout->count(), 2);
  EXPECT_FALSE(ui.wipeRecentHitsButton->isEnabled());
}
