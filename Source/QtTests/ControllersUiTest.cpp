// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_CommonControllersWidget.h"
#include "ui_ControllersPane.h"
#include "ui_GamecubeControllersWidget.h"
#include "ui_WiimoteControllersWidget.h"

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

TEST(ControllersUiTest, FormOwnsTheControllerSettingsSections)
{
  QWidget pane;
  Ui::ControllersPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 4);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->layout(), ui.gamecubeLayout);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->layout(), ui.wiimoteLayout);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->layout(), ui.commonLayout);
  EXPECT_NE(ui.rootLayout->itemAt(3)->spacerItem(), nullptr);
}

TEST(ControllersUiTest, FormOwnsTheGameCubeControllerRows)
{
  QWidget pane;
  Ui::GamecubeControllersWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.gamecubeGroup);
  EXPECT_EQ(ui.gamecubeLayout->rowCount(), 4);
  EXPECT_EQ(ui.gamecubeLayout->columnCount(), 3);
  EXPECT_EQ(ui.gamecubeLayout->columnStretch(1), 1);

  EXPECT_EQ(ui.port1Label->buddy(), ui.port1ComboBox);
  EXPECT_EQ(ui.port2Label->buddy(), ui.port2ComboBox);
  EXPECT_EQ(ui.port3Label->buddy(), ui.port3ComboBox);
  EXPECT_EQ(ui.port4Label->buddy(), ui.port4ComboBox);
  for (QComboBox* const combo :
       {ui.port1ComboBox, ui.port2ComboBox, ui.port3ComboBox, ui.port4ComboBox})
  {
    EXPECT_EQ(combo->sizeAdjustPolicy(),
              QComboBox::SizeAdjustPolicy::AdjustToMinimumContentsLengthWithIcon);
    EXPECT_EQ(combo->minimumContentsLength(), 12);
  }
  EXPECT_FALSE(ui.port1ConfigureButton->autoDefault());
  EXPECT_FALSE(ui.port2ConfigureButton->autoDefault());
  EXPECT_FALSE(ui.port3ConfigureButton->autoDefault());
  EXPECT_FALSE(ui.port4ConfigureButton->autoDefault());

  EXPECT_EQ(NextTabFocusWidget(ui.port1ComboBox), ui.port1ConfigureButton);
  EXPECT_EQ(NextTabFocusWidget(ui.port1ConfigureButton), ui.port2ComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.port4ComboBox), ui.port4ConfigureButton);

  pane.resize(520, 280);
  ui.rootLayout->setGeometry(pane.rect());
  ui.gamecubeLayout->setGeometry(ui.gamecubeGroup->contentsRect());
  EXPECT_LT(ui.port1Label->geometry().right(), ui.port1ComboBox->geometry().left());
  EXPECT_LT(ui.port1ComboBox->geometry().right(), ui.port1ConfigureButton->geometry().left());
  EXPECT_LE(ui.port1ConfigureButton->geometry().right(), ui.gamecubeGroup->contentsRect().right());
}

TEST(ControllersUiTest, FormOwnsTheWiiRemoteControllerRows)
{
  QWidget pane;
  Ui::WiimoteControllersWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.wiimoteGroup);
  EXPECT_EQ(ui.wiimoteLayout->columnStretch(2), 1);
  EXPECT_EQ(ui.wiimoteLayout->itemAtPosition(0, 0)->widget(), ui.passthroughRadioButton);
  EXPECT_EQ(ui.wiimoteLayout->itemAtPosition(4, 0)->widget(), ui.emulatedRadioButton);
  EXPECT_EQ(ui.wiimoteLayout->itemAtPosition(12, 0)->layout(), ui.continuousScanningLayout);

  EXPECT_EQ(ui.bluetoothAdaptersLabel->buddy(), ui.bluetoothAdaptersComboBox);
  EXPECT_EQ(ui.wiimote1Label->buddy(), ui.wiimote1ComboBox);
  EXPECT_EQ(ui.wiimote4Label->buddy(), ui.wiimote4ComboBox);
  EXPECT_EQ(ui.wiimote1ComboBox->count(), 3);
  EXPECT_EQ(ui.wiimote1ComboBox->itemText(0), QStringLiteral("None"));
  EXPECT_EQ(ui.wiimote1ComboBox->itemText(1), QStringLiteral("Emulated Wii Remote"));
  EXPECT_EQ(ui.wiimote1ComboBox->itemText(2), QStringLiteral("Real Wii Remote"));
  for (QComboBox* const combo : {ui.bluetoothAdaptersComboBox, ui.wiimote1ComboBox,
                                 ui.wiimote2ComboBox, ui.wiimote3ComboBox, ui.wiimote4ComboBox})
  {
    EXPECT_EQ(combo->sizeAdjustPolicy(),
              QComboBox::SizeAdjustPolicy::AdjustToMinimumContentsLengthWithIcon);
    EXPECT_EQ(combo->minimumContentsLength(), 12);
  }

  EXPECT_FALSE(ui.bluetoothAdaptersRefreshButton->autoDefault());
  EXPECT_FALSE(ui.passthroughSyncButton->autoDefault());
  EXPECT_FALSE(ui.passthroughResetButton->autoDefault());
  EXPECT_FALSE(ui.wiimote1ConfigureButton->autoDefault());
  EXPECT_EQ(ui.wiimoteRefreshButton->popupMode(),
            QToolButton::ToolButtonPopupMode::MenuButtonPopup);
  EXPECT_TRUE(ui.refreshIndicatorLabel->isHidden());

  pane.resize(520, 560);
  ui.rootLayout->setGeometry(pane.rect());
  ui.wiimoteLayout->setGeometry(ui.wiimoteGroup->contentsRect());
  EXPECT_LT(ui.wiimote1Label->geometry().right(), ui.wiimote1ComboBox->geometry().left());
  EXPECT_LT(ui.wiimote1ComboBox->geometry().right(), ui.wiimote1ConfigureButton->geometry().left());
  EXPECT_LE(ui.wiimote1ConfigureButton->geometry().right(),
            ui.wiimoteGroup->contentsRect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.passthroughRadioButton), ui.bluetoothAdaptersComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.bluetoothAdaptersComboBox), ui.bluetoothAdaptersRefreshButton);
  EXPECT_EQ(NextTabFocusWidget(ui.continuousScanningCheckBox), ui.wiimoteRefreshButton);
}

TEST(ControllersUiTest, FormOwnsTheCommonControllerActions)
{
  QWidget pane;
  Ui::CommonControllersWidget ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 1);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.commonGroup);
  ASSERT_EQ(ui.commonLayout->count(), 3);
  EXPECT_EQ(ui.commonLayout->itemAt(0)->widget(), ui.backgroundInputCheckBox);
  EXPECT_EQ(ui.commonLayout->itemAt(1)->widget(), ui.alternateInputSourcesButton);
  EXPECT_EQ(ui.commonLayout->itemAt(2)->widget(), ui.sdlControllerSettingsButton);
  EXPECT_FALSE(ui.alternateInputSourcesButton->autoDefault());
  EXPECT_FALSE(ui.sdlControllerSettingsButton->autoDefault());
}
