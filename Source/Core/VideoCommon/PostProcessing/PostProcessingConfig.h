// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace VideoCommon
{
// GFX_ENHANCE_POST_SHADER may hold a ';'-separated chain from before the chain feature was
// removed. ResolveConfiguredPreset takes the first entry and warns if any are dropped, so
// existing configs keep working and the loss is visible. Both the librashader and built-in
// post-processing engines share this rule.
std::string ResolveConfiguredPreset(const std::string& preset_spec);
}  // namespace VideoCommon
