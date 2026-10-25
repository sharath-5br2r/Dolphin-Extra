// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This must stay above every #include in this file. LibrashaderLoader.h includes <librashader.h>
// deliberately without any LIBRA_RUNTIME_* macro, so it stays cheap for the rest of VideoCommon.
// Whichever translation unit includes it first thereby satisfies librashader.h's include guard, so
// a later `#include <librashader.h>` here would be a silent no-op leaving every libra_mtl_*
// declaration out -- with no error, because the runtime sections are #ifdef'd, not #error'd. The
// Metal block is additionally gated on __OBJC__, which is the whole reason the file is a .mm; the
// header stays a plain .h with an opaque `void* m_chain`.
#define LIBRA_RUNTIME_METAL
#include <librashader.h>

#include "VideoBackends/Metal/MTLLibrashaderRuntime.h"

#include "Common/Logging/Log.h"

#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLStateTracker.h"
#include "VideoBackends/Metal/MTLTexture.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace Metal
{
namespace
{
// Metal chain entry points, resolved once. Absent symbols leave the pointers null, which
// IsSupported() reports rather than calling through. Only the four the adapter calls are resolved:
// an unused required symbol is one more way for IsSupported() to fail for no reason.
struct MetalFunctions
{
  PFN_libra_mtl_filter_chain_create create = nullptr;
  PFN_libra_mtl_filter_chain_frame frame = nullptr;
  PFN_libra_mtl_filter_chain_set_param set_param = nullptr;
  PFN_libra_mtl_filter_chain_free free = nullptr;

  MetalFunctions()
  {
    using VideoCommon::Librashader::GetSymbol;
    create = reinterpret_cast<PFN_libra_mtl_filter_chain_create>(
        GetSymbol("libra_mtl_filter_chain_create"));
    frame = reinterpret_cast<PFN_libra_mtl_filter_chain_frame>(
        GetSymbol("libra_mtl_filter_chain_frame"));
    set_param = reinterpret_cast<PFN_libra_mtl_filter_chain_set_param>(
        GetSymbol("libra_mtl_filter_chain_set_param"));
    free =
        reinterpret_cast<PFN_libra_mtl_filter_chain_free>(GetSymbol("libra_mtl_filter_chain_free"));
  }

  bool Complete() const { return create && frame && set_param && free; }
};

const MetalFunctions& Functions()
{
  static const MetalFunctions s_functions;
  return s_functions;
}

// Logs a librashader error (if any) and frees it. Returns true if an error was present.
bool CheckError(libra_error_t error, const char* context)
{
  if (error == nullptr)
    return false;

  ERROR_LOG_FMT(VIDEO, "Librashader: {} failed: {}", context,
                VideoCommon::Librashader::DescribeAndFreeError(error));
  return true;
}
}  // namespace

MTLLibrashaderRuntime::MTLLibrashaderRuntime() = default;

MTLLibrashaderRuntime::~MTLLibrashaderRuntime()
{
  DestroyChain();
}

bool MTLLibrashaderRuntime::IsSupported() const
{
  @autoreleasepool
  {
    return VideoCommon::Librashader::GetAvailability().available && Functions().Complete();
  }
}

bool MTLLibrashaderRuntime::CreateChain(libra_shader_preset_t preset)
{
  @autoreleasepool
  {
    // Chain options. filter_chain_mtl_opt_t has only two fields: version and force_no_mipmaps.
    // There is no frames_in_flight, no disable_cache, no force_hlsl_pipeline -- so do not carry
    // over Task 7's frames_in_flight reasoning or its comment, and do not set fields that do not
    // exist.
    filter_chain_mtl_opt_t options = {};
    options.version = LIBRASHADER_CURRENT_VERSION;
    options.force_no_mipmaps = false;

    // mtl_filter_chain_create invalidates the preset handle regardless of success or failure (see
    // the header: "the shader preset is immediately invalidated"), so it must not be freed on
    // either path
    // -- doing so would be a double-free. On error, leave m_chain null (passthrough).
    libra_mtl_filter_chain_t chain = nullptr;
    if (CheckError(Functions().create(&preset, g_queue, &options, &chain),
                   "mtl_filter_chain_create"))
    {
      m_chain = nullptr;
      return false;
    }

    m_chain = chain;
    INFO_LOG_FMT(VIDEO, "Librashader: Metal filter chain created");
    return true;
  }
}

void MTLLibrashaderRuntime::DestroyChain()
{
  @autoreleasepool
  {
    // Release the cached input view and its retained source first, unconditionally. Views can only
    // exist while a chain does (RunFrame is the only thing that creates them and returns early
    // without one), and the sole CreateChain call site is preceded by a DestroyChain, so ordering
    // these after the early return below would not actually strand anything today. Doing it up
    // front removes the reachability argument instead of relying on it.
    m_input_view_source.Reset();
    m_input_view.Reset();

    if (m_chain == nullptr)
      return;

    libra_mtl_filter_chain_t chain = static_cast<libra_mtl_filter_chain_t>(m_chain);
    Functions().free(&chain);
    m_chain = nullptr;
  }
}

bool MTLLibrashaderRuntime::RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                                     u64 frame_count)
{
  @autoreleasepool
  {
    if (m_chain == nullptr)
      return false;

    const auto* in_tex = static_cast<const Texture*>(source);
    auto* const fb = static_cast<Framebuffer*>(target);

    // THE TRAP: the output texture must come from the pass descriptor, not from
    // GetColorAttachment(). On the backbuffer, GetColorAttachment() yields a real Metal::Texture
    // object wrapping a nil id<MTLTexture> -- the live drawable is written only onto the
    // render-pass descriptor. This is correct for a regular framebuffer too: Metal::Framebuffer's
    // constructor seeds the descriptor from the colour attachment.
    id<MTLTexture> const output = fb->PassDesc().colorAttachments[0].texture;
    if (output == nil)
      return false;

    // The input from Dolphin is a 2D array texture (m_native_source is created as
    // AbstractTextureType::Texture_2DArray, which Metal::Gfx maps to MTLTextureType2DArray). At 1x
    // the chain input is Dolphin's own XFB texture, also an array, which the adapter does not own.
    // librashader's Metal runtime takes a view with
    // newTextureViewWithPixelFormat(input.pixelFormat())
    // -- the one-argument variant, which preserves the texture type, so it does not rescue us. Its
    // MSL passes declare texture2d. The fix is a zero-copy type-only view, available since macOS
    // 10.11, so no capability gate is needed. Replaced when the source identity changes.
    id<MTLTexture> mtl_source = in_tex->GetMTLTexture();
    if (m_input_view_source != mtl_source)
    {
      MRCOwned<id<MTLTexture>> view = MRCTransfer([mtl_source
          newTextureViewWithPixelFormat:mtl_source.pixelFormat
                            textureType:MTLTextureType2D
                                 levels:NSMakeRange(0, mtl_source.mipmapLevelCount)
                                 slices:NSMakeRange(0, 1)]);
      if (view == nil)
        return false;

      m_input_view = std::move(view);
      m_input_view_source = MRCRetain(mtl_source);
    }
    id<MTLTexture> input = m_input_view;

    libra_viewport_t vp{0.0f, 0.0f, static_cast<u32>(fb->GetWidth()),
                        static_cast<u32>(fb->GetHeight())};

    // libra_mtl_filter_chain_frame() must NOT be recorded inside a render pass: librashader creates
    // its own. End Dolphin's current render encoder first. Do not open a new one afterwards; the
    // base class's passthrough path will if it needs one.
    g_state_tracker->EndRenderPass();

    // The command buffer must be the one Dolphin is currently recording, so the chain's work is
    // ordered against Dolphin's own.
    id<MTLCommandBuffer> cmd = g_state_tracker->GetRenderCmdBuf();

    libra_mtl_filter_chain_t chain = static_cast<libra_mtl_filter_chain_t>(m_chain);
    const libra_error_t err =
        Functions().frame(&chain, cmd, frame_count, input, output, &vp, nullptr, nullptr);

    return !CheckError(err, "mtl_filter_chain_frame");
  }
}

void MTLLibrashaderRuntime::SetParameter(const char* name, float value)
{
  @autoreleasepool
  {
    if (m_chain == nullptr)
      return;

    libra_mtl_filter_chain_t chain = static_cast<libra_mtl_filter_chain_t>(m_chain);
    CheckError(Functions().set_param(&chain, name, value), "mtl_filter_chain_set_param");
  }
}
}  // namespace Metal
