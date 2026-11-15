// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <gtest/gtest.h>

#include "Common/ScopeGuard.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/Spirv.h"
#include "VideoCommon/VideoConfig.h"

TEST(FramebufferShaderGen, ImGuiColorSpacesCompile)
{
  const auto previous_backend_info = g_backend_info;
  const Common::ScopeGuard restore_backend_info([&] { g_backend_info = previous_backend_info; });
  constexpr const char* header = R"(#version 450 core
#define SAMPLER_BINDING(x) layout(binding=x)
#define VARYING_LOCATION(x) layout(location=x)
#define FRAGMENT_OUTPUT_LOCATION(x) layout(location=x)
#define float2 vec2
#define float3 vec3
#define float4 vec4
)";

  // Cover both varying layouts and every shader compiler's SDR/HDR variant, including
  // the vector sRGB transfer function used for textures and tints on linear backbuffers.
  for (const auto api : {APIType::OpenGL, APIType::D3D, APIType::Vulkan, APIType::Metal})
  {
    for (const bool geometry_shaders : {false, true})
    {
      for (const bool linear_output : {false, true})
      {
        SCOPED_TRACE(testing::Message() << "API=" << static_cast<int>(api) << " geometry="
                                        << geometry_shaders << " linear=" << linear_output);
        g_backend_info.api_type = api;
        g_backend_info.bSupportsGeometryShaders = geometry_shaders;
        const auto source =
            std::string(header) + FramebufferShaderGen::GenerateImGuiPixelShader(linear_output);
        const auto version =
            api == APIType::Metal ? glslang::EShTargetSpv_1_5 : glslang::EShTargetSpv_1_0;
        const auto shader = SPIRV::CompileFragmentShader(source, api, version, nullptr);
        ASSERT_TRUE(shader.has_value());
        EXPECT_FALSE(shader->empty());
      }
    }
  }
}
