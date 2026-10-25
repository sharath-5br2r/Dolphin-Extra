// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMargins>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_ControllerInterfaceWindow.h"
#include "ui_DualShockUDPClientEditServerDialog.h"
#include "ui_DualShockUDPClientWidget.h"
#include "ui_FreeLookRotation.h"
#include "ui_GCKeyboardEmu.h"
#include "ui_GCPadWiiUConfigDialog.h"
#include "ui_IOWindow.h"
#include "ui_MappingGridPage.h"
#include "ui_MappingHorizontalPage.h"
#include "ui_MappingWindow.h"
#include "ui_WiimoteEmuExtension.h"
#include "ui_WiimoteEmuExtensionMotionInput.h"
#include "ui_WiimoteEmuExtensionMotionSimulation.h"
#include "ui_WiimoteEmuMotionControlIMU.h"

TEST(MappingUiTest, SharedMappingPagesExposeEmptyControllerGroupLayouts)
{
  QWidget grid_widget;
  Ui::MappingGridPage grid_ui;
  grid_ui.setupUi(&grid_widget);
  EXPECT_EQ(grid_ui.groupLayout->count(), 0);

  QWidget horizontal_widget;
  Ui::MappingHorizontalPage horizontal_ui;
  horizontal_ui.setupUi(&horizontal_widget);
  EXPECT_EQ(horizontal_ui.groupLayout->count(), 0);
}

TEST(MappingUiTest, MappingWindowFormOwnsDeviceResetProfileAndTabShell)
{
  QDialog dialog;
  Ui::MappingWindow ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.configLayout->count(), 3);
  EXPECT_EQ(ui.devicesBox->title(), QStringLiteral("Device"));
  EXPECT_EQ(ui.resetBox->title(), QStringLiteral("Reset"));
  EXPECT_EQ(ui.profilesBox->title(), QStringLiteral("Profile"));
  EXPECT_EQ(ui.deviceOptionsButton->popupMode(), QToolButton::MenuButtonPopup);
  EXPECT_EQ(ui.profileOtherActionsButton->popupMode(), QToolButton::InstantPopup);
  EXPECT_EQ(ui.profileOtherActionsButton->arrowType(), Qt::DownArrow);
  EXPECT_TRUE(ui.profilesComboBox->isEditable());
  EXPECT_EQ(ui.profilesComboBox->minimumWidth(), 100);
  EXPECT_FALSE(ui.resetDefaultButton->autoDefault());
  EXPECT_FALSE(ui.resetClearButton->autoDefault());
  EXPECT_FALSE(ui.profilesLoadButton->autoDefault());
  EXPECT_FALSE(ui.profilesSaveButton->autoDefault());
  EXPECT_TRUE(ui.tabWidget->tabBarAutoHide());
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Close);
}

TEST(MappingUiTest, ExpressionFormOwnsEditorAndRuntimeInsertionTargets)
{
  QDialog dialog;
  Ui::IOWindow ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.mainLayout->stretch(1), 2);
  EXPECT_EQ(ui.mainLayout->stretch(2), 1);
  EXPECT_EQ(ui.optionsLayout->stretch(0), 8);
  EXPECT_EQ(ui.optionsLayout->stretch(1), 1);
  EXPECT_EQ(ui.variablesComboLayout->count(), 0);
  EXPECT_EQ(ui.operatorsComboLayout->count(), 0);
  EXPECT_EQ(ui.functionsComboLayout->count(), 0);
  EXPECT_EQ(ui.parseTextLayout->contentsMargins(), QMargins());
  EXPECT_FALSE(ui.optionTableWidget->tabKeyNavigation());
  EXPECT_EQ(ui.optionTableWidget->editTriggers(), QAbstractItemView::NoEditTriggers);
  EXPECT_EQ(ui.optionTableWidget->selectionMode(), QAbstractItemView::SingleSelection);
  EXPECT_EQ(ui.optionTableWidget->selectionBehavior(), QAbstractItemView::SelectRows);
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Ok);
}

TEST(MappingUiTest, AdapterFormOwnsStatusSettingsAndOkButton)
{
  QDialog dialog;
  Ui::GCPadWiiUConfigDialog ui;
  ui.setupUi(&dialog);

  EXPECT_TRUE(ui.statusLabel->text().isEmpty());
  EXPECT_TRUE(ui.pollRateLabel->text().isEmpty());
  EXPECT_EQ(ui.rumbleCheckBox->text(), QStringLiteral("Enable Rumble"));
  EXPECT_EQ(ui.simulateBongosCheckBox->text(), QStringLiteral("Simulate DK Bongos"));
  EXPECT_EQ(ui.buttonBox->standardButtons(), QDialogButtonBox::Ok);
}

TEST(MappingUiTest, WarningPagesOwnMessagesAndGroupInsertionLayouts)
{
  QWidget keyboard;
  Ui::GCKeyboardEmu keyboard_ui;
  keyboard_ui.setupUi(&keyboard);
  EXPECT_TRUE(keyboard_ui.warningLabel->wordWrap());
  EXPECT_EQ(keyboard_ui.warningLayout->stretch(1), 1);
  EXPECT_EQ(keyboard_ui.groupLayout->count(), 0);

  QWidget free_look;
  Ui::FreeLookRotation free_look_ui;
  free_look_ui.setupUi(&free_look);
  EXPECT_TRUE(free_look_ui.noteLabel->wordWrap());
  EXPECT_EQ(free_look_ui.mainLayout->count(), 1);

  QWidget motion_input;
  Ui::WiimoteEmuMotionControlIMU motion_input_ui;
  motion_input_ui.setupUi(&motion_input);
  EXPECT_TRUE(motion_input_ui.warningLabel->wordWrap());
  EXPECT_EQ(motion_input_ui.groupsLayout->count(), 0);

  QWidget extension_input;
  Ui::WiimoteEmuExtensionMotionInput extension_input_ui;
  extension_input_ui.setupUi(&extension_input);
  EXPECT_TRUE(extension_input_ui.warningLabel->wordWrap());
  EXPECT_EQ(extension_input_ui.nunchukLayout->count(), 1);
}

TEST(MappingUiTest, ExtensionFormOwnsAllVariantContainers)
{
  QWidget widget;
  Ui::WiimoteEmuExtension ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.mainLayout->count(), 10);
  EXPECT_EQ(ui.classicBox->title(), QStringLiteral("Classic Controller"));
  EXPECT_EQ(ui.drumsBox->title(), QStringLiteral("Drum Kit"));
  EXPECT_EQ(ui.guitarBox->title(), QStringLiteral("Guitar"));
  EXPECT_EQ(ui.nunchukBox->title(), QStringLiteral("Nunchuk"));
  EXPECT_EQ(ui.turntableBox->title(), QStringLiteral("DJ Turntable"));
  EXPECT_EQ(ui.uDrawTabletBox->title(), QStringLiteral("uDraw GameTablet"));
  EXPECT_EQ(ui.drawsomeTabletBox->title(), QStringLiteral("Drawsome Tablet"));
  EXPECT_EQ(ui.taTaConBox->title(), QStringLiteral("Taiko Drum"));
  EXPECT_EQ(ui.shinkansenBox->title(), QStringLiteral("Shinkansen"));
  EXPECT_EQ(ui.noneLabel->alignment(), Qt::AlignCenter);

  QWidget simulation;
  Ui::WiimoteEmuExtensionMotionSimulation simulation_ui;
  simulation_ui.setupUi(&simulation);
  EXPECT_EQ(simulation_ui.nunchukBox->title(), QStringLiteral("Nunchuk"));
  EXPECT_EQ(simulation_ui.nunchukLayout->count(), 0);
}

TEST(MappingUiTest, AlternateInputFormsOwnConditionalAndDsuShells)
{
  QDialog dialog;
  Ui::ControllerInterfaceWindow window_ui;
  window_ui.setupUi(&dialog);
  EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Alternate Input Sources"));
  EXPECT_EQ(window_ui.nothingToConfigureLabel->alignment(), Qt::AlignCenter);
  EXPECT_EQ(window_ui.buttonBox->standardButtons(), QDialogButtonBox::Close);

  QWidget dsu_widget;
  Ui::DualShockUDPClientWidget dsu_ui;
  dsu_ui.setupUi(&dsu_widget);
  EXPECT_FALSE(dsu_ui.addServerButton->autoDefault());
  EXPECT_FALSE(dsu_ui.editServerButton->autoDefault());
  EXPECT_FALSE(dsu_ui.removeServerButton->autoDefault());
  EXPECT_TRUE(dsu_ui.descriptionLabel->wordWrap());
  EXPECT_TRUE(dsu_ui.descriptionLabel->openExternalLinks());
  EXPECT_EQ(dsu_ui.descriptionLabel->textInteractionFlags(), Qt::TextBrowserInteraction);
}

TEST(MappingUiTest, DsuServerEditorFormOwnsValidatedFieldLayout)
{
  QDialog dialog;
  Ui::DualShockUDPClientEditServerDialog ui;
  ui.setupUi(&dialog);

  EXPECT_EQ(ui.mainLayout->rowCount(), 4);
  EXPECT_EQ(ui.descriptionLabel->buddy(), ui.descriptionLineEdit);
  EXPECT_EQ(ui.serverAddressLabel->buddy(), ui.serverAddressLineEdit);
  EXPECT_EQ(ui.serverPortLabel->buddy(), ui.serverPortSpinBox);
  EXPECT_EQ(ui.descriptionLineEdit->placeholderText(),
            QStringLiteral("BetterJoy, DS4Windows, etc"));
  EXPECT_EQ(ui.serverPortSpinBox->maximum(), 65535);
  EXPECT_EQ(ui.buttonBox->standardButtons(),
            QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
}
