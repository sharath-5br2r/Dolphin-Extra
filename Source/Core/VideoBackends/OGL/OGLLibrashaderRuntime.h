// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"

namespace OGL
{
class OGLTexture;

// OpenGL binding for librashader's native filter chain. Owns the chain handle, the libra_gl_*
// entry points and the two GL_TEXTURE_2D images the chain is handed;
// VideoCommon::LibrashaderPostProcessing owns everything else.
//
// Two things make OpenGL different from the other backends' runtimes:
//
//  - The chain has to be created with a function loader, because librashader resolves its own GL
//    entry points rather than being handed a device object.
//
//  - librashader's GL runtime speaks only GL_TEXTURE_2D: it binds input textures with
//    glBindTexture(GL_TEXTURE_2D) and attaches the output with
//    glFramebufferTexture2D(..., GL_TEXTURE_2D, ...), and the passes it compiles declare sampler2D.
//    Every Dolphin GL texture that reaches this runtime is a GL_TEXTURE_2D_ARRAY (the XFB texture,
//    LibrashaderPostProcessing's native source and its chain output are all
//    AbstractTextureType::Texture_2DArray), for which both of those calls are a
//    GL_INVALID_OPERATION. So the runtime keeps a GL_TEXTURE_2D image of its own on each side and
//    copies layer 0 in and out around the chain -- see the comment on RunFrame's blits for why a
//    copy rather than a glTextureView.
class OGLLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime
{
public:
  OGLLibrashaderRuntime();
  ~OGLLibrashaderRuntime() override;

  // Reports whether the librashader shared library was found, every libra_gl_* entry point
  // resolved, and this GL context can run a slang preset at all.
  bool IsSupported() const override;

  bool CreateChain(libra_shader_preset_t preset) override;
  void DestroyChain() override;
  bool HasChain() const override { return m_chain != nullptr; }

  bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                u64 frame_count) override;
  void SetParameter(const char* name, float value) override;

  // DiscardPendingTargetClear() is deliberately not overridden: OGLGfx has no deferred clear to
  // drop, SetAndClearFramebuffer() issues glClear immediately. It would not be reached anyway --
  // ShouldRenderChainDirectly() is false for every OpenGL present, because the window framebuffer
  // owns no texture for the chain to render into.

private:
  // A GL_TEXTURE_2D image mirroring one layer of a Dolphin texture, for handing to librashader.
  // GLuint and GLenum are spelled as u32 (which is what they are) so this header needs no GL
  // headers.
  struct Texture2DImage
  {
    u32 texture = 0;
    u32 width = 0;
    u32 height = 0;
    u32 internal_format = 0;
  };

  bool EnsureImage(Texture2DImage& image, u32 width, u32 height, u32 internal_format);
  void ReleaseImages();

  // librashader OpenGL filter chain; null means "no chain", which LibrashaderPostProcessing renders
  // as a passthrough copy. Spelled as the opaque struct rather than libra_gl_filter_chain_t because
  // that typedef only exists behind LIBRA_RUNTIME_OPENGL, which includers of this header
  // (OGLGfx.cpp) do not define.
  _filter_chain_gl* m_chain = nullptr;

  // The chain's input and output, kept across frames and reallocated when the size or format they
  // have to match changes.
  Texture2DImage m_source_image;
  Texture2DImage m_target_image;
};
}  // namespace OGL
