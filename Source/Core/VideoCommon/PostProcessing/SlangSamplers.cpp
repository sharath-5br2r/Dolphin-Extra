// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/SlangSamplers.h"

#include "VideoCommon/BPMemory.h"

namespace VideoCommon
{
static WrapMode ToWrapMode(SlangWrapMode w)
{
  switch (w)
  {
  case SlangWrapMode::Repeat:
    return WrapMode::Repeat;
  case SlangWrapMode::MirroredRepeat:
    return WrapMode::Mirror;
  case SlangWrapMode::ClampToEdge:
  case SlangWrapMode::ClampToBorder:  // border unsupported; approximate with clamp
  default:
    return WrapMode::Clamp;
  }
}

SamplerState MakeSlangSamplerState(SlangWrapMode wrap, bool filter_linear, bool has_mips)
{
  SamplerState s;
  s.tm0.hex = 0;
  s.tm1.hex = 0;
  const FilterMode f = filter_linear ? FilterMode::Linear : FilterMode::Near;
  s.tm0.min_filter = f;
  s.tm0.mag_filter = f;
  s.tm0.mipmap_filter = has_mips ? FilterMode::Linear : FilterMode::Near;
  s.tm0.wrap_u = ToWrapMode(wrap);
  s.tm0.wrap_v = ToWrapMode(wrap);
  s.tm1.min_lod = 0;
  // max_lod is multiplied by 16 (see RenderState.h). 13 mip levels * 16 covers 8K textures.
  s.tm1.max_lod = has_mips ? (13u * 16u) : 0u;
  return s;
}
}  // namespace VideoCommon
