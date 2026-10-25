// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "SignalRecorder.h"

TEST(ConfigChangeBroadcaster, InstanceIsASingleton)
{
  EXPECT_EQ(&ConfigWidget::ConfigChangeBroadcaster::Instance(),
            &ConfigWidget::ConfigChangeBroadcaster::Instance());
}

TEST(ConfigChangeBroadcaster, BroadcastEmitsChangedOncePerCall)
{
  auto& broadcaster = ConfigWidget::ConfigChangeBroadcaster::Instance();
  SignalRecorder recorder{&broadcaster, &ConfigWidget::ConfigChangeBroadcaster::Changed};

  broadcaster.Broadcast();
  EXPECT_EQ(recorder.Count(), 1);

  broadcaster.Broadcast();
  broadcaster.Broadcast();
  EXPECT_EQ(recorder.Count(), 3);
}
