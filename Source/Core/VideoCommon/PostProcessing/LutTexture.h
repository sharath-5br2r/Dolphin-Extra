// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoCommon/PostProcessing/SlangPreset.h"

class AbstractTexture;

namespace VideoCommon
{
// Loads a LUT PNG into a sampled 2D texture (RGBA8), generating mips if lut.mipmap.
// Returns nullptr on decode/read failure.
std::unique_ptr<AbstractTexture> LoadLutTexture(const SlangLutConfig& lut);
}  // namespace VideoCommon
