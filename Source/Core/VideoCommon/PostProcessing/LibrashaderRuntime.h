// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

class AbstractFramebuffer;
class AbstractTexture;

namespace VideoCommon
{
// One graphics backend's binding to a librashader native runtime.
//
// LibrashaderPostProcessing owns everything that is the same on every backend: preset
// resolution, the native-resolution source downscale, output sizing, the frame counter and the
// passthrough fallback. A runtime only has to create and free a chain and record one frame.
//
// The target is an AbstractFramebuffer rather than an AbstractTexture because D3D11's chain
// entry point takes an ID3D11RenderTargetView, and in Dolphin the RTV lives on DXFramebuffer
// while the SRV lives on DXTexture. Every other backend can reach its texture through
// GetColorAttachment().
class LibrashaderRuntime
{
public:
  virtual ~LibrashaderRuntime() = default;

  // True when every libra_<api>_* symbol this runtime needs resolved. Checked before a chain is
  // built, so an incomplete library degrades to the built-in executor rather than to a black
  // screen.
  virtual bool IsSupported() const = 0;

  // Builds a chain from `preset`. librashader invalidates the preset handle on BOTH success and
  // failure ("the shader preset is immediately invalidated"), so implementations must never free
  // it -- that would be a double free. Returns false and logs on failure.
  virtual bool CreateChain(libra_shader_preset_t preset) = 0;

  // Frees the chain, if there is one. Must be idempotent: LibrashaderPostProcessing's destructor
  // calls it before releasing the runtime, so an implementation whose own destructor also frees the
  // chain (a good idea, for standalone use) sees two calls. Guarding on a null handle is enough.
  virtual void DestroyChain() = 0;

  virtual bool HasChain() const = 0;

  // Records the whole chain, reading `source` and writing `target` over its full extent with the
  // viewport at (0,0). Called outside any render pass. Implementations must transition `source` to
  // shader-read and `target` to render-target themselves, and must reconcile whatever Dolphin
  // caches about the images they touched: the Vulkan runtime does this for the target's image
  // layout (OverrideImageLayout), because librashader leaves it in COLOR_ATTACHMENT_OPTIMAL without
  // a closing barrier.
  //
  // librashader also binds its own pipeline and descriptors on the same command list and does not
  // restore Dolphin's. Dolphin's Vulkan backend gets away with that incidentally -- the emulated
  // frame's own draws re-dirty everything the post-chain passthrough blit needs, so nothing is
  // invalidated explicitly and nothing has been observed to misrender. Do not read that as
  // "nothing is required": each backend must check whether its own state cache is as forgiving,
  // and invalidate what is not.
  virtual bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                        u64 frame_count) = 0;

  // Pushes a #pragma parameter override into the live chain. No-op without a chain.
  virtual void SetParameter(const char* name, float value) = 0;

  // Called before RunFrame() when the chain writes straight into the backbuffer, so a backend
  // with a deferred clear can drop it: the chain's final pass covers every pixel.
  virtual void DiscardPendingTargetClear() {}
};
}  // namespace VideoCommon
