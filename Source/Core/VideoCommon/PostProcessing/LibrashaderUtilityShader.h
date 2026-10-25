// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "VideoCommon/PostProcessing/SlangSourceDownscale.h"

namespace VideoCommon
{
std::string GenerateLibrashaderFullscreenVertexShader(bool flip_y, bool emit_texcoord = true);
std::string GenerateLibrashaderPassthroughPixelShader();
std::string
GenerateLibrashaderSourceNormalizationPixelShader(const SlangSourceDownscalePlan& plan);
}  // namespace VideoCommon
