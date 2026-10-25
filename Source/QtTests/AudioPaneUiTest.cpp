// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_AudioPane.h"

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

TEST(AudioPaneUiTest, FormOwnsTheAudioSettingsStructure)
{
  QWidget pane;
  Ui::AudioPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 3);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.volumeGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->layout(), ui.mainColumnLayout);
  EXPECT_NE(ui.rootLayout->itemAt(2)->spacerItem(), nullptr);

  ASSERT_EQ(ui.mainColumnLayout->count(), 5);
  EXPECT_EQ(ui.mainColumnLayout->itemAt(0)->widget(), ui.dspGroup);
  EXPECT_EQ(ui.mainColumnLayout->itemAt(1)->widget(), ui.backendGroup);
  EXPECT_EQ(ui.mainColumnLayout->itemAt(2)->widget(), ui.playbackGroup);
  EXPECT_EQ(ui.mainColumnLayout->itemAt(3)->widget(), ui.wiimoteRoutingGroup);
  EXPECT_EQ(ui.mainColumnLayout->itemAt(4)->widget(), ui.gbaRoutingGroup);

  EXPECT_EQ(ui.dspLayout->itemAtPosition(0, 0)->widget(), ui.dspEngineLabel);
  EXPECT_EQ(ui.dspLayout->itemAtPosition(0, 1)->widget(), ui.dspEngineComboBox);
  EXPECT_EQ(ui.dspEngineLabel->buddy(), ui.dspEngineComboBox);

  EXPECT_EQ(ui.backendLayout->itemAt(0, QFormLayout::LabelRole)->widget(), ui.backendLabel);
  EXPECT_EQ(ui.backendLayout->itemAt(0, QFormLayout::FieldRole)->widget(), ui.backendComboBox);
  EXPECT_EQ(ui.backendLayout->itemAt(1, QFormLayout::LabelRole)->widget(), ui.wasapiDeviceLabel);
  EXPECT_EQ(ui.backendLayout->itemAt(1, QFormLayout::FieldRole)->widget(), ui.wasapiDeviceComboBox);
  EXPECT_EQ(ui.backendLayout->itemAt(2, QFormLayout::LabelRole)->widget(), ui.latencyLabel);
  EXPECT_EQ(ui.backendLayout->itemAt(2, QFormLayout::FieldRole)->widget(), ui.latencySlider);
  EXPECT_EQ(ui.backendLayout->itemAt(3, QFormLayout::SpanningRole)->widget(),
            ui.dolbyProLogicCheckBox);
  EXPECT_EQ(ui.backendLayout->itemAt(4, QFormLayout::LabelRole)->widget(), ui.dolbyQualityLabel);
  EXPECT_EQ(ui.backendLayout->itemAt(4, QFormLayout::FieldRole)->widget(), ui.dolbyQualityComboBox);
  EXPECT_EQ(ui.backendLabel->buddy(), ui.backendComboBox);
  EXPECT_EQ(ui.wasapiDeviceLabel->buddy(), ui.wasapiDeviceComboBox);
  EXPECT_EQ(ui.latencyLabel->buddy(), ui.latencySlider);
  EXPECT_EQ(ui.dolbyQualityLabel->buddy(), ui.dolbyQualityComboBox);
  EXPECT_EQ(ui.dolbyQualityComboBox->count(), 4);
  EXPECT_EQ(ui.latencySlider->minimum(), 0);
  EXPECT_EQ(ui.latencySlider->maximum(), 200);

  EXPECT_EQ(ui.playbackLayout->itemAtPosition(0, 0)->widget(), ui.audioBufferSizeLabel);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(0, 1)->widget(), ui.audioBufferSizeSlider);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(0, 2)->widget(), ui.audioBufferSizeValueLabel);
  EXPECT_EQ(ui.audioBufferSizeLabel->buddy(), ui.audioBufferSizeSlider);
  EXPECT_EQ(ui.audioBufferSizeSlider->minimum(), 16);
  EXPECT_EQ(ui.audioBufferSizeSlider->maximum(), 512);
  EXPECT_EQ(ui.audioBufferSizeSlider->singleStep(), 8);
  EXPECT_EQ(ui.audioBufferSizeSlider->pageStep(), 8);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(1, 0)->widget(), ui.audioFillGapsCheckBox);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(2, 0)->widget(), ui.audioPreservePitchCheckBox);
  EXPECT_EQ(ui.playbackLayout->itemAtPosition(3, 0)->widget(), ui.muteOnUnlimitedSpeedCheckBox);

  EXPECT_EQ(ui.wiimoteRoutingLayout->itemAtPosition(0, 0)->widget(), ui.wiimoteRoutingCheckBox);
  EXPECT_EQ(ui.wiimoteRoutingLayout->itemAtPosition(1, 0)->widget(), ui.wiimote1CheckBox);
  EXPECT_EQ(ui.wiimoteRoutingLayout->itemAtPosition(1, 1)->widget(), ui.wiimote1DeviceComboBox);
  EXPECT_EQ(ui.wiimoteRoutingLayout->itemAtPosition(4, 0)->widget(), ui.wiimote4CheckBox);
  EXPECT_EQ(ui.wiimoteRoutingLayout->itemAtPosition(4, 1)->widget(), ui.wiimote4DeviceComboBox);

  EXPECT_EQ(ui.gbaRoutingLayout->itemAtPosition(0, 0)->widget(), ui.gbaRoutingCheckBox);
  EXPECT_EQ(ui.gbaRoutingLayout->itemAtPosition(1, 0)->widget(), ui.gba1CheckBox);
  EXPECT_EQ(ui.gbaRoutingLayout->itemAtPosition(1, 1)->widget(), ui.gba1DeviceComboBox);
  EXPECT_EQ(ui.gbaRoutingLayout->itemAtPosition(4, 0)->widget(), ui.gba4CheckBox);
  EXPECT_EQ(ui.gbaRoutingLayout->itemAtPosition(4, 1)->widget(), ui.gba4DeviceComboBox);

  ASSERT_EQ(ui.volumeLayout->count(), 2);
  EXPECT_EQ(ui.volumeLayout->itemAt(0)->widget(), ui.volumeSlider);
  EXPECT_EQ(ui.volumeLayout->itemAt(1)->widget(), ui.volumeValueLabel);
  EXPECT_EQ(ui.volumeSlider->orientation(), Qt::Horizontal);
  EXPECT_EQ(ui.volumeSlider->minimum(), 0);
  EXPECT_EQ(ui.volumeSlider->maximum(), 100);
  EXPECT_EQ(ui.volumeValueLabel->alignment(), Qt::AlignCenter);

  pane.resize(560, 780);
  ui.rootLayout->setGeometry(pane.rect());
  EXPECT_LE(ui.dspGroup->geometry().right(), pane.rect().right());
  EXPECT_LE(ui.backendGroup->geometry().right(), pane.rect().right());
  EXPECT_LE(ui.playbackGroup->geometry().right(), pane.rect().right());
  EXPECT_LE(ui.wiimoteRoutingGroup->geometry().right(), pane.rect().right());
  EXPECT_LE(ui.gbaRoutingGroup->geometry().right(), pane.rect().right());
  EXPECT_LE(ui.volumeGroup->geometry().right(), pane.rect().right());

  EXPECT_EQ(NextTabFocusWidget(ui.volumeSlider), ui.dspEngineComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.dspEngineComboBox), ui.backendComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.backendComboBox), ui.wasapiDeviceComboBox);
  EXPECT_EQ(NextTabFocusWidget(ui.wasapiDeviceComboBox), ui.latencySlider);
  EXPECT_EQ(NextTabFocusWidget(ui.latencySlider), ui.dolbyProLogicCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.dolbyQualityComboBox), ui.audioBufferSizeSlider);
  EXPECT_EQ(NextTabFocusWidget(ui.muteOnUnlimitedSpeedCheckBox), ui.wiimoteRoutingCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.wiimote4DeviceComboBox), ui.gbaRoutingCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.gba4DeviceComboBox), ui.volumeSlider);
}
