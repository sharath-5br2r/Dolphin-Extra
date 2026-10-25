// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.h>

#include "VideoBackends/Metal/MRCHelpers.h"

#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"

namespace Metal
{
// Metal binding for librashader's native filter chain. Owns the chain handle and the libra_mtl_*
// entry points; VideoCommon::LibrashaderPostProcessing owns everything else.
class MTLLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime
{
public:
  MTLLibrashaderRuntime();
  ~MTLLibrashaderRuntime() override;

  // Reports whether the librashader shared library was found, its ABI is compatible, and every
  // libra_mtl_* entry point resolved. AbstractGfx::CreatePostProcessor() uses it to decide whether
  // to fall back to the built-in engine. Safe to call before any chain exists.
  bool IsSupported() const override;

  bool CreateChain(libra_shader_preset_t preset) override;
  void DestroyChain() override;
  bool HasChain() const override { return m_chain != nullptr; }

  bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                u64 frame_count) override;
  void SetParameter(const char* name, float value) override;

  // Metal clears eagerly through BeginClearRenderPass when binding the backbuffer, so there is no
  // deferred clear to discard. Not overriding DiscardPendingTargetClear() is correct.

private:
  // librashader Metal filter chain; null means "no chain", which LibrashaderPostProcessing renders
  // as a passthrough copy. Spelled as the opaque struct rather than libra_mtl_filter_chain_t
  // because that typedef only exists behind LIBRA_RUNTIME_METAL, which includers of this header
  // (MTLGfx.mm) do not define.
  void* m_chain = nullptr;

  // Cache of the input texture view. The input from Dolphin is a 2D array texture but librashader's
  // Metal passes expect texture2d, so a type-only view is created. Replaced when the source
  // identity changes (resolution or format change). The source is retained to make the identity
  // check sound: without it, a freed texture whose address is reused would alias a stale entry.
  MRCOwned<id<MTLTexture>> m_input_view_source;
  MRCOwned<id<MTLTexture>> m_input_view;
};
}  // namespace Metal
