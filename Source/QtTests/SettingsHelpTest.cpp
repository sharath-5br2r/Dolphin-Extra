// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QPushButton>

#include <gtest/gtest.h>

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/SettingsHelp.h"

TEST(SettingsHelpTest, StockControlsPublishPersistentHelpMetadata)
{
  QPushButton button(QStringLiteral("Button title"));
  ConfigWidget::SetDescription(&button, {}, QStringLiteral("Binder description"));

  EXPECT_EQ(SettingsHelp::Title(&button), QStringLiteral("Button title"));
  EXPECT_EQ(SettingsHelp::Description(&button), QStringLiteral("Binder description"));
  EXPECT_EQ(ConfigWidget::ToolTipTitle(&button), QStringLiteral("Button title"));
  EXPECT_EQ(ConfigWidget::ToolTipDescription(&button), QStringLiteral("Binder description"));
}
