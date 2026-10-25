// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture;

namespace VideoCommon
{
// Backend-agnostic post-processing engine interface. Presenter owns one of these and drives it
// per frame. Implemented by MultipassPostProcessing (the built-in slang executor) and by
// LibrashaderPostProcessing, which drives librashader's native runtime on any backend whose
// AbstractGfx::CreateLibrashaderRuntime() returns one. Both live in VideoCommon.
class IPostProcessor
{
public:
  virtual ~IPostProcessor() = default;

  virtual bool Initialize(AbstractTextureFormat format) = 0;
  virtual void RecompileShader() = 0;
  virtual void RecompilePipeline() = 0;
  virtual void BlitFromTexture(const MathUtil::Rectangle<int>& dst,
                               const MathUtil::Rectangle<int>& src,
                               const AbstractTexture* src_tex, int src_layer,
                               u32 native_width, u32 native_height) = 0;
};
}  // namespace VideoCommon
