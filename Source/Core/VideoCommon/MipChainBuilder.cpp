// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/MipChainBuilder.h"

#include <array>
#include <string>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/PostProcessing/MipGen.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
MipChainBuilder::MipChainBuilder() = default;
MipChainBuilder::~MipChainBuilder() = default;

bool MipChainBuilder::EnsurePipeline(AbstractTextureFormat format)
{
  if (m_pipeline && m_pipeline_format == format)
    return true;

  // Same fullscreen-triangle idiom as MultipassPostProcessing::BuildPassthroughPipeline, so it
  // works for any render-target format (ScaleTexture is RGBA8-only).
  const std::string flip_y = SlangNeedsClipYFlip(g_backend_info.api_type) ?
                                 "  gl_Position.y = -gl_Position.y;\n" :
                                 "";
  const std::string vertex_source =
      "VARYING_LOCATION(0) out float2 v_tex0;\n"
      "void main() {\n"
      "  v_tex0 = float2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
      "  gl_Position = float4(v_tex0 * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n" +
      flip_y + "}\n";
  // Layer 0 only -- see the array-layer precondition on MipChainBuilder::Generate.
  const char* const pixel_source =
      "UBO_BINDING(std140, 1) uniform PSBlock { float4 src_lod; };\n"
      "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
      "VARYING_LOCATION(0) in float2 v_tex0;\n"
      "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n"
      "void main() {\n"
      "  ocol0 = textureLod(samp0, float3(v_tex0, 0.0), src_lod.x);\n"
      "}\n";

  m_vertex_shader = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, vertex_source, nullptr,
                                                  "mip chain vertex");
  m_pixel_shader =
      g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pixel_source, nullptr, "mip chain pixel");
  m_pipeline.reset();
  if (!m_vertex_shader || !m_pixel_shader)
    return false;

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_vertex_shader.get();
  config.pixel_shader = m_pixel_shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(format);
  config.usage = AbstractPipelineUsage::Utility;
  m_pipeline = g_gfx->CreatePipeline(config);
  m_pipeline_format = format;
  return m_pipeline != nullptr;
}

bool MipChainBuilder::EnsureScratch(u32 width, u32 height, AbstractTextureFormat format)
{
  if (m_scratch && m_scratch->GetWidth() >= width && m_scratch->GetHeight() >= height &&
      m_scratch->GetFormat() == format)
  {
    return true;
  }

  m_scratch_framebuffer.reset();
  const TextureConfig config(width, height, 1, 1, 1, format, AbstractTextureFlag_RenderTarget,
                             AbstractTextureType::Texture_2DArray);
  m_scratch = g_gfx->CreateTexture(config, "mip chain scratch");
  if (!m_scratch)
    return false;
  m_scratch_framebuffer = g_gfx->CreateFramebuffer(m_scratch.get(), nullptr);
  return m_scratch_framebuffer != nullptr;
}

bool MipChainBuilder::Generate(AbstractTexture* texture)
{
  const u32 levels = texture->GetLevels();
  if (levels <= 1)
    return true;
  if (m_failed)
    return false;

  DEBUG_ASSERT_MSG(VIDEO, texture->GetLayers() == 1,
                   "MipChainBuilder only fills array layer 0, but was given a {}-layer texture",
                   texture->GetLayers());

  const AbstractTextureFormat format = texture->GetFormat();
  if (!EnsurePipeline(format) ||
      !EnsureScratch(MipLevelSize(texture->GetWidth(), 1), MipLevelSize(texture->GetHeight(), 1),
                     format))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create mip chain builder resources; mipmapped post-processing "
                         "passes will sample an incomplete chain.");
    m_failed = true;
    return false;
  }

  // No Begin/EndUtilityDrawing() here: every caller is already inside the present path's utility
  // scope (see the precondition in the header). Opening a nested scope is not harmless -- the inner
  // EndUtilityDrawing() rebinds the EFB and restores the stored viewport mid-present.
  for (u32 level = 1; level < levels; ++level)
  {
    const u32 width = MipLevelSize(texture->GetWidth(), level);
    const u32 height = MipLevelSize(texture->GetHeight(), level);
    const MathUtil::Rectangle<int> rect(0, 0, static_cast<int>(width), static_cast<int>(height));

    // Read the level we just produced; it must be sampleable before we bind it.
    texture->FinishedRendering();

    const std::array<float, 4> uniforms = {static_cast<float>(level - 1), 0.0f, 0.0f, 0.0f};
    g_vertex_manager->UploadUtilityUniforms(uniforms.data(), sizeof(uniforms));

    // SetAndDiscardFramebuffer, not SetFramebuffer: this is the same framebuffer object every
    // iteration, and D3D12's SetFramebuffer early-returns on an unchanged pointer without setting
    // DirtyState_Framebuffer -- so the transition back to RENDER_TARGET (only BindFramebuffer does
    // it) would be skipped for level >= 2, after FinishedRendering() and the copy below have moved
    // the scratch to PIXEL_SHADER_RESOURCE. SetAndDiscardFramebuffer transitions unconditionally,
    // and discarding is correct here because the draw overwrites the whole rect.
    g_gfx->SetAndDiscardFramebuffer(m_scratch_framebuffer.get());
    // Deliberately not ConvertFramebufferRectangle(): the CopyRectangleFromTexture() below reads
    // this same rect straight out of the scratch texture, and texture copies address texels
    // identically on every backend. Converting only the viewport would push the draw to the far end
    // of the (level-1-sized, hence oversized for level >= 2) scratch on lower-left-origin backends
    // while the copy kept reading rows 0..height-1. The vertex shader's SlangNeedsClipYFlip already
    // makes source texel row 0 land on scratch texel row 0 on every backend.
    g_gfx->SetViewportAndScissor(rect);
    g_gfx->SetPipeline(m_pipeline.get());
    g_gfx->SetTexture(0, texture);
    g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
    g_gfx->Draw(0, 3);

    m_scratch->FinishedRendering();
    texture->CopyRectangleFromTexture(m_scratch.get(), rect, 0, 0, rect, 0, level);
  }
  texture->FinishedRendering();
  return true;
}
}  // namespace VideoCommon
