// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_PathPane.h"

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

TEST(PathPaneUiTest, FormOwnsThePathsSettingsStructure)
{
  QWidget pane;
  Ui::PathPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.gameFoldersGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->layout(), ui.pathsLayout);

  ASSERT_EQ(ui.gameFoldersLayout->count(), 4);
  EXPECT_EQ(ui.gameFoldersLayout->itemAt(0)->widget(), ui.pathListWidget);
  EXPECT_EQ(ui.gameFoldersLayout->itemAt(1)->layout(), ui.folderButtonsLayout);
  EXPECT_EQ(ui.gameFoldersLayout->itemAt(2)->widget(), ui.recursivePathsCheckBox);
  EXPECT_EQ(ui.gameFoldersLayout->itemAt(3)->widget(), ui.autoRefreshCheckBox);
  EXPECT_EQ(ui.pathListWidget->spacing(), 1);

  ASSERT_EQ(ui.folderButtonsLayout->count(), 3);
  EXPECT_NE(ui.folderButtonsLayout->itemAt(0)->spacerItem(), nullptr);
  EXPECT_EQ(ui.folderButtonsLayout->itemAt(1)->widget(), ui.addPathButton);
  EXPECT_EQ(ui.folderButtonsLayout->itemAt(2)->widget(), ui.removePathButton);
  EXPECT_FALSE(ui.removePathButton->isEnabled());

  const std::array labels{ui.defaultIsoLabel, ui.nandRootLabel,         ui.dumpPathLabel,
                          ui.loadPathLabel,   ui.resourcePackPathLabel, ui.wfsPathLabel};
  const std::array line_edits{ui.defaultIsoLineEdit,       ui.nandRootLineEdit,
                              ui.dumpPathLineEdit,         ui.loadPathLineEdit,
                              ui.resourcePackPathLineEdit, ui.wfsPathLineEdit};
  const std::array browse_buttons{ui.defaultIsoBrowseButton,       ui.nandRootBrowseButton,
                                  ui.dumpPathBrowseButton,         ui.loadPathBrowseButton,
                                  ui.resourcePackPathBrowseButton, ui.wfsPathBrowseButton};

  EXPECT_EQ(ui.pathsLayout->rowCount(), 6);
  EXPECT_EQ(ui.pathsLayout->columnCount(), 3);
  for (int row = 0; row < 6; ++row)
  {
    EXPECT_EQ(ui.pathsLayout->itemAtPosition(row, 0)->widget(), labels[static_cast<size_t>(row)]);
    EXPECT_EQ(ui.pathsLayout->itemAtPosition(row, 1)->widget(),
              line_edits[static_cast<size_t>(row)]);
    EXPECT_EQ(ui.pathsLayout->itemAtPosition(row, 2)->widget(),
              browse_buttons[static_cast<size_t>(row)]);
    EXPECT_EQ(labels[static_cast<size_t>(row)]->buddy(), line_edits[static_cast<size_t>(row)]);
    EXPECT_FALSE(browse_buttons[static_cast<size_t>(row)]->autoDefault());
  }
  EXPECT_FALSE(ui.addPathButton->autoDefault());
  EXPECT_FALSE(ui.removePathButton->autoDefault());

  pane.resize(520, 650);
  ui.rootLayout->setGeometry(pane.rect());
  EXPECT_LT(ui.defaultIsoLabel->geometry().right(), ui.defaultIsoLineEdit->geometry().left());
  EXPECT_LT(ui.defaultIsoLineEdit->geometry().right(),
            ui.defaultIsoBrowseButton->geometry().left());
  EXPECT_LE(ui.defaultIsoBrowseButton->geometry().right(), pane.rect().right());
  EXPECT_LT(ui.resourcePackPathLabel->geometry().right(),
            ui.resourcePackPathLineEdit->geometry().left());
  EXPECT_LE(ui.wfsPathBrowseButton->geometry().right(), pane.rect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.pathListWidget), ui.addPathButton);
  EXPECT_EQ(NextTabFocusWidget(ui.addPathButton), ui.removePathButton);
  EXPECT_EQ(NextTabFocusWidget(ui.removePathButton), ui.recursivePathsCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.recursivePathsCheckBox), ui.autoRefreshCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.autoRefreshCheckBox), ui.defaultIsoLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.defaultIsoLineEdit), ui.defaultIsoBrowseButton);
  EXPECT_EQ(NextTabFocusWidget(ui.defaultIsoBrowseButton), ui.nandRootLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.nandRootLineEdit), ui.nandRootBrowseButton);
  EXPECT_EQ(NextTabFocusWidget(ui.resourcePackPathBrowseButton), ui.wfsPathLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.wfsPathLineEdit), ui.wfsPathBrowseButton);
}
