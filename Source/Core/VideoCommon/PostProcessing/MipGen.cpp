// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/MipGen.h"

#include <algorithm>

namespace VideoCommon
{
std::vector<MipLevel> GenerateBoxMips(u32 width, u32 height, const u8* rgba8)
{
  std::vector<MipLevel> mips;

  // Level 0: copy of the input.
  MipLevel level0;
  level0.width = width;
  level0.height = height;
  level0.rgba8.assign(rgba8, rgba8 + static_cast<size_t>(width) * height * 4);
  mips.push_back(std::move(level0));

  while (mips.back().width > 1 || mips.back().height > 1)
  {
    const MipLevel& prev = mips.back();
    const u32 w = std::max<u32>(1, prev.width / 2);
    const u32 h = std::max<u32>(1, prev.height / 2);

    MipLevel next;
    next.width = w;
    next.height = h;
    next.rgba8.resize(static_cast<size_t>(w) * h * 4);

    for (u32 y = 0; y < h; ++y)
    {
      // Source rows for the 2x2 block; clamp when the dimension is already 1.
      const u32 sy0 = std::min(y * 2, prev.height - 1);
      const u32 sy1 = std::min(y * 2 + 1, prev.height - 1);
      for (u32 x = 0; x < w; ++x)
      {
        const u32 sx0 = std::min(x * 2, prev.width - 1);
        const u32 sx1 = std::min(x * 2 + 1, prev.width - 1);

        const size_t i00 = (static_cast<size_t>(sy0) * prev.width + sx0) * 4;
        const size_t i01 = (static_cast<size_t>(sy0) * prev.width + sx1) * 4;
        const size_t i10 = (static_cast<size_t>(sy1) * prev.width + sx0) * 4;
        const size_t i11 = (static_cast<size_t>(sy1) * prev.width + sx1) * 4;
        const size_t dst = (static_cast<size_t>(y) * w + x) * 4;

        for (u32 c = 0; c < 4; ++c)
        {
          const u32 sum = static_cast<u32>(prev.rgba8[i00 + c]) + prev.rgba8[i01 + c] +
                          prev.rgba8[i10 + c] + prev.rgba8[i11 + c];
          next.rgba8[dst + c] = static_cast<u8>((sum + 2) / 4);
        }
      }
    }

    mips.push_back(std::move(next));
  }

  return mips;
}
}  // namespace VideoCommon
