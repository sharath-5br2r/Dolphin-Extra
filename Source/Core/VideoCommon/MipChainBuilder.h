// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTexture.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;

namespace VideoCommon
{
// Fills mip levels 1..N-1 of a render-target texture with successive half-size filtered copies,
// for backends where AbstractTexture::GenerateMipmaps() is a no-op (see
// BackendInfo::bSupportsGPUMipGeneration). Each level is drawn into a scratch single-level target
// and then copied 1:1 into the real level, so a draw never reads and writes the same resource --
// which is what keeps this free of per-subresource barriers on D3D12.
//
// Resources are created lazily and reused across frames; one scratch target sized to level 1
// (a quarter of level 0) serves every level.
class MipChainBuilder
{
public:
  MipChainBuilder();
  ~MipChainBuilder();

  // Returns false if the pipeline or scratch target could not be created, in which case the
  // texture's mip levels are left as they were. Safe to call with a single-level texture (no-op).
  //
  // Preconditions:
  //  - The caller is already inside a g_gfx->BeginUtilityDrawing()/EndUtilityDrawing() scope.
  //    Generate() does not open its own: a nested EndUtilityDrawing() rebinds the EFB and restores
  //    the stored viewport, which is wrong in the middle of presenting. Generate() leaves the
  //    framebuffer, viewport, pipeline, texture and sampler bindings dirty for the caller to reset.
  //  - `texture` has exactly one array layer. Only layer 0 is downsampled and written; layers >= 1
  //    would keep whatever they were allocated with.
  bool Generate(AbstractTexture* texture);

private:
  bool EnsurePipeline(AbstractTextureFormat format);
  bool EnsureScratch(u32 width, u32 height, AbstractTextureFormat format);

  std::unique_ptr<AbstractShader> m_vertex_shader;
  std::unique_ptr<AbstractShader> m_pixel_shader;
  std::unique_ptr<AbstractPipeline> m_pipeline;
  AbstractTextureFormat m_pipeline_format = AbstractTextureFormat::Undefined;
  std::unique_ptr<AbstractTexture> m_scratch;
  std::unique_ptr<AbstractFramebuffer> m_scratch_framebuffer;
  bool m_failed = false;  // set after a creation failure so we don't retry every frame
};
}  // namespace VideoCommon
