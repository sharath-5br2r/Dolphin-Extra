// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "ui_AdvancedPane.h"

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

QWidget* NextTabFocusWidgetOutside(QWidget* widget)
{
  QWidget* next = widget;
  do
  {
    next = next->nextInFocusChain();
  } while (!(next->focusPolicy() & Qt::TabFocus) || widget->isAncestorOf(next));
  return next;
}
}  // namespace

TEST(AdvancedPaneUiTest, FormOwnsTheAdvancedSettingsStructure)
{
  QWidget pane;
  Ui::AdvancedPane ui;
  ui.setupUi(&pane);

  ASSERT_EQ(ui.rootLayout->count(), 8);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.cpuOptionsGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.timingGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(2)->widget(), ui.clockOverrideGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(3)->widget(), ui.vbiOverrideGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(4)->widget(), ui.memoryOverrideGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(5)->widget(), ui.customRtcGroup);
  EXPECT_EQ(ui.rootLayout->itemAt(6)->widget(), ui.resetSettingsGroup);
  EXPECT_NE(ui.rootLayout->itemAt(7)->spacerItem(), nullptr);

  ASSERT_EQ(ui.cpuOptionsLayout->count(), 4);
  EXPECT_EQ(ui.cpuOptionsLayout->itemAt(0)->layout(), ui.cpuEngineLayout);
  EXPECT_EQ(ui.cpuOptionsLayout->itemAt(1)->widget(), ui.enableMmuCheckBox);
  EXPECT_EQ(ui.cpuOptionsLayout->itemAt(2)->widget(), ui.pauseOnPanicCheckBox);
  EXPECT_EQ(ui.cpuOptionsLayout->itemAt(3)->widget(), ui.accurateCpuCacheCheckBox);
  EXPECT_EQ(ui.cpuEngineLayout->itemAt(0, QFormLayout::LabelRole)->widget(), ui.cpuEngineLabel);
  EXPECT_EQ(ui.cpuEngineLayout->itemAt(0, QFormLayout::FieldRole)->widget(), ui.cpuEngineComboBox);
  EXPECT_EQ(ui.cpuEngineLabel->buddy(), ui.cpuEngineComboBox);
  EXPECT_EQ(ui.cpuEngineComboBox->count(), 0);

  ASSERT_EQ(ui.timingLayout->count(), 3);
  EXPECT_EQ(ui.timingLayout->itemAt(0)->widget(), ui.correctTimeDriftCheckBox);
  EXPECT_EQ(ui.timingLayout->itemAt(1)->widget(), ui.rushFramePresentationCheckBox);
  EXPECT_EQ(ui.timingLayout->itemAt(2)->widget(), ui.smoothEarlyPresentationCheckBox);

  ASSERT_EQ(ui.clockOverrideLayout->count(), 2);
  EXPECT_EQ(ui.clockOverrideLayout->itemAt(0)->widget(), ui.cpuClockOverrideCheckBox);
  EXPECT_EQ(ui.clockOverrideLayout->itemAt(1)->layout(), ui.cpuClockSliderLayout);
  EXPECT_EQ(ui.cpuClockSliderLayout->itemAt(0)->widget(), ui.cpuClockSlider);
  EXPECT_EQ(ui.cpuClockSliderLayout->itemAt(1)->widget(), ui.cpuClockLabel);
  EXPECT_EQ(ui.cpuClockSlider->minimum(), 0);
  EXPECT_EQ(ui.cpuClockSlider->maximum(), 499);
  EXPECT_EQ(ui.cpuClockSlider->singleStep(), 1);

  ASSERT_EQ(ui.vbiOverrideLayout->count(), 2);
  EXPECT_EQ(ui.vbiOverrideLayout->itemAt(0)->widget(), ui.vbiOverrideCheckBox);
  EXPECT_EQ(ui.vbiOverrideLayout->itemAt(1)->layout(), ui.vbiSliderLayout);
  EXPECT_EQ(ui.vbiSliderLayout->itemAt(0)->widget(), ui.vbiOverrideSlider);
  EXPECT_EQ(ui.vbiSliderLayout->itemAt(1)->widget(), ui.vbiOverrideLabel);
  EXPECT_EQ(ui.vbiOverrideSlider->minimum(), 0);
  EXPECT_EQ(ui.vbiOverrideSlider->maximum(), 499);
  EXPECT_EQ(ui.vbiOverrideSlider->singleStep(), 1);

  ASSERT_EQ(ui.memoryOverrideLayout->count(), 3);
  EXPECT_EQ(ui.memoryOverrideLayout->itemAt(0)->widget(), ui.memoryOverrideCheckBox);
  EXPECT_EQ(ui.memoryOverrideLayout->itemAt(1)->layout(), ui.mem1SliderLayout);
  EXPECT_EQ(ui.memoryOverrideLayout->itemAt(2)->layout(), ui.mem2SliderLayout);
  EXPECT_EQ(ui.mem1Slider->minimum(), 24);
  EXPECT_EQ(ui.mem1Slider->maximum(), 64);
  EXPECT_EQ(ui.mem2Slider->minimum(), 64);
  EXPECT_EQ(ui.mem2Slider->maximum(), 256);

  ASSERT_EQ(ui.customRtcLayout->count(), 2);
  EXPECT_EQ(ui.customRtcLayout->itemAt(0)->widget(), ui.customRtcCheckBox);
  EXPECT_EQ(ui.customRtcLayout->itemAt(1)->widget(), ui.customRtcDateTimeEdit);
  ASSERT_EQ(ui.resetSettingsLayout->count(), 1);
  EXPECT_EQ(ui.resetSettingsLayout->itemAt(0)->widget(), ui.resetSettingsButton);
  EXPECT_FALSE(ui.resetSettingsButton->autoDefault());
  EXPECT_FALSE(ui.resetSettingsButton->isDefault());

  pane.resize(520, 850);
  ui.rootLayout->setGeometry(pane.rect());
  ui.cpuOptionsLayout->setGeometry(ui.cpuOptionsGroup->contentsRect());
  ui.cpuEngineLayout->setGeometry(ui.cpuOptionsLayout->itemAt(0)->geometry());
  ui.clockOverrideLayout->setGeometry(ui.clockOverrideGroup->contentsRect());
  ui.cpuClockSliderLayout->setGeometry(ui.clockOverrideLayout->itemAt(1)->geometry());
  ui.vbiOverrideLayout->setGeometry(ui.vbiOverrideGroup->contentsRect());
  ui.vbiSliderLayout->setGeometry(ui.vbiOverrideLayout->itemAt(1)->geometry());
  ui.memoryOverrideLayout->setGeometry(ui.memoryOverrideGroup->contentsRect());
  ui.mem1SliderLayout->setGeometry(ui.memoryOverrideLayout->itemAt(1)->geometry());
  ui.mem2SliderLayout->setGeometry(ui.memoryOverrideLayout->itemAt(2)->geometry());

  EXPECT_LT(ui.cpuEngineLabel->geometry().right(), ui.cpuEngineComboBox->geometry().left());
  EXPECT_LE(ui.cpuEngineComboBox->geometry().right(), ui.cpuOptionsGroup->contentsRect().right());

  const std::array slider_rows{
      std::pair{ui.cpuClockSlider, ui.cpuClockLabel},
      std::pair{ui.vbiOverrideSlider, ui.vbiOverrideLabel},
      std::pair{ui.mem1Slider, ui.mem1Label},
      std::pair{ui.mem2Slider, ui.mem2Label},
  };
  for (const auto& [slider, label] : slider_rows)
  {
    EXPECT_LT(slider->geometry().right(), label->geometry().left());
    EXPECT_GE(slider->geometry().left(), 0);
  }

  EXPECT_EQ(NextTabFocusWidget(ui.cpuEngineComboBox), ui.enableMmuCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.enableMmuCheckBox), ui.pauseOnPanicCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.pauseOnPanicCheckBox), ui.accurateCpuCacheCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.accurateCpuCacheCheckBox), ui.correctTimeDriftCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.smoothEarlyPresentationCheckBox), ui.cpuClockOverrideCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.cpuClockOverrideCheckBox), ui.cpuClockSlider);
  EXPECT_EQ(NextTabFocusWidget(ui.cpuClockSlider), ui.vbiOverrideCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.vbiOverrideSlider), ui.memoryOverrideCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.memoryOverrideCheckBox), ui.mem1Slider);
  EXPECT_EQ(NextTabFocusWidget(ui.mem1Slider), ui.mem2Slider);
  EXPECT_EQ(NextTabFocusWidget(ui.mem2Slider), ui.customRtcCheckBox);
  EXPECT_EQ(NextTabFocusWidget(ui.customRtcCheckBox), ui.customRtcDateTimeEdit);
  EXPECT_EQ(NextTabFocusWidgetOutside(ui.customRtcDateTimeEdit), ui.resetSettingsButton);
}
