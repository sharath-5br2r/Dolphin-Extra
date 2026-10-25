// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/LibrashaderUtilityShader.h"

namespace VideoCommon
{
std::string GenerateLibrashaderFullscreenVertexShader(bool flip_y, bool emit_texcoord)
{
  std::string source;
  if (emit_texcoord)
    source += "VARYING_LOCATION(0) out float2 v_tex0;\n";
  source += "void main() {\n";
  if (emit_texcoord)
    source += "  v_tex0 = float2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n";
  else
    source += "  float2 texcoord = float2(float((gl_VertexID << 1) & 2), "
              "float(gl_VertexID & 2));\n";
  source += emit_texcoord ?
                "  gl_Position = float4(v_tex0 * float2(2.0, -2.0) + "
                "float2(-1.0, 1.0), 0.0, 1.0);\n" :
                "  gl_Position = float4(texcoord * float2(2.0, -2.0) + "
                "float2(-1.0, 1.0), 0.0, 1.0);\n";
  if (flip_y)
    source += "  gl_Position.y = -gl_Position.y;\n";
  source += "}\n";
  return source;
}

std::string GenerateLibrashaderPassthroughPixelShader()
{
  return "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
         "UBO_BINDING(std140, 1) uniform PSBlock {\n"
         "  int4 source_rect;\n"
         "  int source_layer;\n"
         "};\n"
         "VARYING_LOCATION(0) in float2 v_tex0;\n"
         "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n"
         "void main() {\n"
         "  float2 size = float2(textureSize(samp0, 0).xy);\n"
         "  float2 uv = (float2(source_rect.xy) +\n"
         "               v_tex0 * float2(source_rect.zw)) / size;\n"
         "  ocol0 = texture(samp0, float3(uv, float(source_layer)));\n"
         "}\n";
}

std::string
GenerateLibrashaderSourceNormalizationPixelShader(const SlangSourceDownscalePlan& plan)
{
  if (!plan.box_filter)
    return GenerateLibrashaderPassthroughPixelShader();

  const std::string factor = std::to_string(plan.factor);
  const std::string sample_count = std::to_string(plan.factor * plan.factor);
  return "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
         "UBO_BINDING(std140, 1) uniform PSBlock {\n"
         "  int4 source_rect;\n"
         "  int source_layer;\n"
         "};\n"
         "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n"
         "void main() {\n"
         "  int2 base = source_rect.xy + int2(gl_FragCoord.xy) * " +
         factor +
         ";\n"
         "  float4 sum = float4(0.0, 0.0, 0.0, 0.0);\n"
         "  for (int y = 0; y < " +
         factor +
         "; ++y)\n"
         "    for (int x = 0; x < " +
         factor +
         "; ++x)\n"
         "      sum += texelFetch(samp0,\n"
         "                        int3(base + int2(x, y), source_layer), 0);\n"
         "  ocol0 = sum * (1.0 / " +
         sample_count +
         ".0);\n"
         "}\n";
}
}  // namespace VideoCommon
