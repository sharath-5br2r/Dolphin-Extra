// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

class AbstractTexture;

namespace VideoCommon
{
// True once GFX_LIBRASHADER_DUMP_CHAIN_IMAGES is set, GFX_LIBRASHADER_DUMP_CHAIN_DELAY_FRAMES
// frames have gone by with it set, and this run has not dumped yet. Counts a frame per call, so
// call it exactly once per frame the chain runs.
bool ShouldDumpChainImages();

// Spends the one-frame budget. Call once per dumping frame, after the images are written.
void NoteChainImagesDumped();

// Writes `texture` as PNG under <User>/Dump/Textures/. Synchronous full readback; only call
// behind ShouldDumpChainImages().
bool DumpChainImage(const AbstractTexture* texture, std::string_view label);

// Test-only reset for the process-global budget. The budget is process-global by design so it
// persists across frames; tests must re-arm it between test cases.
void ResetChainImageDumpBudgetForTest();
}  // namespace VideoCommon
