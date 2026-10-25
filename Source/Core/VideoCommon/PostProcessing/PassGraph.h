// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace VideoCommon
{
// Pure, GPU-free analysis of a .slangp pass chain's cross-frame / cross-pass sampler references.
// Used by the multipass executor to decide which render-stage features to set up before rendering.

// Aliases whose output is sampled by some pass as "<Alias>Feedback" (the previous frame's copy of
// that pass's output). Such passes must double-buffer their render target across frames.
// `all_sampler_names[p]` is pass p's input sampler names (binding order).
std::set<std::string>
ComputeFeedbackAliases(const std::vector<std::vector<std::string>>& all_sampler_names);

// If `name` is "OriginalHistoryN" for a non-negative integer N, returns N; otherwise nullopt.
// OriginalHistory0 == Original (the current frame's input); N>=1 is N frames earlier.
std::optional<u32> ParseOriginalHistoryIndex(const std::string& name);

// Highest OriginalHistoryN index with N>=1 referenced anywhere (0 if only History0 / none is
// used). This is the number of past-frame copies of Original the chain needs to retain.
u32 ComputeMaxHistoryIndex(const std::vector<std::vector<std::string>>& all_sampler_names);

// Pass indices whose OUTPUT must have a mipmap chain generated: for each pass i>0 whose
// `mipmap_input_flags[i]` is true, its Source is pass i-1's output, so i-1 is returned.
// mipmap_input on pass 0 marks nothing (its Source is the externally-owned game frame).
std::set<size_t> ComputeMipmapSourcePasses(const std::vector<bool>& mipmap_input_flags);
}  // namespace VideoCommon
