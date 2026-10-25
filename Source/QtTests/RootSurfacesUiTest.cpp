// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMargins>
#include <QTableWidget>
#include <QWidget>

#include <gtest/gtest.h>

#include "DolphinQt/QtUtils/PartiallyClosableTabWidget.h"

#include "ui_CheatSearchFactoryWidget.h"
#include "ui_CheatSearchWidget.h"
#include "ui_CheatsManager.h"
#include "ui_ConvertDialog.h"
#include "ui_EmulationStatusWidget.h"
#include "ui_GCMemcardManager.h"
#include "ui_ResourcePackManager.h"
#include "ui_RiivolutionBootWidget.h"
#include "ui_UpdateAvailableDialog.h"

TEST(RootSurfacesUiTest, CheatSearchFactoryFormOwnsSearchSetup)
{
  QWidget widget;
  Ui::CheatSearchFactoryWidget ui;
  ui.setupUi(&widget);

  ASSERT_EQ(ui.mainLayout->count(), 4);
  EXPECT_TRUE(ui.standardAddressSpaceRadioButton->isChecked());
  EXPECT_FALSE(ui.customAddressSpaceRadioButton->isChecked());
  EXPECT_TRUE(ui.customVirtualAddressSpaceRadioButton->isChecked());
  EXPECT_EQ(ui.customAddressSpaceLayout->contentsMargins(), QMargins(6, 6, 6, 6));
  EXPECT_EQ(ui.customAddressStartLineEdit->text(), QStringLiteral("0x80000000"));
  EXPECT_EQ(ui.customAddressEndLineEdit->text(), QStringLiteral("0x81800000"));
  EXPECT_TRUE(ui.alignedCheckBox->isChecked());
  EXPECT_FALSE(ui.newSearchButton->autoDefault());
}

TEST(RootSurfacesUiTest, CheatSearchSessionFormOwnsPermanentResultsLayout)
{
  QWidget widget;
  Ui::CheatSearchWidget ui;
  ui.setupUi(&widget);

  ASSERT_EQ(ui.mainLayout->count(), 8);
  EXPECT_TRUE(ui.sessionInfoLabel->wordWrap());
  EXPECT_EQ(ui.valueLayout->count(), 4);
  EXPECT_EQ(ui.buttonLayout->count(), 3);
  EXPECT_EQ(ui.checkboxesLayout->stretch(0), 1);
  EXPECT_EQ(ui.checkboxesLayout->stretch(1), 2);
  EXPECT_EQ(ui.infoLabel1->text(), QStringLiteral("Waiting for first scan..."));
  EXPECT_EQ(ui.addressTableWidget->contextMenuPolicy(), Qt::CustomContextMenu);
  EXPECT_EQ(ui.addressTableWidget->selectionBehavior(), QAbstractItemView::SelectRows);
}

TEST(RootSurfacesUiTest, CheatsManagerFormOwnsTabsAndCloseButton)
{
  QDialog dialog;
  Ui::CheatsManager ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Cheats Manager"));
  EXPECT_NE(dynamic_cast<PartiallyClosableTabWidget*>(ui.tabWidget), nullptr);
  EXPECT_EQ(ui.mainLayout->count(), 2);
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Close);
}

TEST(RootSurfacesUiTest, ConvertFormOwnsOptionsAndInformation)
{
  QDialog dialog;
  Ui::ConvertDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Convert"));
  EXPECT_EQ(ui.formLayout->fieldGrowthPolicy(), QFormLayout::AllNonFixedFieldsGrow);
  EXPECT_EQ(ui.formLayout->rowCount(), 5);
  EXPECT_EQ(ui.formatLabel->buddy(), ui.formatComboBox);
  EXPECT_EQ(ui.blockSizeLabel->buddy(), ui.blockSizeComboBox);
  EXPECT_EQ(ui.compressionLabel->buddy(), ui.compressionComboBox);
  EXPECT_EQ(ui.compressionLevelLabel->buddy(), ui.compressionLevelComboBox);
  EXPECT_EQ(ui.scrubLabel->buddy(), ui.scrubCheckBox);
  EXPECT_TRUE(ui.infoLabel->wordWrap());
}

TEST(RootSurfacesUiTest, EmulationStatusFormOwnsSixUniformMetrics)
{
  QWidget widget;
  Ui::EmulationStatusWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->count(), 6);
  EXPECT_EQ(ui.mainLayout->spacing(), 0);
  EXPECT_EQ(ui.mainLayout->contentsMargins(), QMargins());
  for (const auto* label : {ui.statusRenderer, ui.statusResolution, ui.statusFps, ui.statusVps,
                            ui.statusSpeed, ui.statusVolume})
  {
    EXPECT_EQ(label->alignment(), Qt::AlignCenter);
    EXPECT_EQ(label->minimumHeight(), 20);
    EXPECT_EQ(label->maximumHeight(), 20);
    EXPECT_EQ(label->sizePolicy().horizontalPolicy(), QSizePolicy::Minimum);
    EXPECT_EQ(label->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
  }
}

TEST(RootSurfacesUiTest, MemoryCardFormOwnsBothSlotShellsAndActions)
{
  QDialog dialog;
  Ui::GCMemcardManager ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("GameCube Memory Card Manager"));
  EXPECT_EQ(dialog.size(), QSize(650, 500));
  EXPECT_EQ(ui.slotALayout->itemAtPosition(1, 0)->widget(), ui.slotATableWidget);
  EXPECT_EQ(ui.slotBLayout->itemAtPosition(1, 0)->widget(), ui.slotBTableWidget);
  for (const auto* table : {ui.slotATableWidget, ui.slotBTableWidget})
  {
    EXPECT_EQ(table->columnCount(), 5);
    EXPECT_FALSE(table->tabKeyNavigation());
    EXPECT_EQ(table->selectionMode(), QAbstractItemView::ExtendedSelection);
    EXPECT_EQ(table->selectionBehavior(), QAbstractItemView::SelectRows);
    EXPECT_FALSE(table->showGrid());
  }
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Close);
}

TEST(RootSurfacesUiTest, ResourcePackFormOwnsTableAndActionColumn)
{
  QDialog dialog;
  Ui::ResourcePackManager ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Resource Pack Manager"));
  EXPECT_EQ(dialog.size(), QSize(900, 600));
  EXPECT_EQ(ui.mainLayout->itemAtPosition(0, 0)->widget(), ui.tableWidget);
  EXPECT_EQ(ui.mainLayout->itemAtPosition(0, 1)->widget(), ui.openDirectoryButton);
  EXPECT_EQ(ui.mainLayout->itemAtPosition(5, 1)->widget(), ui.priorityDownButton);
  EXPECT_FALSE(ui.tableWidget->tabKeyNavigation());
  EXPECT_EQ(ui.tableWidget->selectionMode(), QAbstractItemView::SingleSelection);
  EXPECT_EQ(ui.tableWidget->selectionBehavior(), QAbstractItemView::SelectRows);
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Ok);
}

TEST(RootSurfacesUiTest, RiivolutionFormOwnsPermanentShellAndGeneratedContentTarget)
{
  QDialog dialog;
  Ui::RiivolutionBootWidget ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Start with Riivolution Patches"));
  EXPECT_EQ(dialog.size(), QSize(400, 600));
  EXPECT_TRUE(ui.scrollArea->widgetResizable());
  EXPECT_EQ(ui.stretchHelperLayout->itemAt(0)->layout(), ui.patchSectionLayout);
  EXPECT_EQ(ui.buttonLayout->itemAt(1)->widget(), ui.openXmlButton);
  EXPECT_EQ(ui.buttonLayout->itemAt(2)->widget(), ui.savePresetButton);
  EXPECT_EQ(ui.buttonLayout->itemAt(3)->widget(), ui.bootGameButton);
  EXPECT_TRUE(ui.bootGameButton->isDefault());
}

TEST(RootSurfacesUiTest, UpdateFormOwnsReleaseNotesAndDeferredUpdateChoice)
{
  QDialog dialog;
  Ui::UpdateAvailableDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Update available"));
  EXPECT_EQ(ui.mainLayout->count(), 4);
  EXPECT_EQ(ui.updateLabel->textFormat(), Qt::RichText);
  EXPECT_TRUE(ui.updateLabel->text().contains(QStringLiteral("%1")));
  EXPECT_TRUE(ui.updateLabel->text().contains(QStringLiteral("%2")));
  EXPECT_TRUE(ui.changelogBrowser->openExternalLinks());
  EXPECT_EQ(ui.changelogBrowser->minimumWidth(), 400);
  EXPECT_FALSE(ui.updateLaterCheckBox->isChecked());
}
