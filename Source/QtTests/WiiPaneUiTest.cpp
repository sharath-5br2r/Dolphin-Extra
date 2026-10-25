// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_WiiPane.h"

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

TEST(WiiPaneUiTest, FormOwnsTheWiiSettingsStructure)
{
  QWidget pane;
  Ui::WiiPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 5);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.miscGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.sdCardGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.usbPassthroughGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(3)->widget(), ui.wiiRemoteGroup);
  EXPECT_NE(ui.rootLayout->itemAt(4)->spacerItem(), nullptr);

  EXPECT_EQ(ui.miscLayout->rowCount(), 5);
  EXPECT_EQ(ui.miscLayout->columnCount(), 2);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(0, 0)->widget(), ui.pal60ModeCheckBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(0, 1)->widget(), ui.connectKeyboardCheckBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(1, 0)->widget(), ui.screenSaverCheckBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(1, 1)->widget(), ui.wiiLinkCheckBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(2, 0)->widget(), ui.aspectRatioLabel);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(2, 1)->widget(), ui.aspectRatioComboBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(3, 0)->widget(), ui.systemLanguageLabel);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(3, 1)->widget(), ui.systemLanguageComboBox);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(4, 0)->widget(), ui.soundModeLabel);
  EXPECT_EQ(ui.miscLayout->itemAtPosition(4, 1)->widget(), ui.soundModeComboBox);

  EXPECT_EQ(ui.aspectRatioLabel->buddy(), ui.aspectRatioComboBox);
  EXPECT_EQ(ui.systemLanguageLabel->buddy(), ui.systemLanguageComboBox);
  EXPECT_EQ(ui.soundModeLabel->buddy(), ui.soundModeComboBox);
  EXPECT_EQ(ui.aspectRatioComboBox->itemText(0), QStringLiteral("4:3"));
  EXPECT_EQ(ui.aspectRatioComboBox->itemText(1), QStringLiteral("16:9"));

  const QStringList languages{
      QStringLiteral("Japanese"),
      QStringLiteral("English"),
      QStringLiteral("German"),
      QStringLiteral("French"),
      QStringLiteral("Spanish"),
      QStringLiteral("Italian"),
      QStringLiteral("Dutch"),
      QStringLiteral("Simplified Chinese"),
      QStringLiteral("Traditional Chinese"),
      QStringLiteral("Korean"),
  };
  ASSERT_EQ(ui.systemLanguageComboBox->count(), languages.size());
  for (int i = 0; i < languages.size(); ++i)
    EXPECT_EQ(ui.systemLanguageComboBox->itemText(i), languages[i]);

  const QStringList sound_modes{QStringLiteral("Mono"), QStringLiteral("Stereo"),
                                QStringLiteral("Surround")};
  ASSERT_EQ(ui.soundModeComboBox->count(), sound_modes.size());
  for (int i = 0; i < sound_modes.size(); ++i)
    EXPECT_EQ(ui.soundModeComboBox->itemText(i), sound_modes[i]);

  EXPECT_EQ(ui.sdCardLayout->rowCount(), 6);
  EXPECT_EQ(ui.sdCardLayout->columnCount(), 3);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(0, 0)->widget(), ui.insertSdCardCheckBox);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(0, 1)->widget(), ui.allowSdWritesCheckBox);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(1, 0)->widget(), ui.sdCardPathLabel);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(1, 1)->widget(), ui.sdCardPathLineEdit);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(1, 2)->widget(), ui.sdCardPathBrowseButton);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(2, 0)->widget(), ui.syncSdFolderCheckBox);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(3, 0)->widget(), ui.sdSyncFolderLabel);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(3, 1)->widget(), ui.sdSyncFolderLineEdit);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(3, 2)->widget(), ui.sdSyncFolderBrowseButton);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(4, 0)->widget(), ui.sdCardSizeLabel);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(4, 1)->widget(), ui.sdCardSizeComboBox);
  EXPECT_EQ(ui.sdCardLayout->itemAtPosition(5, 0)->layout(), ui.sdCardActionLayout);

  EXPECT_EQ(ui.sdCardPathLabel->buddy(), ui.sdCardPathLineEdit);
  EXPECT_EQ(ui.sdSyncFolderLabel->buddy(), ui.sdSyncFolderLineEdit);
  EXPECT_EQ(ui.sdCardSizeLabel->buddy(), ui.sdCardSizeComboBox);
  EXPECT_FALSE(ui.sdCardPathBrowseButton->autoDefault());
  EXPECT_FALSE(ui.sdSyncFolderBrowseButton->autoDefault());
  EXPECT_FALSE(ui.sdPackButton->autoDefault());
  EXPECT_FALSE(ui.sdUnpackButton->autoDefault());

  const QStringList sd_sizes{
      QStringLiteral("Auto"),          QStringLiteral("64 MiB"),
      QStringLiteral("128 MiB"),       QStringLiteral("256 MiB"),
      QStringLiteral("512 MiB"),       QStringLiteral("1 GiB"),
      QStringLiteral("2 GiB"),         QStringLiteral("4 GiB (SDHC)"),
      QStringLiteral("8 GiB (SDHC)"),  QStringLiteral("16 GiB (SDHC)"),
      QStringLiteral("32 GiB (SDHC)"),
  };
  ASSERT_EQ(ui.sdCardSizeComboBox->count(), sd_sizes.size());
  for (int i = 0; i < sd_sizes.size(); ++i)
    EXPECT_EQ(ui.sdCardSizeComboBox->itemText(i), sd_sizes[i]);

  ASSERT_EQ(ui.usbPassthroughLayout->count(), 2);
  EXPECT_EQ(ui.usbPassthroughLayout->itemAt(0)->widget(), ui.usbPassthroughList);
  EXPECT_EQ(ui.usbPassthroughLayout->itemAt(1)->layout(), ui.usbPassthroughButtonLayout);
  EXPECT_EQ(ui.usbPassthroughList->sizeHint(), ui.usbPassthroughList->minimumSizeHint());
  EXPECT_FALSE(ui.usbPassthroughAddButton->autoDefault());
  EXPECT_FALSE(ui.usbPassthroughRemoveButton->autoDefault());
  EXPECT_FALSE(ui.usbPassthroughRemoveButton->isEnabled());

  EXPECT_EQ(ui.wiiRemoteLayout->rowCount(), 4);
  EXPECT_EQ(ui.wiiRemoteLayout->columnCount(), 2);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(0, 0)->widget(), ui.sensorBarPositionLabel);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(0, 1)->widget(), ui.sensorBarPositionComboBox);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(1, 0)->widget(), ui.irSensitivityLabel);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(1, 1)->widget(), ui.irSensitivitySlider);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(2, 0)->widget(), ui.speakerVolumeLabel);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(2, 1)->widget(), ui.speakerVolumeSlider);
  EXPECT_EQ(ui.wiiRemoteLayout->itemAtPosition(3, 0)->widget(), ui.wiiRemoteRumbleCheckBox);
  EXPECT_EQ(ui.sensorBarPositionLabel->buddy(), ui.sensorBarPositionComboBox);
  EXPECT_EQ(ui.irSensitivityLabel->buddy(), ui.irSensitivitySlider);
  EXPECT_EQ(ui.speakerVolumeLabel->buddy(), ui.speakerVolumeSlider);
  EXPECT_EQ(ui.sensorBarPositionComboBox->itemText(0), QStringLiteral("Top"));
  EXPECT_EQ(ui.sensorBarPositionComboBox->itemText(1), QStringLiteral("Bottom"));
  EXPECT_EQ(ui.irSensitivitySlider->minimum(), 1);
  EXPECT_EQ(ui.irSensitivitySlider->maximum(), 5);
  EXPECT_EQ(ui.speakerVolumeSlider->minimum(), 0);
  EXPECT_EQ(ui.speakerVolumeSlider->maximum(), 127);

  pane.resize(520, 930);
  ui.rootLayout->setGeometry(pane.rect());
  ui.miscLayout->setGeometry(ui.miscGroup->contentsRect());
  ui.sdCardLayout->setGeometry(ui.sdCardGroup->contentsRect());
  ui.wiiRemoteLayout->setGeometry(ui.wiiRemoteGroup->contentsRect());
  EXPECT_LT(ui.aspectRatioLabel->geometry().right(), ui.aspectRatioComboBox->geometry().left());
  EXPECT_LE(ui.aspectRatioComboBox->geometry().right(), ui.miscGroup->contentsRect().right());
  EXPECT_LT(ui.sdCardPathLabel->geometry().right(), ui.sdCardPathLineEdit->geometry().left());
  EXPECT_LT(ui.sdCardPathLineEdit->geometry().right(),
            ui.sdCardPathBrowseButton->geometry().left());
  EXPECT_LE(ui.sdCardPathBrowseButton->geometry().right(), ui.sdCardGroup->contentsRect().right());
  EXPECT_LT(ui.sensorBarPositionLabel->geometry().right(),
            ui.sensorBarPositionComboBox->geometry().left());
  EXPECT_LE(ui.sensorBarPositionComboBox->geometry().right(),
            ui.wiiRemoteGroup->contentsRect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.pal60ModeCheckBox), ui.connectKeyboardCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.connectKeyboardCheckBox), ui.screenSaverCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.screenSaverCheckBox), ui.wiiLinkCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.wiiLinkCheckBox), ui.aspectRatioComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.soundModeComboBox), ui.insertSdCardCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.sdCardPathLineEdit), ui.sdCardPathBrowseButton);
  EXPECT_EQ(NextTabFocusWidget(ui.sdCardPathBrowseButton), ui.syncSdFolderCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.sdSyncFolderBrowseButton), ui.sdCardSizeComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.sdUnpackButton), ui.usbPassthroughList);
  EXPECT_EQ(NextTabFocusWidget(ui.usbPassthroughList), ui.usbPassthroughAddButton);
  EXPECT_EQ(NextTabFocusWidget(ui.usbPassthroughRemoveButton), ui.sensorBarPositionComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.sensorBarPositionComboBox), ui.irSensitivitySlider);
  EXPECT_EQ(NextTabFocusWidget(ui.irSensitivitySlider), ui.speakerVolumeSlider);
  EXPECT_EQ(NextTabFocusWidget(ui.speakerVolumeSlider), ui.wiiRemoteRumbleCheckBox);
}
