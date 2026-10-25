// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/Constants.h"
#include "VideoCommon/PostProcessing/SlangShader.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VideoCommon.h"

class AbstractShader;

namespace VideoCommon
{
// Every texture a translated slang pass can sample is allocated with this type: the pass outputs
// and feedback buffers (MultipassPostProcessing::ConfigureChain), the Original history textures
// (which clone the XFB's config), the XFB itself (TextureCacheBase, layered for stereo), and the
// LUTs (LutTexture). Backends derive the view/target from TextureConfig::type alone, never from the
// layer count, so a pass must declare its samplers to match or the binding is silently wrong: on
// desktop OpenGL a sampler2D reads the texture unit's GL_TEXTURE_2D binding, which nothing sets for
// an array texture, and returns black with GL_NO_ERROR.
//
// Only three of those five sites can name this constant. The XFB's own type is not ours to pick --
// it is an array because stereo renders one eye per layer, a decision that predates and outranks
// post-processing -- and the history textures must copy the XFB's config verbatim, because
// ShiftHistory moves frames with CopyRectangleFromTexture. Writing SLANG_INPUT_TEXTURE_TYPE into
// either would invert the dependency and read as though the chain dictated the frame format. So the
// direction is kept honest by checking instead of imposing -- at run time only. There is nothing
// here for a static_assert to catch: the XFB's type is an argument at its allocation site
// (TextureCacheBase spells Texture_2DArray out by hand rather than leaning on the TextureConfig
// default), and the history textures copy it from the texture they are handed, so both are values
// no translation unit can see. The live checks are in MultipassPostProcessing: BlitFromTexture
// asserts the incoming type in the rebuild branch, which every preset with a translated chain
// passes through on its first frame and on every geometry change, and repeats the test per frame as
// a debug-only assert to cover a type that changes with the geometry unmoved; EnsureHistoryTextures
// asserts the type it is about to clone, and is only reached by presets that ask for
// OriginalHistoryN with N >= 1. The passthrough path returns before all three, so it is covered by
// nothing here -- its own shader spells sampler2DArray out, and that is the only thing holding it.
constexpr AbstractTextureType SLANG_INPUT_TEXTURE_TYPE = AbstractTextureType::Texture_2DArray;

// The GLSL sampler type that matches an AbstractTextureType binding.
constexpr std::string_view SlangSamplerGlslType(AbstractTextureType type)
{
  return type == AbstractTextureType::Texture_2DArray ? "sampler2DArray" : "sampler2D";
}

// The most texture samplers TranslateSlangPass will accept in one pass; beyond it the pass is
// rejected with an error rather than translated into a shader the backends cannot bind.
// (crt-royale's mask-apply pass needs 9, so this is not a theoretical ceiling.)
//
// Derived, not chosen. A translated pass is an ordinary Dolphin pixel shader, so the number of
// samplers it may declare is exactly the pixel sampler budget every backend already promises --
// deriving it means the two cannot drift, which is the failure this replaced: the ceiling used to
// be a local 16 in SlangTranslator.cpp justified by a comment naming Vulkan's
// NUM_UTILITY_PIXEL_SAMPLERS, with nothing tying the two together and no backend obliged to agree.
// Vulkan's constant is now derived from the same place (VideoBackends/Vulkan/Constants.h). What
// remains testable is that the enforcement below actually uses this number: see
// SlangTranslatorTest's boundary cases.
constexpr size_t SLANG_MAX_SAMPLERS = MAX_PIXEL_SHADER_SAMPLERS;

// The set of texture samplers a pass reads, in binding order (index 0..N-1).
// Includes "Source", "Original", each referenced alias, and each referenced LUT.
// GLSL scalar/vector/matrix category of a UBO member, used for std140 packing by the executor.
enum class UboMemberType
{
  Float,   // float / int / uint (4 bytes, 4-byte aligned)
  Vec2,    // 8 bytes, 8-byte aligned
  Vec3,    // 12 bytes, 16-byte aligned
  Vec4,    // 16 bytes, 16-byte aligned
  Mat4,    // 64 bytes, 16-byte aligned
  Unknown  // unrecognized; treated as vec4 to stay conservative
};

struct UboMember
{
  std::string name;
  UboMemberType type = UboMemberType::Unknown;
};

struct TranslatedPass
{
  std::string vertex_glsl;    // ready for g_gfx->CreateShaderFromSource(Vertex, ...)
  std::string fragment_glsl;  // ready for CreateShaderFromSource(Pixel, ...)
  std::vector<std::string> sampler_names;  // binding index -> semantic/alias/LUT name
  // The merged PSBlock members in declaration order (params block, then global block). The
  // executor packs the uniform buffer to match this std140 layout.
  std::vector<UboMember> ubo_members;
  bool ok = false;
  std::string error;  // set when ok == false (e.g. more samplers than SLANG_MAX_SAMPLERS)
};

// True when the injected fullscreen-triangle vertex shader must negate clip-space Y. NDC Y is
// flipped in Vulkan; we also flip on OpenGL so that (0,0) is the lower-left. Mirrors
// FramebufferShaderGen::GenerateScreenQuadVertexShader -- keep the two in sync.
//
// This is the answer for a draw whose render target is a texture Dolphin owns, which is what every
// caller of this predicate draws into. A draw that targets the *presented* framebuffer asks
// SlangNeedsPresentClipYFlip below instead; on OpenGL the two answers differ.
constexpr bool SlangNeedsClipYFlip(APIType api_type)
{
  return api_type == APIType::Vulkan || api_type == APIType::OpenGL;
}
static_assert(SlangNeedsClipYFlip(APIType::Vulkan));
static_assert(SlangNeedsClipYFlip(APIType::OpenGL));
static_assert(!SlangNeedsClipYFlip(APIType::D3D));
static_assert(!SlangNeedsClipYFlip(APIType::Metal));

// True when a fullscreen-triangle blit that targets the framebuffer being presented must negate
// clip-space Y.
//
// OpenGL puts clip-space Y = -1 at framebuffer row 0 whatever the target is; what differs is what
// row 0 means. When the target is one of Dolphin's textures, row 0 is texel row 0, and the rest of
// the stack -- uploads and readbacks (OGLTexture does not reorder rows), AbstractTexture::Save,
// CopyRectangleFromTexture -- treats texel row 0 as the image's top row, so the flip is exactly
// what lands the source's top row on the target's top row. The window's default framebuffer holds
// no texels and obeys no such convention: its row 0 is the bottom scanline of the display, so the
// same flip presents the frame upside down. Vulkan needs the flip for both, because its clip space
// is Y-down and row 0 is the top edge of a texture and of a swapchain image alike; D3D and Metal
// need it for neither.
//
// The single-pass post-processor this tree replaced encoded the same split, and spelled out why:
// d157e51018^:Source/Core/VideoCommon/PostProcessing.cpp, GetVertexShaderBody() -- "Vulkan Y needs
// to be inverted on every pass" / "OpenGL Y needs to be inverted in all passes except the last
// one", the last pass being the one that renders to the screen. That file was deleted by
// d157e51018 ("replace single-pass post-processor with multi-pass slang pipeline"), which is where
// the two answers were collapsed into one predicate and the OpenGL present became inverted; it is
// no longer in the tree, hence the commit-relative path.
constexpr bool SlangNeedsPresentClipYFlip(APIType api_type)
{
  return api_type == APIType::Vulkan;
}
static_assert(SlangNeedsPresentClipYFlip(APIType::Vulkan));
static_assert(!SlangNeedsPresentClipYFlip(APIType::OpenGL));
static_assert(!SlangNeedsPresentClipYFlip(APIType::D3D));
static_assert(!SlangNeedsPresentClipYFlip(APIType::Metal));

// known_aliases: names produced by earlier passes; lut_names: declared LUTs.
// flip_clip_y: see SlangNeedsClipYFlip. Callers pass SlangNeedsClipYFlip(g_backend_info.api_type).
TranslatedPass TranslateSlangPass(const SlangShaderSource& shader,
                                  const std::vector<std::string>& known_aliases,
                                  const std::vector<std::string>& lut_names, bool flip_clip_y);

// Packs a std140 uniform buffer matching `members` (in declaration order). For each member,
// `resolver(name, out)` fills `out` with the member's component values (1..16 floats); if it
// returns false the member is zero-filled. Applies std140 alignment rules for the supported
// member types (Float/Vec2/Vec3/Vec4/Mat4). Pure/testable.
using UniformResolver = std::function<bool(const std::string& name, float* out, int count)>;
std::vector<u8> PackSlangUniforms(const std::vector<UboMember>& members,
                                  const UniformResolver& resolver);

struct CompiledPassShaders
{
  std::unique_ptr<AbstractShader> vertex;
  std::unique_ptr<AbstractShader> pixel;
};

// Compiles a translated pass. include_dir roots the #include resolver (the directory of the
// .slang file); the Sys shaders dir is added as the system include root. Returns empty uniques
// on failure.
CompiledPassShaders CompileTranslatedPass(const TranslatedPass& pass,
                                          const std::string& include_dir);

// Compiles the vertex stage alone, with the same include roots. For replacing one pass's vertex
// shader with a differently-translated one (a different flip_clip_y) without paying for its
// fragment stage again -- see MultipassPostProcessing::RetargetFinalPassToPresent.
std::unique_ptr<AbstractShader> CompileTranslatedVertex(const std::string& vertex_glsl,
                                                        const std::string& include_dir);
}  // namespace VideoCommon
