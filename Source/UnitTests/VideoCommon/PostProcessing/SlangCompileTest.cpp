// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// End-to-end translator oracle: run a real RetroArch-slang-shaped shader through the parser +
// translator, then compile the result with glslang (the exact SPIRV::Compile* path the Vulkan
// backend uses). This catches ABI mismatches (uniform blocks, vertex attributes, macro
// collisions) that a text-only test cannot.

#include <array>
#include <sstream>
#include <string>

#include <cstdio>

#include <glslang/Public/ResourceLimits.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

// SPIRV-Cross is only built where a video backend needs it: CMakeLists.txt adds
// Externals/spirv_cross under `if(WIN32 OR APPLE)`, for D3D11/D3D12 and Metal. On Android and
// Linux the target does not exist at all, so the HLSL/MSL half of the oracle has to compile out
// rather than break the build. Everything else here runs through glslang, which is available
// everywhere, so all four backend rows keep their coverage on every platform.
#if defined(_WIN32) || defined(__APPLE__)
#define DOLPHIN_TEST_HAS_SPIRV_CROSS
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>
#endif

#include "VideoCommon/PostProcessing/LibrashaderUtilityShader.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"
#include "VideoCommon/PostProcessing/SlangShader.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/Spirv.h"

using namespace VideoCommon;

namespace
{
// The Vulkan backend prepends this header (mirrors VideoBackends/Vulkan/ShaderCompiler.cpp's
// SHADER_HEADER) before calling SPIRV::Compile*; g_gfx->CreateShaderFromSource does this at
// runtime. The oracle must replicate it to faithfully compile what the backend would.
constexpr const char* VULKAN_SHADER_HEADER = R"(
  #version 450 core
  #extension GL_ARB_shading_language_include : enable
  #define ATTRIBUTE_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
  #define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (x - 1))
  #define SAMPLER_BINDING(x) layout(set = 1, binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(set = 1, binding = (x + 16))
  #define SSBO_BINDING(x) layout(std430, set = 2, binding = x)
  #define INPUT_ATTACHMENT_BINDING(x, y, z) layout(set = x, binding = y, input_attachment_index = z)
  #define VARYING_LOCATION(x) layout(location = x)
  #define FORCE_EARLY_Z layout(early_fragment_tests) in
  #define FB_FETCH_VALUE subpassLoad(in_ocol0)
  #define API_VULKAN 1
  #define float2 vec2
  #define float3 vec3
  #define float4 vec4
  #define uint2 uvec2
  #define uint3 uvec3
  #define uint4 uvec4
  #define int2 ivec2
  #define int3 ivec3
  #define int4 ivec4
  #define frac fract
  #define lerp mix
  #define gl_VertexID gl_VertexIndex
  #define gl_InstanceID gl_InstanceIndex
)";

// D3D backend header, copied verbatim from VideoBackends/D3DCommon/Shader.cpp SHADER_HEADER.
constexpr const char* D3D_SHADER_HEADER = R"(
  // Target GLSL 4.5.
  #version 450 core

  #extension GL_ARB_shading_language_include : enable

  #define ATTRIBUTE_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
  #define UBO_BINDING(packing, x) layout(packing, binding = (x - 1))
  #define SAMPLER_BINDING(x) layout(binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(binding = x)
  #define SSBO_BINDING(x) layout(binding = (x + 2))
  #define VARYING_LOCATION(x) layout(location = x)
  #define FORCE_EARLY_Z layout(early_fragment_tests) in

  // hlsl to glsl function translation
  #define float2 vec2
  #define float3 vec3
  #define float4 vec4
  #define uint2 uvec2
  #define uint3 uvec3
  #define uint4 uvec4
  #define int2 ivec2
  #define int3 ivec3
  #define int4 ivec4
  #define frac fract
  #define lerp mix

  #define API_D3D 1
)";

// Metal backend header, copied verbatim from VideoBackends/Metal/MTLUtil.mm SHADER_HEADER.
constexpr const char* METAL_SHADER_HEADER = R"(
// Target GLSL 4.5.
#version 450 core
// Always available on Metal
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#define ATTRIBUTE_LOCATION(x) layout(location = x)
#define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
#define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
#define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (x - 1))
#define SAMPLER_BINDING(x) layout(set = 1, binding = x)
#define TEXEL_BUFFER_BINDING(x) layout(set = 1, binding = (x + 8))
#define SSBO_BINDING(x) layout(std430, set = 2, binding = x)
#define INPUT_ATTACHMENT_BINDING(x, y, z) layout(set = x, binding = y, input_attachment_index = z)
#define VARYING_LOCATION(x) layout(location = x)
#define FORCE_EARLY_Z layout(early_fragment_tests) in

// Metal framebuffer fetch helpers.
#define FB_FETCH_VALUE subpassLoad(in_ocol0)

// hlsl to glsl function translation
#define API_METAL 1
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define frac fract
#define lerp mix

// These were changed in Vulkan
#define gl_VertexID gl_VertexIndex
#define gl_InstanceID gl_InstanceIndex
)";

// OpenGL backend header — hand-maintained mirror of the modern-desktop variant assembled at
// ProgramShaderCache.cpp runtime. The real OGL backend leaves VARYING_LOCATION empty
// (ProgramShaderCache.cpp:790, with a TODO to define it if using bSupportsExplicitLayoutInShader)
// and matches varyings by name, because it hands GLSL straight to the driver. This test must
// define VARYING_LOCATION anyway: SPIRV::Compile* always targets SPIR-V (Spirv.cpp:179), and
// SPIR-V requires explicit locations on user varyings. Therefore this row does NOT prove
// name-based varying matching — that coverage is not available through this API and is a known
// gap. What it DOES prove: the translated GLSL compiles under APIType::OpenGL, i.e. without
// EShMsgVulkanRules and without the Vulkan header's gl_VertexID/gl_InstanceID aliases, so no
// Vulkan-only builtin or construct survives translation. That is real, distinct coverage. Note
// the real OGL backend never calls SPIRV::Compile* at all (no such call exists anywhere under
// Source/Core/VideoBackends/OGL/), so this row is a translate-and-compile proxy rather than
// driver acceptance.
constexpr const char* OGL_SHADER_HEADER = R"(
  #version 450 core

  #extension GL_ARB_explicit_attrib_location : enable
  #define ATTRIBUTE_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
  #define UBO_BINDING(packing, x) layout(packing, binding = x)
  #define SAMPLER_BINDING(x) layout(binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(binding = x)
  #define SSBO_BINDING(x) layout(std430, binding = x)
  #define IMAGE_BINDING(format, x) layout(format, binding = x)

  #define VARYING_LOCATION(x) layout(location = x)

  #define API_OPENGL 1
  #define float2 vec2
  #define float3 vec3
  #define float4 vec4
  #define uint2 uvec2
  #define uint3 uvec3
  #define uint4 uvec4
  #define int2 ivec2
  #define int3 ivec3
  #define int4 ivec4
  #define frac fract
  #define lerp mix
)";

struct BackendShaderHeader
{
  const char* name;
  APIType api_type;
  const char* header;
  glslang::EShTargetLanguageVersion spv_version;
};

constexpr BackendShaderHeader BACKEND_HEADERS[] = {
    {"Vulkan", APIType::Vulkan, VULKAN_SHADER_HEADER, glslang::EShTargetSpv_1_0},
    {"D3D", APIType::D3D, D3D_SHADER_HEADER, glslang::EShTargetSpv_1_0},
    {"Metal", APIType::Metal, METAL_SHADER_HEADER, glslang::EShTargetSpv_1_5},
    {"OpenGL", APIType::OpenGL, OGL_SHADER_HEADER, glslang::EShTargetSpv_1_0},
};

// Compiles both stages of a translated pass with one backend's GLSL preamble; returns true only if
// both succeed, and on failure *which_failed names the backend and stage.
bool CompilesOnBackend(const TranslatedPass& pass, const BackendShaderHeader& backend,
                       std::string* which_failed)
{
  const std::string vs_src = std::string(backend.header) + "\n" + pass.vertex_glsl;
  const std::string fs_src = std::string(backend.header) + "\n" + pass.fragment_glsl;
  const auto vs = SPIRV::CompileVertexShader(vs_src, backend.api_type, backend.spv_version, nullptr);
  if (!vs)
  {
    *which_failed = std::string(backend.name) + " vertex";
    return false;
  }
  const auto fs = SPIRV::CompileFragmentShader(fs_src, backend.api_type, backend.spv_version, nullptr);
  if (!fs)
  {
    *which_failed = std::string(backend.name) + " fragment";
    return false;
  }
  return true;
}

#ifdef DOLPHIN_TEST_HAS_SPIRV_CROSS
// Cross-compiles fragment SPIR-V exactly the way D3DCommon/Shader.cpp GetHLSLFromSPIRV does for
// feature level 11 (shader_model = 50).
std::string HlslFromSpirv(const SPIRV::CodeVector& spv)
{
  spirv_cross::CompilerHLSL::Options options;
  options.shader_model = 50;
  spirv_cross::CompilerHLSL compiler(spv);
  compiler.set_hlsl_options(options);
  return compiler.compile();
}

// ... and the way Metal/MTLUtil.mm does on macOS (MSL 2.3, framebuffer-fetch subpasses).
std::string MslFromSpirv(const SPIRV::CodeVector& spv)
{
  spirv_cross::CompilerMSL::Options options;
  options.platform = spirv_cross::CompilerMSL::Options::macOS;
  options.set_msl_version(2, 3);
  options.use_framebuffer_fetch_subpasses = true;
  spirv_cross::CompilerMSL compiler(spv);
  compiler.set_msl_options(options);
  return compiler.compile();
}
#endif  // DOLPHIN_TEST_HAS_SPIRV_CROSS

const BackendShaderHeader& BackendNamed(const char* name)
{
  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    if (std::string_view(backend.name) == name)
      return backend;
  }
  ADD_FAILURE() << "no backend named " << name;
  return BACKEND_HEADERS[0];
}
}  // namespace

TEST(SlangCompile, LibrashaderUtilityShadersCompileOnAllBackends)
{
  const SlangSourceDownscalePlan bilinear_plan{.normalize = true};
  const SlangSourceDownscalePlan box_plan{
      .normalize = true, .box_filter = true, .factor = 3};

  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    const auto expect_compiles = [&backend](std::string_view name, const std::string& vertex,
                                            const std::string& fragment) {
      SCOPED_TRACE(std::string(backend.name) + " " + std::string(name));
      const auto vs = SPIRV::CompileVertexShader(std::string(backend.header) + vertex,
                                                 backend.api_type, backend.spv_version, nullptr);
      ASSERT_TRUE(vs.has_value());
      const auto fs = SPIRV::CompileFragmentShader(std::string(backend.header) + fragment,
                                                   backend.api_type, backend.spv_version, nullptr);
      ASSERT_TRUE(fs.has_value());
    };

    expect_compiles("passthrough",
                    GenerateLibrashaderFullscreenVertexShader(
                        SlangNeedsPresentClipYFlip(backend.api_type)),
                    GenerateLibrashaderPassthroughPixelShader());
    expect_compiles(
        "bilinear normalization",
        GenerateLibrashaderFullscreenVertexShader(SlangNeedsClipYFlip(backend.api_type)),
        GenerateLibrashaderSourceNormalizationPixelShader(bilinear_plan));
    expect_compiles(
        "box normalization",
        GenerateLibrashaderFullscreenVertexShader(SlangNeedsClipYFlip(backend.api_type), false),
        GenerateLibrashaderSourceNormalizationPixelShader(box_plan));
  }

  const std::string box_vertex = GenerateLibrashaderFullscreenVertexShader(false, false);
  EXPECT_EQ(box_vertex.find("v_tex0"), std::string::npos);
}

// The canonical RetroArch "stock" passthrough shader: dual uniform blocks (push_constant Push
// {} params + std140 UBO {} global), vertex attributes Position/TexCoord, and global.MVP.
TEST(SlangCompile, StockShaderCompilesOnAllBackends)
{
  const std::string text =
      "#version 450\n"
      "layout(push_constant) uniform Push\n"
      "{\n"
      "    vec4 SourceSize;\n"
      "    vec4 OriginalSize;\n"
      "    vec4 OutputSize;\n"
      "    uint FrameCount;\n"
      "} params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO\n"
      "{\n"
      "    mat4 MVP;\n"
      "} global;\n"
      "#pragma stage vertex\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 1) in vec2 TexCoord;\n"
      "layout(location = 0) out vec2 vTexCoord;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = global.MVP * Position;\n"
      "   vTexCoord = TexCoord;\n"
      "}\n"
      "#pragma stage fragment\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "void main()\n"
      "{\n"
      "    FragColor = vec4(texture(Source, vTexCoord).rgb, 1.0);\n"
      "}\n";

  std::string error;
  const auto parsed = ParseSlangShader(text, &error);
  ASSERT_TRUE(parsed.has_value()) << error;

  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    SCOPED_TRACE(backend.name);
    const auto translated =
        TranslateSlangPass(*parsed, {}, {}, SlangNeedsClipYFlip(backend.api_type));
    ASSERT_TRUE(translated.ok) << translated.error;

    std::string which;
    EXPECT_TRUE(CompilesOnBackend(translated, backend, &which))
        << which << " stage failed to compile:\nVS:\n"
        << translated.vertex_glsl << "\nFS:\n"
        << translated.fragment_glsl;
  }
}

// The HLSL-compat macros RetroArch shaders pull in (lerp/frac/mul/float2...) collide with
// Dolphin's own backend-prepended macros; the translation must compile regardless.
TEST(SlangCompile, CompatMacrosCompileOnAllBackends)
{
  const std::string text =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; } params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n"
      "#define lerp(a,b,c) mix(a,b,c)\n"
      "#define frac(x) (fract(x))\n"
      "#define mul(a,b) (b*a)\n"
      "#define float4 vec4\n"
      "#pragma stage vertex\n"
      "layout(location = 0) in vec4 Position;\n"
      "void main() { gl_Position = global.MVP * Position; }\n"
      "#pragma stage fragment\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "void main() { FragColor = lerp(float4(0.0), texture(Source, vec2(0.5)), frac(0.5)); }\n";

  std::string error;
  const auto parsed = ParseSlangShader(text, &error);
  ASSERT_TRUE(parsed.has_value()) << error;

  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    SCOPED_TRACE(backend.name);
    const auto translated =
        TranslateSlangPass(*parsed, {}, {}, SlangNeedsClipYFlip(backend.api_type));
    ASSERT_TRUE(translated.ok) << translated.error;

    std::string which;
    EXPECT_TRUE(CompilesOnBackend(translated, backend, &which))
        << which << " stage failed:\nVS:\n"
        << translated.vertex_glsl << "\nFS:\n"
        << translated.fragment_glsl;
  }
}

namespace
{
// A pass shaped like the ones that actually break: the sampler is handed to a user function
// (crt-royale's `tex2D_linearize(sampler2D tex, vec2 coords)`) and reached through a macro, so no
// name-based rewrite of the call sites could ever see it. Also exercises every sampling entry
// point the real libretro pack uses on a 2D sampler.
constexpr const char* SAMPLER_SHADER = R"(#version 450
layout(push_constant) uniform Push { vec4 SourceSize; } params;
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() { gl_Position = global.MVP * Position; vTexCoord = TexCoord; }
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
#define SAMPLE(t, c) texture(t, c)
vec4 tex2D_linearize(sampler2D tex, vec2 coords)
{
  return SAMPLE(tex, coords) + texelFetch(tex, ivec2(coords), 0) +
         textureLod(tex, coords, 1.0) + textureOffset(tex, coords, ivec2(1, 0)) +
         textureLodOffset(tex, coords, 1.0, ivec2(1, 0)) +
         texelFetchOffset(tex, ivec2(coords), 0, ivec2(1, 0)) +
         textureGrad(tex, coords, vec2(0.0), vec2(0.0)) + textureGather(tex, coords) +
         textureGather(tex, coords, 1) + texture(tex, coords, 0.5) +
         vec4(vec2(textureSize(tex, 0)), 0.0, 0.0);
}
void main() { FragColor = tex2D_linearize(Source, vTexCoord); }
)";

TranslatedPass TranslateForBackend(const char* shader_text, const BackendShaderHeader& backend)
{
  std::string error;
  const auto parsed = ParseSlangShader(shader_text, &error);
  EXPECT_TRUE(parsed.has_value()) << error;
  if (!parsed.has_value())
    return {};
  return TranslateSlangPass(*parsed, {}, {}, SlangNeedsClipYFlip(backend.api_type));
}
}  // namespace

// H1. Every texture the slang chain binds is allocated as SLANG_INPUT_TEXTURE_TYPE, which is
// Texture_2DArray: MultipassPostProcessing's pass outputs and feedback buffers, the history
// textures that clone the XFB's config, the XFB itself, and the LUTs. A `sampler2D` declared
// against one of those samples the texture unit's *2D* binding, which nothing ever sets -- on
// desktop OpenGL that is object 0, i.e. black, returned with GL_NO_ERROR (measured on GL 4.6 /
// NVIDIA 596.49). glslang accepts `sampler2D` happily, so this can only be caught by asserting on
// the generated declaration.
TEST(SlangCompile, SamplersAreDeclaredAsArrays)
{
  const std::string expected =
      "uniform " + std::string(SlangSamplerGlslType(SLANG_INPUT_TEXTURE_TYPE)) + " Source;";
  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    SCOPED_TRACE(backend.name);
    const auto translated = TranslateForBackend(SAMPLER_SHADER, backend);
    ASSERT_TRUE(translated.ok) << translated.error;
    EXPECT_NE(translated.fragment_glsl.find(expected), std::string::npos)
        << "expected `" << expected << "` in:\n"
        << translated.fragment_glsl;
    EXPECT_EQ(translated.fragment_glsl.find("uniform sampler2D Source;"), std::string::npos)
        << "sampler2D declared against a Texture_2DArray binding";
  }
}

// The real oracle: assert what reaches the driver, not what the translator wrote. This is the
// assertion the previous oracle was missing -- it stopped at SPIRV::Compile*, and `sampler2D` is
// valid GLSL, so all four rows passed a chain that rendered black.
//
// Needs SPIRV-Cross, so this one test is Windows/macOS-only (see the include guard above). That
// costs nothing in practice: it asserts what the D3D and Metal backends generate, and those
// backends only exist on the platforms where the dependency is built.
#ifdef DOLPHIN_TEST_HAS_SPIRV_CROSS
TEST(SlangCompile, CrossCompiledSamplersAreArrayTextures)
{
  {
    const BackendShaderHeader& d3d = BackendNamed("D3D");
    const auto translated = TranslateForBackend(SAMPLER_SHADER, d3d);
    ASSERT_TRUE(translated.ok) << translated.error;
    const auto spv = SPIRV::CompileFragmentShader(std::string(d3d.header) + "\n" +
                                                      translated.fragment_glsl,
                                                  d3d.api_type, d3d.spv_version, nullptr);
    ASSERT_TRUE(spv.has_value()) << translated.fragment_glsl;
    const std::string hlsl = HlslFromSpirv(*spv);
    EXPECT_NE(hlsl.find("Texture2DArray<float4> Source"), std::string::npos) << hlsl;
    EXPECT_EQ(hlsl.find("Texture2D<float4> Source"), std::string::npos) << hlsl;
  }
  {
    const BackendShaderHeader& metal = BackendNamed("Metal");
    const auto translated = TranslateForBackend(SAMPLER_SHADER, metal);
    ASSERT_TRUE(translated.ok) << translated.error;
    const auto spv = SPIRV::CompileFragmentShader(std::string(metal.header) + "\n" +
                                                      translated.fragment_glsl,
                                                  metal.api_type, metal.spv_version, nullptr);
    ASSERT_TRUE(spv.has_value()) << translated.fragment_glsl;
    const std::string msl = MslFromSpirv(*spv);
    EXPECT_NE(msl.find("texture2d_array<float> Source"), std::string::npos) << msl;
    EXPECT_EQ(msl.find("texture2d<float> Source"), std::string::npos) << msl;
  }
}
#endif  // DOLPHIN_TEST_HAS_SPIRV_CROSS

// The array declaration is only useful if the 2-coordinate call sites still compile. They are
// reached through a macro and through a user function's sampler parameter, so the shim overloads
// -- not a call-site rewrite -- are what has to carry them.
TEST(SlangCompile, TwoCoordinateCallSitesCompileOnAllBackends)
{
  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    SCOPED_TRACE(backend.name);
    const auto translated = TranslateForBackend(SAMPLER_SHADER, backend);
    ASSERT_TRUE(translated.ok) << translated.error;
    std::string which;
    EXPECT_TRUE(CompilesOnBackend(translated, backend, &which))
        << which << " stage failed:\nVS:\n"
        << translated.vertex_glsl << "\nFS:\n"
        << translated.fragment_glsl;
  }
}

namespace
{
// Parses GLSL with glslang's front end only -- no SPIR-V target, no Vulkan rules. That is what the
// OpenGL/GLES backend actually does (it hands GLSL straight to the driver and never calls
// SPIRV::Compile* anywhere under VideoBackends/OGL/), and unlike SPIRV::Compile* it can be pointed
// below `#version 310 es`: glslang rejects an ES version under 310 outright when the target is
// SPIR-V, so the ES row below is only reachable this way.
bool ParsesAtVersion(const std::string& source, EShLanguage stage, int version, EProfile profile,
                     std::string* log)
{
  static const bool initialized = glslang::InitializeProcess();
  EXPECT_TRUE(initialized);
  glslang::TShader shader(stage);
  const char* str = source.c_str();
  shader.setStrings(&str, 1);
  glslang::TShader::ForbidIncluder includer;
  const bool ok = shader.parse(GetDefaultResources(), version, profile,
                               /*forceDefaultVersionAndProfile=*/false, /*forwardCompatible=*/false,
                               EShMsgDefault, includer);
  log->assign(shader.getInfoLog());
  return ok;
}

// The OpenGL backend's binding macros for a context with neither explicit binding layout nor
// ARB_shading_language_420pack -- the last `binding_layout` branch in ProgramShaderCache.cpp, where
// every binding macro expands to nothing. Everything after it mirrors that header's "silly
// differences" block.
constexpr const char* LOW_VERSION_MACROS = R"(
#define ATTRIBUTE_LOCATION(x)
#define FRAGMENT_OUTPUT_LOCATION(x)
#define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y)
#define UBO_BINDING(packing, x) layout(packing)
#define SAMPLER_BINDING(x)
#define TEXEL_BUFFER_BINDING(x)
#define SSBO_BINDING(x) layout(std430)
#define IMAGE_BINDING(format, x) layout(format)
#define VARYING_LOCATION(x)
#define API_OPENGL 1
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define frac fract
#define lerp mix
)";

struct LowVersionTarget
{
  const char* name;
  const char* preamble;
  int version;
  EProfile profile;
};

// Whole-word search, so `textureGather` is not found inside `textureGatherOffset` and, more to the
// point here, is not reported present merely because `texture` is.
bool ContainsWordInSource(const std::string& text, const std::string& word)
{
  const auto is_word_char = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
  };
  for (size_t pos = text.find(word); pos != std::string::npos; pos = text.find(word, pos + 1))
  {
    const size_t after = pos + word.size();
    if ((pos == 0 || !is_word_char(text[pos - 1])) &&
        (after >= text.size() || !is_word_char(text[after])))
    {
      return true;
    }
  }
  return false;
}

// GetGLSLVersionString() (ProgramShaderCache.cpp) can return 130, 140, 150 or 330 on desktop, and
// CreatePostProcessor() (AbstractGfx.cpp) applies no GLSL-version or capability gate, so a
// translated slang pass has to compile at 330. 410 is the control: it is above the textureGather
// floor, so it passes whether or not the shims are gated.
const LowVersionTarget DESKTOP_TARGETS[] = {
    {"330", "#version 330\n", 330, ECoreProfile},
    {"410", "#version 410 core\n", 410, ECoreProfile},
};

// The same three ES versions GetGLSLVersionString() and OGLConfig.cpp can select. ESSL 3.00 and up
// forbid overloading a built-in outright, which is why the shims are renamed helpers rather than
// overloads; these rows are what proves the rename actually buys the ES path.
const LowVersionTarget GLES_TARGETS[] = {
    {"300 es",
     "#version 300 es\nprecision highp float;\nprecision highp int;\n"
     "precision highp sampler2DArray;\n",
     300, EEsProfile},
    {"310 es",
     "#version 310 es\nprecision highp float;\nprecision highp int;\n"
     "precision highp sampler2DArray;\n",
     310, EEsProfile},
    {"320 es",
     "#version 320 es\nprecision highp float;\nprecision highp int;\n"
     "precision highp sampler2DArray;\n",
     320, EEsProfile},
};

// A pass that samples with `texture` and nothing else -- the overwhelming majority of the real
// libretro pack, and the shape that must not be made to pay for a builtin it never calls.
constexpr const char* PLAIN_TEXTURE_SHADER = R"(#version 450
layout(push_constant) uniform Push { vec4 SourceSize; } params;
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
void main() { gl_Position = global.MVP * Position; }
#pragma stage fragment
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
void main()
{
  FragColor = texture(Source, vec2(0.5)) + vec4(vec2(textureSize(Source, 0)), 0.0, 0.0);
}
)";
}  // namespace

// R1 regression guard. `textureGather` is core in GLSL 400 and in GLSL ES 310 (measured, both the
// 2- and the 3-argument form); below that on desktop it needs ARB_texture_gather plus
// ARB_gpu_shader5 *enabled*, not merely supported, and ES 3.00 has no form of it at
// all. Because a shim *calls* the builtin, emitting the shim unconditionally makes every translated
// pass depend on GLSL 400 -- including passes that never gather. Measured on a real driver (M2 Pro,
// GL 4.1): the unconditional shim block at `#version 330` gives five "No matching function for call
// to textureGather" errors and fails to compile, while the same source at `#version 410` is fine.
// The four BACKEND_HEADERS are all `#version 450`, so no other test in this file can see this.
TEST(SlangCompile, ShimsDoNotRaiseTheGlslVersionFloor)
{
  const auto translated = TranslateForBackend(PLAIN_TEXTURE_SHADER, BackendNamed("OpenGL"));
  ASSERT_TRUE(translated.ok) << translated.error;
  // The point of the gate: a shader that never gathers must not carry the gather shim.
  EXPECT_FALSE(ContainsWordInSource(translated.fragment_glsl, "textureGather"))
      << translated.fragment_glsl;

  for (const LowVersionTarget& target : DESKTOP_TARGETS)
  {
    SCOPED_TRACE(target.name);
    std::string log;
    EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                    translated.vertex_glsl,
                                EShLangVertex, target.version, target.profile, &log))
        << "vertex: " << log << "\n"
        << translated.vertex_glsl;
    EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                    translated.fragment_glsl,
                                EShLangFragment, target.version, target.profile, &log))
        << "fragment: " << log << "\n"
        << translated.fragment_glsl;
  }
}

// A shader that *does* gather still gets the shim, and still compiles wherever the built-in it
// wraps is available -- which, measured against glslang's own front end, is desktop 400+ and
// ES 3.10+, both the 2- and the 3-argument form. Gating must not be mistaken for dropping the
// feature, and neither must the version guard the helper carries.
TEST(SlangCompile, GatheringShadersStillGetTheGatherShim)
{
  const auto translated = TranslateForBackend(SAMPLER_SHADER, BackendNamed("OpenGL"));
  ASSERT_TRUE(translated.ok) << translated.error;
  EXPECT_TRUE(ContainsWordInSource(translated.fragment_glsl, "textureGather"))
      << translated.fragment_glsl;
  std::string log;
  EXPECT_TRUE(ParsesAtVersion("#version 410 core\n" + std::string(LOW_VERSION_MACROS) + "\n" +
                                  translated.fragment_glsl,
                              EShLangFragment, 410, ECoreProfile, &log))
      << log << "\n"
      << translated.fragment_glsl;
  EXPECT_TRUE(ParsesAtVersion(GLES_TARGETS[2].preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                  translated.fragment_glsl,
                              EShLangFragment, 320, EEsProfile, &log))
      << log << "\n"
      << translated.fragment_glsl;

  // And where the built-in does not exist, a shader that really gathers fails loudly rather than
  // sampling something wrong -- the helper is guarded away, so its call sites do not resolve. This
  // is the property that lets the guard be unconditional: the cost of guarding is paid only by
  // shaders that could not have run there anyway.
  EXPECT_FALSE(ParsesAtVersion(GLES_TARGETS[0].preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                   translated.fragment_glsl,
                               EShLangFragment, 300, EEsProfile, &log))
      << translated.fragment_glsl;
  EXPECT_NE(log.find("dolphin_textureGather"), std::string::npos) << log;
}

namespace
{
// The two shapes in the real pack that put `textureGather` in text the shader never compiles. The
// gate is a whole-word text match on source the translator has no preprocessor for, so both make it
// believe the stage gathers.
//
// `//`-commented, which is the whole of the `fsr` tree: edge-smoothing/fsr/shaders/ffx_fsr1.h:135-137
// is three commented-out lines, and fsr-pass0.slang:6 says "SM 4.0 compatible: no textureGather".
constexpr const char* COMMENTED_OUT_GATHER_SHADER = R"(#version 450
layout(push_constant) uniform Push { vec4 SourceSize; } params;
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
void main() { gl_Position = global.MVP * Position; }
#pragma stage fragment
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
// SM 4.0 compatible: no textureGather
//  vec4 EasuRH(vec2 p) { return textureGather(Source, p, 0); }
/* vec4 EasuGH(vec2 p) { return textureGather(Source, p, 1); }
   vec4 EasuBH(vec2 p) { return textureGather(Source, p, 2); } */
void main() { FragColor = texture(Source, vec2(0.5)); }
)";

// Behind a disabled `#if`, which is the nnedi3 `-predict-h-rgb` family:
// nnedi3-nns16-win8x4-predict-h-rgb.slang:4 is `#define NNEDI3_USE_GATHER 0`, so the only
// `textureGather` text left is in the NNEDI3_DEF_GATHER macro bodies of
// nnedi3-predict-common.inc:80-112, which nothing ever expands. Comment stripping cannot help here
// and neither can any gate short of a real preprocessor, so this is the shape that forces the
// helper to carry its own version guard.
constexpr const char* DISABLED_BRANCH_GATHER_SHADER = R"(#version 450
layout(push_constant) uniform Push { vec4 SourceSize; } params;
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
void main() { gl_Position = global.MVP * Position; }
#pragma stage fragment
#define NNEDI3_USE_GATHER 0
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
#if NNEDI3_USE_GATHER
#define NNEDI3_DEF_GATHER(t, c) textureGather(t, c, 0)
#else
#define NNEDI3_DEF_GATHER(t, c) texture(t, c)
#endif
void main() { FragColor = NNEDI3_DEF_GATHER(Source, vec2(0.5)); }
)";
}  // namespace

// R1, second time round. The gate is a text match on source that still contains comments and
// never-taken preprocessor branches, so it fires on shaders that cannot possibly gather -- 177 rows
// across 76 presets in the 2987-preset libretro pack, and for 7 stages across 5 presets (the nnedi3
// `-predict-h-rgb` ones) the manufactured shim was the *only* thing stopping the stage compiling at
// `#version 300 es`. Comment stripping fixes the first shape; only a self-guarding helper fixes the
// second, which is why the guard is what this test really pins.
TEST(SlangCompile, DeadGatherTextDoesNotRaiseTheGlslVersionFloor)
{
  const auto commented = TranslateForBackend(COMMENTED_OUT_GATHER_SHADER, BackendNamed("OpenGL"));
  ASSERT_TRUE(commented.ok) << commented.error;
  // A commented-out call is not a call: the gate sees comment-stripped source, so no shim at all.
  // Searched for as the helper's declaration rather than as a word, because the comments themselves
  // still say `textureGather` -- and say `dolphin_textureGather` too, since the rename reads no
  // context either. That is harmless in a comment, and it is what makes the gate imprecise.
  EXPECT_EQ(commented.fragment_glsl.find("dolphin_textureGather(sampler2DArray"), std::string::npos)
      << commented.fragment_glsl;

  const auto disabled =
      TranslateForBackend(DISABLED_BRANCH_GATHER_SHADER, BackendNamed("OpenGL"));
  ASSERT_TRUE(disabled.ok) << disabled.error;

  // Either shape has to compile at every version the OpenGL backend can select. Deliberately not
  // asserted: whether the shim is present for the second shape. Today it is (and its guard is what
  // saves it); a future gate with a real preprocessor would drop it instead, and both spellings
  // satisfy the property that matters here.
  for (const TranslatedPass* pass : {&commented, &disabled})
  {
    for (const LowVersionTarget& target : GLES_TARGETS)
    {
      SCOPED_TRACE(target.name);
      std::string log;
      EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                      pass->fragment_glsl,
                                  EShLangFragment, target.version, target.profile, &log))
          << log << "\n"
          << pass->fragment_glsl;
    }
    for (const LowVersionTarget& target : DESKTOP_TARGETS)
    {
      SCOPED_TRACE(target.name);
      std::string log;
      EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                      pass->fragment_glsl,
                                  EShLangFragment, target.version, target.profile, &log))
          << log << "\n"
          << pass->fragment_glsl;
    }
  }
}

// The GLES path, which the shims used to break outright. ESSL 3.00 forbids overloading a built-in:
// glslang enforces it in TParseContext::handleFunctionDeclarator, whose own comment reads "ES 300
// does not allow redefining or overloading of built-in functions" (ParseHelper.cpp:1173-1174), and
// the symbol insert then fails with "'texture' : function name is redeclaration of existing name".
// That bit the plain `texture` shim, which essentially every pack shader triggers, so before the
// shims became renamed helpers every translated pass failed to compile on 300/310/320 es no matter
// how tightly they were gated. A helper named dolphin_texture is a user function, and user
// functions may be overloaded on every target, so all three versions now accept the block.
//
// Reachable in the field: OGLConfig.cpp assigns GlslEs300/GlslEs310/GlslEs320,
// CreatePostProcessor() (AbstractGfx.cpp) applies no gate, and arrays.xml:204 lists OGL as a
// selectable Android backend.
TEST(SlangCompile, ShimsCompileOnGlesThreePointX)
{
  const auto translated = TranslateForBackend(PLAIN_TEXTURE_SHADER, BackendNamed("OpenGL"));
  ASSERT_TRUE(translated.ok) << translated.error;

  for (const LowVersionTarget& target : GLES_TARGETS)
  {
    SCOPED_TRACE(target.name);
    std::string log;
    EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                    translated.vertex_glsl,
                                EShLangVertex, target.version, target.profile, &log))
        << "vertex: " << log << "\n"
        << translated.vertex_glsl;
    EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                    translated.fragment_glsl,
                                EShLangFragment, target.version, target.profile, &log))
        << "fragment: " << log << "\n"
        << translated.fragment_glsl;
  }
}

namespace
{
// Both shapes the rename has to tell apart, in one pass. `COMPAT_TEXTURE` is the alias
// crt/shaders/hyllian/crt-hyllian-fast.slang defines (its call sites never spell `texture` at all),
// and the parameter named `texture` is crt-royale's bloom-functions.h, which declares
// `tex2DblurNfast(const sampler2D texture, ...)` and passes it on by name ten times.
constexpr const char* ALIASED_AND_SHADOWED_SHADER = R"(#version 450
layout(push_constant) uniform Push { vec4 SourceSize; } params;
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
void main() { gl_Position = global.MVP * Position; }
#pragma stage fragment
#define COMPAT_TEXTURE texture
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
vec4 blur(sampler2D texture, vec2 co)
{
  return COMPAT_TEXTURE(texture, co) + float(textureSize(texture, 0).x) * 0.0;
}
void main() { FragColor = blur(Source, vec2(0.5)); }
)";
}  // namespace

// The rename keys on the built-in being *used as a function*, and both halves of that rule are load
// bearing on real presets: rename too little and the alias expands to the built-in with a
// 2-component coordinate; rename too much and the sampler parameter named `texture` turns into a
// local that shadows the helper it is passed to.
TEST(SlangCompile, RenamesFunctionUsesOfBuiltinsAndLeavesObjectsAlone)
{
  const auto translated = TranslateForBackend(ALIASED_AND_SHADOWED_SHADER, BackendNamed("OpenGL"));
  ASSERT_TRUE(translated.ok) << translated.error;
  EXPECT_NE(translated.fragment_glsl.find("#define COMPAT_TEXTURE dolphin_texture"),
            std::string::npos)
      << "an object-like macro whose whole body is the built-in is a call by another name\n"
      << translated.fragment_glsl;
  EXPECT_NE(translated.fragment_glsl.find("vec4 blur(sampler2DArray texture, vec2 co)"),
            std::string::npos)
      << "a parameter named after the built-in is an object, not a call\n"
      << translated.fragment_glsl;

  for (const LowVersionTarget& target : GLES_TARGETS)
  {
    SCOPED_TRACE(target.name);
    std::string log;
    EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                    translated.fragment_glsl,
                                EShLangFragment, target.version, target.profile, &log))
        << log << "\n"
        << translated.fragment_glsl;
  }
  for (const LowVersionTarget& target : DESKTOP_TARGETS)
  {
    SCOPED_TRACE(target.name);
    std::string log;
    EXPECT_TRUE(ParsesAtVersion(target.preamble + std::string(LOW_VERSION_MACROS) + "\n" +
                                    translated.fragment_glsl,
                                EShLangFragment, target.version, target.profile, &log))
        << log << "\n"
        << translated.fragment_glsl;
  }
}

namespace
{
std::string ReadFile(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
std::string DirName(const std::string& p)
{
  const auto s = p.find_last_of("/\\");
  return s == std::string::npos ? "" : p.substr(0, s);
}
}  // namespace

// Compiles every pass of a REAL preset through the full pipeline (parse preset -> resolve pass
// paths -> expand #includes -> translate -> glslang). Guarded by SLANG_PRESET env var so it only
// runs when pointed at a real preset tree (e.g. the crt-royale pack pulled from the buildbot):
//   SLANG_PRESET=/path/to/crt/crt-royale.slangp ./Tests --gtest_filter=SlangCompile.RealPreset
TEST(SlangCompile, RealPresetCompilesAllPasses)
{
  const char* preset_path = std::getenv("SLANG_PRESET");
  if (preset_path == nullptr)
    GTEST_SKIP() << "set SLANG_PRESET=/path/to/foo.slangp to run";

  const std::string text = ReadFile(preset_path);
  ASSERT_FALSE(text.empty()) << "cannot read " << preset_path;
  std::string error;
  const auto preset = ParseSlangPreset(text, DirName(preset_path), &error);
  ASSERT_TRUE(preset.has_value()) << error;

  const SlangFileReader reader = [](const std::string& p, std::string* out) {
    const std::string c = ReadFile(p);
    if (c.empty())
      return false;
    out->assign(c);
    return true;
  };

  std::vector<std::string> lut_names;
  for (const auto& lut : preset->luts)
    lut_names.push_back(lut.name);

  std::array<int, std::size(BACKEND_HEADERS)> ok{};
  std::vector<std::string> known_aliases;
  for (size_t i = 0; i < preset->passes.size(); ++i)
  {
    const auto& pass = preset->passes[i];
    std::string src = ReadFile(pass.shader_path);
    ASSERT_FALSE(src.empty()) << "pass " << i << " unreadable: " << pass.shader_path;
    src = ExpandSlangIncludes(src, DirName(pass.shader_path), reader);
    const auto parsed = ParseSlangShader(src, &error);
    ASSERT_TRUE(parsed.has_value()) << "pass " << i << " parse: " << error;

    for (size_t b = 0; b < std::size(BACKEND_HEADERS); ++b)
    {
      const BackendShaderHeader& backend = BACKEND_HEADERS[b];
      SCOPED_TRACE(backend.name);
      const auto translated =
          TranslateSlangPass(*parsed, known_aliases, lut_names, SlangNeedsClipYFlip(backend.api_type));
      ASSERT_TRUE(translated.ok) << "pass " << i << " translate: " << translated.error;

      std::string which;
      const bool compiled = CompilesOnBackend(translated, backend, &which);
      EXPECT_TRUE(compiled) << "pass " << i << " (" << pass.shader_path << ") " << which
                            << " stage failed to compile";
      if (compiled)
        ++ok[b];
    }
    if (!pass.alias.empty())
      known_aliases.push_back(pass.alias);
  }
  for (size_t b = 0; b < std::size(BACKEND_HEADERS); ++b)
  {
    std::printf("RealPreset[%s]: %d/%zu passes compiled\n", BACKEND_HEADERS[b].name, ok[b],
                preset->passes.size());
  }
}
