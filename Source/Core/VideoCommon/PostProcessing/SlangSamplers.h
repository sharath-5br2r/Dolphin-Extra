// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/PostProcessing/SlangPreset.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon
{
// has_mips controls mipmap filtering + lod range; filter_linear controls min/mag filter.
SamplerState MakeSlangSamplerState(SlangWrapMode wrap, bool filter_linear, bool has_mips);
}  // namespace VideoCommon
