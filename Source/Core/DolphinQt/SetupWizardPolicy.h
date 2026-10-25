// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace SetupWizard
{
inline constexpr char INCOMPLETE_SETTING[] = "setupwizard/incomplete";

constexpr bool ShouldRun(bool dolphin_config_existed, bool setup_incomplete, bool batch_mode)
{
  return !batch_mode && (!dolphin_config_existed || setup_incomplete);
}
}  // namespace SetupWizard
