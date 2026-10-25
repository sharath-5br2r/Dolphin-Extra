// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <vector>

#include "Common/CommonTypes.h"

namespace VideoCommon
{
struct MipLevel
{
  u32 width;
  u32 height;
  std::vector<u8> rgba8;  // width*height*4
};

// Number of mip levels in a full chain for a width x height level-0 image, i.e. down to 1x1.
constexpr u32 MipLevelCount(u32 width, u32 height)
{
  u32 levels = 1;
  for (u32 dim = std::max(width, height); dim > 1; dim >>= 1)
    ++levels;
  return levels;
}
static_assert(MipLevelCount(1, 1) == 1);
static_assert(MipLevelCount(2, 1) == 2);
static_assert(MipLevelCount(256, 256) == 9);
static_assert(MipLevelCount(640, 480) == 10);

// One dimension of `level`, halving each step and clamping at 1 (the usual GPU convention).
constexpr u32 MipLevelSize(u32 size, u32 level)
{
  const u32 shifted = size >> level;
  return shifted > 1 ? shifted : 1;
}
static_assert(MipLevelSize(640, 0) == 640);
static_assert(MipLevelSize(640, 1) == 320);
static_assert(MipLevelSize(640, 10) == 1);
static_assert(MipLevelSize(1, 5) == 1);

// level0 is the full-res RGBA8 image (width*height*4). Returns level0 plus successive
// half-size box-filtered levels down to 1x1.
std::vector<MipLevel> GenerateBoxMips(u32 width, u32 height, const u8* rgba8);
}  // namespace VideoCommon
