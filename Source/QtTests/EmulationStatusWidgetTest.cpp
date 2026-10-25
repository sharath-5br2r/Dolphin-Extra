// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QLabel>
#include <gtest/gtest.h>

#include "DolphinQt/EmulationStatusWidget.h"

namespace
{
QLabel* Label(EmulationStatusWidget& widget, const char* name)
{
  return widget.findChild<QLabel*>(QLatin1String{name});
}
}  // namespace

TEST(EmulationStatusWidgetTest, IdleStatusHidesTheWidget)
{
  EmulationStatusWidget widget;

  widget.SetStatus({});

  EXPECT_TRUE(widget.isHidden());
  EXPECT_EQ(Label(widget, "statusRenderer")->contentsMargins(), QMargins(10, 0, 10, 0));
}

TEST(EmulationStatusWidgetTest, RunningStatusFormatsEveryAvailableMetric)
{
  EmulationStatusWidget widget;
  EmulationStatus status;
  status.active = true;
  status.renderer = QStringLiteral("Metal");
  status.width = 1920;
  status.height = 1584;
  status.fps = 59.94;
  status.vps = 60.0;
  status.speed = 0.999;
  status.volume = 75;

  widget.SetStatus(status);

  EXPECT_FALSE(widget.isHidden());
  ASSERT_NE(Label(widget, "statusRenderer"), nullptr);
  ASSERT_NE(Label(widget, "statusResolution"), nullptr);
  ASSERT_NE(Label(widget, "statusFps"), nullptr);
  ASSERT_NE(Label(widget, "statusVps"), nullptr);
  ASSERT_NE(Label(widget, "statusSpeed"), nullptr);
  ASSERT_NE(Label(widget, "statusVolume"), nullptr);
  EXPECT_EQ(Label(widget, "statusRenderer")->text(), QStringLiteral("Renderer: Metal"));
  EXPECT_EQ(Label(widget, "statusResolution")->text(), QStringLiteral("Resolution: 1920x1584"));
  EXPECT_EQ(Label(widget, "statusFps")->text(), QStringLiteral("FPS: 59.9"));
  EXPECT_EQ(Label(widget, "statusVps")->text(), QStringLiteral("VPS: 60.0"));
  EXPECT_EQ(Label(widget, "statusSpeed")->text(), QStringLiteral("Speed: 100%"));
  EXPECT_EQ(Label(widget, "statusVolume")->text(), QStringLiteral("Volume: 75%"));
}

TEST(EmulationStatusWidgetTest, UnavailableFieldsHideAndMutedVolumeIsExplicit)
{
  EmulationStatusWidget widget;
  EmulationStatus status;
  status.active = true;
  status.muted = true;

  widget.SetStatus(status);

  EXPECT_TRUE(Label(widget, "statusRenderer")->isHidden());
  EXPECT_TRUE(Label(widget, "statusResolution")->isHidden());
  EXPECT_EQ(Label(widget, "statusVolume")->text(), QStringLiteral("Volume: Muted"));
}
