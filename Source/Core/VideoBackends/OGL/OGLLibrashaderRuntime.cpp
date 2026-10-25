// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This must stay above every #include in this file. LibrashaderLoader.h includes <librashader.h>
// deliberately without any LIBRA_RUNTIME_* macro, so it stays cheap for the rest of VideoCommon.
// Whichever translation unit includes it first thereby satisfies librashader.h's include guard, so
// a later `#include <librashader.h>` here would be a silent no-op leaving every libra_gl_*
// declaration out -- with no error, because the runtime sections are #ifdef'd, not #error'd.
#define LIBRA_RUNTIME_OPENGL
#include <librashader.h>

#include "VideoBackends/OGL/OGLLibrashaderRuntime.h"

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#include "Common/GL/GLContext.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/OGL/OGLConfig.h"
#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoBackends/OGL/OGLTexture.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace OGL
{
namespace
{
// OpenGL chain entry points, resolved once. Absent symbols leave the pointers null, which
// IsSupported() reports rather than calling through. Only the four the adapter calls are resolved:
// an unused required symbol is one more way for IsSupported() to fail for no reason. There is no
// _create_deferred here -- OpenGL is immediate-mode, so librashader exports no deferred variant.
struct OpenGLFunctions
{
  PFN_libra_gl_filter_chain_create create = nullptr;
  PFN_libra_gl_filter_chain_frame frame = nullptr;
  PFN_libra_gl_filter_chain_set_param set_param = nullptr;
  PFN_libra_gl_filter_chain_free free = nullptr;

  OpenGLFunctions()
  {
    using VideoCommon::Librashader::GetSymbol;
    create = reinterpret_cast<PFN_libra_gl_filter_chain_create>(
        GetSymbol("libra_gl_filter_chain_create"));
    frame =
        reinterpret_cast<PFN_libra_gl_filter_chain_frame>(GetSymbol("libra_gl_filter_chain_frame"));
    set_param = reinterpret_cast<PFN_libra_gl_filter_chain_set_param>(
        GetSymbol("libra_gl_filter_chain_set_param"));
    free =
        reinterpret_cast<PFN_libra_gl_filter_chain_free>(GetSymbol("libra_gl_filter_chain_free"));
  }

  bool Complete() const { return create && frame && set_param && free; }
};

const OpenGLFunctions& Functions()
{
  static const OpenGLFunctions s_functions;
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

// librashader resolves its own GL entry points through this at chain-creation time. Dolphin's
// GLContext is the only object that can answer for the active context, and OGLGfx owns it.
const void* GLLoader(const char* name)
{
  void* address = GetOGLGfx()->GetMainGLContext()->GetFuncAddress(name);
#if defined(__linux__) || defined(__APPLE__)
  // GLContextAGL never implemented GetFuncAddress, so on macOS the base class's `return nullptr`
  // comes back for every symbol. GLExtensions.cpp:2426-2436 handles this the same way for Dolphin's
  // own loader: RTLD_NEXT, not RTLD_DEFAULT, so the search starts after this image.
  if (address == nullptr)
    address = dlsym(RTLD_NEXT, name);
#endif
  return address;
}

struct GlslCaps
{
  // filter_chain_gl_opt_t::glsl_version, "should be at least 330".
  u16 glsl_version;
  // Direct State Access is GL 4.5 and up. librashader's header: "Using the shader cache requires
  // this option, so this option will implicitly disable the shader cache if false" -- which is why
  // disable_cache is never set by hand here. On macOS, capped at GL 4.1, this lands on false and
  // the chain is built uncached automatically.
  bool use_dsa;
};

// Maps Dolphin's GLSL version enum to the number librashader wants, at the floor of each value's
// range (OGLConfig.h:16-18 documents those ranges). Written as a switch rather than a comparison
// because the GlslEs* values sort ABOVE the desktop ones in this enum, so `>= Glsl450` would be
// true for GLES; IsSupported() already excludes GLES, and this keeps that trap from mattering here.
GlslCaps DescribeGlsl(GlslVersion version)
{
  switch (version)
  {
  case Glsl450:
    return {450, true};
  case Glsl430:
    return {430, false};
  case Glsl400:
    return {400, false};
  case Glsl330:
    return {330, false};
  default:
    // Unreachable: IsSupported() rejects everything below Glsl330 and every GLES version.
    return {330, false};
  }
}

// Attaches one layer of a texture as COLOR_ATTACHMENT0 of the framebuffer bound to `fb_target`.
// glFramebufferTextureLayer is only legal for layered textures and glFramebufferTexture2D only for
// non-layered ones, so the texture's own target decides which call to make.
void AttachColorLayer(GLenum fb_target, GLenum tex_target, GLuint texture, u32 layer)
{
  if (tex_target == GL_TEXTURE_2D_ARRAY || tex_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    glFramebufferTextureLayer(fb_target, GL_COLOR_ATTACHMENT0, texture, 0, layer);
  else
    glFramebufferTexture2D(fb_target, GL_COLOR_ATTACHMENT0, tex_target, texture, 0);
}

// 1:1 same-format copy of one layer, through the two framebuffers OGLGfx keeps for exactly this
// purpose. Mirrors OGLTexture::BlitFramebuffer, which cannot be used directly (it reaches
// glFramebufferTextureLayer for both sides, which is invalid for a non-array texture, and its
// glCopyImageSubData fast path needs GL 4.3). Source and destination rects are identical and cover
// the whole image, so this introduces no scaling and no flip. The caller restores the framebuffer
// binding.
void BlitLayer(GLenum src_target, GLuint src, u32 src_layer, GLenum dst_target, GLuint dst,
               u32 dst_layer, u32 width, u32 height)
{
  GetOGLGfx()->BindSharedReadFramebuffer();
  AttachColorLayer(GL_READ_FRAMEBUFFER, src_target, src, src_layer);
  GetOGLGfx()->BindSharedDrawFramebuffer();
  AttachColorLayer(GL_DRAW_FRAMEBUFFER, dst_target, dst, dst_layer);

  // glBlitFramebuffer is affected by the scissor test, which Dolphin leaves enabled.
  glDisable(GL_SCISSOR_TEST);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glEnable(GL_SCISSOR_TEST);
}
}  // namespace

OGLLibrashaderRuntime::OGLLibrashaderRuntime() = default;

OGLLibrashaderRuntime::~OGLLibrashaderRuntime()
{
  // DestroyChain() releases the scratch images too, and is idempotent.
  DestroyChain();
}

bool OGLLibrashaderRuntime::IsSupported() const
{
  if (!VideoCommon::Librashader::GetAvailability().available || !Functions().Complete())
    return false;

  // librashader's GL runtime compiles GLSL 330 and up, so GLES and the older desktop cores cannot
  // run a slang preset at all. The GlslEs* values sort above the desktop ones in this enum, so the
  // bIsES test is what actually excludes GLES -- the version comparison alone would admit it.
  //
  // bSupportsTextureStorage is in here because the GL_TEXTURE_2D images this runtime hands
  // librashader are allocated with glTexStorage2D (ARB_texture_storage, core in GL 4.2). Every
  // driver that can offer GLSL 330 has had the extension for over a decade; without it the backend
  // keeps the built-in executor instead of rendering a chain into an image with no storage.
  return !g_ogl_config.bIsES && g_ogl_config.eSupportedGLSLVersion >= Glsl330 &&
         g_ogl_config.bSupportsTextureStorage;
}

bool OGLLibrashaderRuntime::CreateChain(libra_shader_preset_t preset)
{
  const GlslCaps caps = DescribeGlsl(g_ogl_config.eSupportedGLSLVersion);

  filter_chain_gl_opt_t options = {};
  options.version = LIBRASHADER_CURRENT_VERSION;
  options.glsl_version = caps.glsl_version;
  options.use_dsa = caps.use_dsa;
  options.force_no_mipmaps = false;
  options.disable_cache = false;

  // create() invalidates `preset` on success and on failure alike (see the header: "the shader
  // preset is immediately invalidated"), so it is never freed here -- that would be a double free.
  // On error, leave m_chain null, which LibrashaderPostProcessing renders as a passthrough copy.
  if (CheckError(Functions().create(&preset, GLLoader, &options, &m_chain),
                 "gl_filter_chain_create"))
  {
    m_chain = nullptr;
    return false;
  }

  INFO_LOG_FMT(VIDEO, "Librashader: OpenGL filter chain created (GLSL {}, DSA {})",
               caps.glsl_version, caps.use_dsa ? "on" : "off");
  return true;
}

void OGLLibrashaderRuntime::DestroyChain()
{
  // Release the two scratch GL_TEXTURE_2D images unconditionally, before the early return, the way
  // DXLibrashaderRuntime::DestroyChain releases its cached input view: only RunFrame() allocates
  // them and it returns early without a chain, so the chain and the images always live and die
  // together. Leaving them to the destructor was a real leak rather than a reachability argument,
  // because LibrashaderPostProcessing::RecompileShader() destroys the chain and returns with this
  // runtime object alive -- so selecting Post-Processing "(off)" at 4K retained two full-size
  // images (~64 MB) for the rest of the session.
  ReleaseImages();

  if (m_chain == nullptr)
    return;

  Functions().free(&m_chain);
  m_chain = nullptr;
}

bool OGLLibrashaderRuntime::RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                                     u64 frame_count)
{
  if (m_chain == nullptr)
    return false;

  const auto* in_tex = static_cast<const OGLTexture*>(source);
  auto* out_tex = static_cast<OGLTexture*>(target->GetColorAttachment());
  if (out_tex == nullptr)
    return false;

  // The chain reads and writes GL_TEXTURE_2D images of ours, not Dolphin's array textures (see the
  // class comment). The formats are the sized internal ones -- glTexStorage2D requires that, and so
  // does librashader, which reuses the value it is given as the internal format of the
  // framebuffers it allocates for its own passes.
  const u32 in_format =
      OGLTexture::GetGLInternalFormatForTextureFormat(in_tex->GetFormat(), /*storage=*/true);
  const u32 out_format =
      OGLTexture::GetGLInternalFormatForTextureFormat(out_tex->GetFormat(), /*storage=*/true);
  if (!EnsureImage(m_source_image, in_tex->GetWidth(), in_tex->GetHeight(), in_format) ||
      !EnsureImage(m_target_image, out_tex->GetWidth(), out_tex->GetHeight(), out_format))
  {
    return false;
  }

  // Layer 0 only, matching every other runtime: they hand librashader the whole array texture and
  // its sampler2D passes read slice 0 of it, so the pixels the chain sees are the same ones.
  BlitLayer(in_tex->GetGLTarget(), in_tex->GetGLTextureId(), 0, GL_TEXTURE_2D,
            m_source_image.texture, 0, m_source_image.width, m_source_image.height);

  const libra_image_gl_t in{m_source_image.texture, m_source_image.internal_format,
                            m_source_image.width, m_source_image.height};
  const libra_image_gl_t out{m_target_image.texture, m_target_image.internal_format,
                             m_target_image.width, m_target_image.height};
  const libra_viewport_t vp{0.0f, 0.0f, static_cast<u32>(target->GetWidth()),
                            static_cast<u32>(target->GetHeight())};

  // No command list and no image transitions: GL is immediate-mode and librashader issues its own
  // barriers where it needs them. The images are by value.
  const bool failed = CheckError(
      Functions().frame(&m_chain, static_cast<size_t>(frame_count), in, out, &vp, nullptr, nullptr),
      "gl_filter_chain_frame");
  if (!failed)
  {
    BlitLayer(GL_TEXTURE_2D, m_target_image.texture, 0, out_tex->GetGLTarget(),
              out_tex->GetGLTextureId(), 0, m_target_image.width, m_target_image.height);
  }

  // librashader binds an FBO of its own to draw each pass into and does not put ours back (in the
  // non-DSA path it leaves FBO 0 bound, in the DSA path its last pass target), and the blits above
  // leave the shared read/draw framebuffers bound. RestoreFramebufferBinding() rebinds the
  // framebuffer OGLGfx believes is current, which is the caller's -- not `target`, which
  // LibrashaderPostProcessing passes without ever binding, so rebinding that instead would leave
  // the cache describing something else than GL holds.
  GetOGLGfx()->RestoreFramebufferBinding();

  // Everything else the chain bound is still bound, and every OGLGfx setter skips its work when
  // what it is asked for matches what it recorded. Nothing here re-dirties that by itself: the
  // passthrough blit that follows asks for the same output texture and the same pipeline every
  // frame, so from the second frame on it would early-out and draw with librashader's program,
  // textures and samplers instead of ours.
  GetOGLGfx()->InvalidateCachedState();

  return !failed;
}

void OGLLibrashaderRuntime::SetParameter(const char* name, float value)
{
  if (m_chain == nullptr)
    return;

  CheckError(Functions().set_param(&m_chain, name, value), "gl_filter_chain_set_param");
}

bool OGLLibrashaderRuntime::EnsureImage(Texture2DImage& image, u32 width, u32 height,
                                        u32 internal_format)
{
  if (image.texture != 0 && image.width == width && image.height == height &&
      image.internal_format == internal_format)
  {
    return true;
  }

  // Immutable storage cannot be respecified, so a changed size or format means a new texture.
  if (image.texture != 0)
    glDeleteTextures(1, &image.texture);
  image = {};

  GLuint texture = 0;
  glGenTextures(1, &texture);
  if (texture == 0)
    return false;

  // The scratch unit, not whatever unit happens to be active: binding into a real sampler slot
  // would make OGLGfx's cached binding for that slot a lie.
  ActivateMutableTextureUnit();
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexStorage2D(GL_TEXTURE_2D, 1, internal_format, width, height);
  glBindTexture(GL_TEXTURE_2D, 0);

  image = {texture, width, height, internal_format};
  return true;
}

void OGLLibrashaderRuntime::ReleaseImages()
{
  for (Texture2DImage* image : {&m_source_image, &m_target_image})
  {
    if (image->texture != 0)
      glDeleteTextures(1, &image->texture);
    *image = {};
  }
}
}  // namespace OGL
