// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "DolphinQt/SetupWizardPolicy.h"

TEST(SetupWizardPolicyTest, RunsForNewGuiProfiles)
{
  EXPECT_TRUE(SetupWizard::ShouldRun(false, false, false));
}

TEST(SetupWizardPolicyTest, ResumesInterruptedSetup)
{
  EXPECT_TRUE(SetupWizard::ShouldRun(true, true, false));
}

TEST(SetupWizardPolicyTest, LeavesEstablishedProfilesAlone)
{
  EXPECT_FALSE(SetupWizard::ShouldRun(true, false, false));
}

TEST(SetupWizardPolicyTest, NeverInterruptsBatchMode)
{
  EXPECT_FALSE(SetupWizard::ShouldRun(false, false, true));
  EXPECT_FALSE(SetupWizard::ShouldRun(true, true, true));
}
