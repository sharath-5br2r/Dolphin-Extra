// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_GameCubePane.h"

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

TEST(GameCubePaneUiTest, FormOwnsTheGameCubeSettingsStructure)
{
  QWidget pane;
  Ui::GameCubePane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 4);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.iplGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.deviceGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.gbaGroup);
  EXPECT_NE(ui.rootLayout->itemAt(3)->spacerItem(), nullptr);

  ASSERT_EQ(ui.iplLayout->count(), 2);
  EXPECT_EQ(ui.iplLayout->itemAt(0)->widget(), ui.skipMainMenuCheckBox);
  EXPECT_EQ(ui.iplLayout->itemAt(1)->layout(), ui.languageLayout);
  EXPECT_EQ(ui.systemLanguageLabel->buddy(), ui.systemLanguageComboBox);

  const QStringList languages{QStringLiteral("English"), QStringLiteral("German"),
                              QStringLiteral("French"),  QStringLiteral("Spanish"),
                              QStringLiteral("Italian"), QStringLiteral("Dutch")};
  ASSERT_EQ(ui.systemLanguageComboBox->count(), languages.size());
  for (int i = 0; i < languages.size(); ++i)
    EXPECT_EQ(ui.systemLanguageComboBox->itemText(i), languages[i]);

  EXPECT_EQ(ui.deviceLayout->rowCount(), 11);
  EXPECT_EQ(ui.deviceLayout->columnCount(), 3);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(0, 0)->widget(), ui.slotALabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(0, 1)->widget(), ui.slotAComboBox);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(0, 2)->widget(), ui.slotAConfigButton);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(5, 0)->widget(), ui.slotBLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(5, 1)->widget(), ui.slotBComboBox);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(5, 2)->widget(), ui.slotBConfigButton);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(10, 0)->widget(), ui.sp1Label);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(10, 1)->widget(), ui.sp1ComboBox);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(10, 2)->widget(), ui.sp1ConfigButton);

  const std::array slot_labels{ui.slotALabel, ui.slotBLabel, ui.sp1Label};
  const std::array slot_combos{ui.slotAComboBox, ui.slotBComboBox, ui.sp1ComboBox};
  const std::array slot_buttons{ui.slotAConfigButton, ui.slotBConfigButton, ui.sp1ConfigButton};
  for (size_t i = 0; i < slot_labels.size(); ++i)
  {
    EXPECT_EQ(slot_labels[i]->buddy(), slot_combos[i]);
    EXPECT_FALSE(slot_buttons[i]->isEnabled());
    EXPECT_FALSE(slot_buttons[i]->autoDefault());
  }

  const std::array device_path_labels{ui.slotAMemcardPathLabel, ui.slotAAgpPathLabel,
                                      ui.slotAGciPathLabel,     ui.slotBMemcardPathLabel,
                                      ui.slotBAgpPathLabel,     ui.slotBGciPathLabel};
  const std::array device_path_edits{ui.slotAMemcardPathLineEdit, ui.slotAAgpPathLineEdit,
                                     ui.slotAGciPathLineEdit,     ui.slotBMemcardPathLineEdit,
                                     ui.slotBAgpPathLineEdit,     ui.slotBGciPathLineEdit};
  for (size_t i = 0; i < device_path_labels.size(); ++i)
  {
    EXPECT_EQ(device_path_labels[i]->buddy(), device_path_edits[i]);
    EXPECT_TRUE(device_path_labels[i]->isHidden());
    EXPECT_TRUE(device_path_edits[i]->isHidden());
  }
  EXPECT_TRUE(ui.slotAGciOverrideLabel->isHidden());
  EXPECT_TRUE(ui.slotBGciOverrideLabel->isHidden());
  EXPECT_TRUE(ui.slotAGciOverrideLabel->wordWrap());
  EXPECT_TRUE(ui.slotBGciOverrideLabel->wordWrap());

  EXPECT_EQ(ui.deviceLayout->itemAtPosition(1, 0)->widget(), ui.slotAMemcardPathLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(1, 1)->widget(), ui.slotAMemcardPathLineEdit);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(2, 0)->widget(), ui.slotAAgpPathLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(2, 1)->widget(), ui.slotAAgpPathLineEdit);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(3, 0)->widget(), ui.slotAGciOverrideLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(4, 0)->widget(), ui.slotAGciPathLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(4, 1)->widget(), ui.slotAGciPathLineEdit);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(6, 0)->widget(), ui.slotBMemcardPathLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(6, 1)->widget(), ui.slotBMemcardPathLineEdit);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(7, 0)->widget(), ui.slotBAgpPathLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(7, 1)->widget(), ui.slotBAgpPathLineEdit);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(8, 0)->widget(), ui.slotBGciOverrideLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(9, 0)->widget(), ui.slotBGciPathLabel);
  EXPECT_EQ(ui.deviceLayout->itemAtPosition(9, 1)->widget(), ui.slotBGciPathLineEdit);

  EXPECT_EQ(ui.gbaLayout->rowCount(), 8);
  EXPECT_EQ(ui.gbaLayout->columnCount(), 3);
  const std::array gba_labels{ui.gbaBiosLabel, ui.gbaRom1Label, ui.gbaRom2Label, ui.gbaRom3Label,
                              ui.gbaRom4Label, ui.gbaRom5Label, ui.gbaSavesLabel};
  const std::array gba_edits{ui.gbaBiosLineEdit, ui.gbaRom1LineEdit, ui.gbaRom2LineEdit,
                             ui.gbaRom3LineEdit, ui.gbaRom4LineEdit, ui.gbaRom5LineEdit,
                             ui.gbaSavesLineEdit};
  const std::array gba_buttons{ui.gbaBiosBrowseButton, ui.gbaRom1BrowseButton,
                               ui.gbaRom2BrowseButton, ui.gbaRom3BrowseButton,
                               ui.gbaRom4BrowseButton, ui.gbaRom5BrowseButton,
                               ui.gbaSavesBrowseButton};
  for (size_t i = 0; i < gba_labels.size(); ++i)
  {
    const int row = i == gba_labels.size() - 1 ? 7 : static_cast<int>(i);
    EXPECT_EQ(ui.gbaLayout->itemAtPosition(row, 0)->widget(), gba_labels[i]);
    EXPECT_EQ(ui.gbaLayout->itemAtPosition(row, 1)->widget(), gba_edits[i]);
    EXPECT_EQ(ui.gbaLayout->itemAtPosition(row, 2)->widget(), gba_buttons[i]);
    EXPECT_EQ(gba_labels[i]->buddy(), gba_edits[i]);
    EXPECT_FALSE(gba_buttons[i]->autoDefault());
  }
  EXPECT_EQ(ui.gbaLayout->itemAtPosition(6, 0)->widget(), ui.gbaSaveInRomPathCheckBox);

  pane.resize(520, 980);
  ui.rootLayout->setGeometry(pane.rect());
  ui.deviceLayout->setGeometry(ui.deviceGroup->contentsRect());
  ui.gbaLayout->setGeometry(ui.gbaGroup->contentsRect());
  EXPECT_LT(ui.slotALabel->geometry().right(), ui.slotAComboBox->geometry().left());
  EXPECT_LT(ui.slotAComboBox->geometry().right(), ui.slotAConfigButton->geometry().left());
  EXPECT_LE(ui.slotAConfigButton->geometry().right(), ui.deviceGroup->contentsRect().right());
  EXPECT_LT(ui.gbaBiosLabel->geometry().right(), ui.gbaBiosLineEdit->geometry().left());
  EXPECT_LT(ui.gbaBiosLineEdit->geometry().right(), ui.gbaBiosBrowseButton->geometry().left());
  EXPECT_LE(ui.gbaBiosBrowseButton->geometry().right(), ui.gbaGroup->contentsRect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.skipMainMenuCheckBox), ui.systemLanguageComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.systemLanguageComboBox), ui.slotAComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.slotAComboBox), ui.slotAConfigButton);
  EXPECT_EQ(NextTabFocusWidget(ui.slotAConfigButton), ui.slotAMemcardPathLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.slotAMemcardPathLineEdit), ui.slotAAgpPathLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.slotAAgpPathLineEdit), ui.slotAGciPathLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.slotAGciPathLineEdit), ui.slotBComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.slotBConfigButton), ui.slotBMemcardPathLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.slotBGciPathLineEdit), ui.sp1ComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.sp1ConfigButton), ui.gbaBiosLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.gbaBiosLineEdit), ui.gbaBiosBrowseButton);
  EXPECT_EQ(NextTabFocusWidget(ui.gbaRom5BrowseButton), ui.gbaSaveInRomPathCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.gbaSaveInRomPathCheckBox), ui.gbaSavesLineEdit);
  EXPECT_EQ(NextTabFocusWidget(ui.gbaSavesLineEdit), ui.gbaSavesBrowseButton);
}
