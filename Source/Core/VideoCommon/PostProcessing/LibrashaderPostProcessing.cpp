// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/LibrashaderPostProcessing.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <utility>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/PostProcessing/ChainDebugDump.h"
#include "VideoCommon/PostProcessing/ChainOutputPolicy.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"
#include "VideoCommon/PostProcessing/LibrashaderParameters.h"
#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"
#include "VideoCommon/PostProcessing/LibrashaderUtilityShader.h"
#include "VideoCommon/PostProcessing/PostProcessingConfig.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
namespace
{
struct alignas(16) SourceRectUniforms
{
  std::array<s32, 4> source_rect;
  std::array<s32, 4> source_layer;
};
static_assert(sizeof(SourceRectUniforms) == 32);

void UploadSourceRectUniforms(const AbstractTexture* texture,
                              const MathUtil::Rectangle<int>& source_rect, int source_layer)
{
  const MathUtil::Rectangle<int> converted = g_gfx->ConvertFramebufferRectangle(
      source_rect, texture->GetWidth(), texture->GetHeight());
  const SourceRectUniforms uniforms{
      {converted.left, converted.top, converted.GetWidth(), converted.GetHeight()},
      {std::max(source_layer, 0), 0, 0, 0}};
  g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));
}
}  // namespace

std::string ResolvePresetPath(const std::string& preset_spec)
{
  const std::string name = ResolveConfiguredPreset(preset_spec);
  if (name.empty())
    return {};

  std::string path = File::GetUserPath(D_SHADERS_IDX) + "shaders_slang" DIR_SEP + name + ".slangp";
  if (!File::Exists(path))
    path = File::GetUserPath(D_SHADERS_IDX) + name + ".slangp";
  if (!File::Exists(path))
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP "shaders_slang" DIR_SEP + name + ".slangp";
  if (!File::Exists(path))
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + name + ".slangp";
  if (!File::Exists(path))
    return {};
  return path;
}

LibrashaderPostProcessing::LibrashaderPostProcessing(std::unique_ptr<LibrashaderRuntime> runtime)
    : m_runtime(std::move(runtime))
{
}

LibrashaderPostProcessing::~LibrashaderPostProcessing()
{
  // Free the chain here rather than relying on every runtime's destructor to remember. The runtime
  // object is still fully alive in this body, so the virtual call dispatches normally; DestroyChain
  // is required to be idempotent for exactly this reason.
  m_runtime->DestroyChain();
}

bool LibrashaderPostProcessing::Initialize(AbstractTextureFormat format)
{
  // `format` is deliberately unused: see the note by m_frame_count in the header. Every format
  // decision is taken per frame from the live framebuffer, which is what survives an HDR toggle.
  m_available = m_runtime->IsSupported();
  if (!m_available)
    return false;

  RecompileShader();
  return true;
}

void LibrashaderPostProcessing::RecompileShader()
{
  // Free any previous chain before rebuilding.
  m_runtime->DestroyChain();
  m_frame_count = 0;
  m_parameters.clear();
  m_preset_relative_path.clear();

  if (!m_available)
    return;

  const std::string preset_name = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  const std::string path = ResolvePresetPath(preset_name);
  if (path.empty())
  {
    if (!preset_name.empty())
    {
      WARN_LOG_FMT(VIDEO, "Librashader: preset '{}' not found; falling back to passthrough",
                   preset_name);
    }
    return;
  }

  // Derive the preset-relative path for override storage. This is the key under
  // [LibrashaderParameters], matching PCSX2's shape: preset path relative to the shaders root. The
  // parameters dialog derives it from this same function, so the two cannot disagree about where a
  // preset's overrides live.
  m_preset_relative_path = LibrashaderParameters::KeyForPreset(path, preset_name);

  // Enumerate the preset's parameters once here at chain construction and cache them. A preset's
  // parameter list cannot change without the chain being rebuilt, so this needs no per-frame work.
  // A generation change then costs one Config::GetAsString, one ParseOverrides, and N SetParameter
  // calls -- no disk access and no preset parse.
  std::string enum_error;
  if (!LibrashaderParameters::Enumerate(path, &m_parameters, &enum_error))
  {
    WARN_LOG_FMT(VIDEO, "Librashader: failed to enumerate parameters for '{}': {}", path,
                 enum_error);
    // Non-fatal: the chain can still run; the dialog will just have nothing to show.
  }

  // Tested against the error handle, not against its description: an error whose message renders
  // empty would otherwise be read as success, and CreateChain would then be handed a null preset.
  libra_shader_preset_t preset = nullptr;
  if (const libra_error_t error = Librashader::Common().preset_create(path.c_str(), &preset))
  {
    ERROR_LOG_FMT(VIDEO, "Librashader: preset_create('{}') failed: {}", path,
                  Librashader::DescribeAndFreeError(error));
    return;
  }

  // CreateChain consumes `preset` on every path, so there is nothing to free here.
  if (m_runtime->CreateChain(preset))
  {
    INFO_LOG_FMT(VIDEO, "Librashader: filter chain created from '{}'", path);

    // Apply stored overrides once at chain creation. The dialog pushes EVERY parameter when the
    // generation changes, not only the overridden ones, because a freshly built chain starts from
    // the preset defaults but a live chain remembers the last value it was given -- so resetting a
    // parameter has to send the default explicitly.
    ApplyStoredOverrides();
  }
}

void LibrashaderPostProcessing::RecompilePipeline()
{
  // librashader owns its internal pipelines and rebuilds them as part of the filter chain, so there
  // is nothing backend-pipeline-specific to rebuild here. The passthrough pipeline is (re)built
  // lazily in BlitFromTexture when the framebuffer format changes.
}

void LibrashaderPostProcessing::ApplyStoredOverrides()
{
  if (!m_runtime->HasChain() || m_parameters.empty())
    return;

  // Load the stored overrides for this preset.
  const LibrashaderParameters::Overrides overrides =
      LibrashaderParameters::Load(m_preset_relative_path);

  // Build a map of overridden values for fast lookup.
  std::map<std::string, float> override_map;
  for (const auto& [name, value] : overrides)
    override_map[name] = value;

  // The dialog pushes EVERY parameter, not only the overridden ones. A freshly built chain starts
  // from the preset defaults, but a live chain remembers the last value it was given -- so
  // resetting a parameter has to send the default explicitly.
  for (const auto& param : m_parameters)
  {
    const auto it = override_map.find(param.name);
    const float value = (it != override_map.end()) ? it->second : param.initial;
    m_runtime->SetParameter(param.name.c_str(), value);
  }

  // Record that we've seen the current generation, so the poll in BlitFromTexture doesn't
  // immediately reapply on the next frame.
  m_last_seen_generation = LibrashaderParameters::CurrentGeneration();
}

void LibrashaderPostProcessing::BuildPassthroughPipeline()
{
  AbstractFramebuffer* const framebuffer = g_gfx->GetCurrentFramebuffer();
  if (framebuffer == nullptr)
    return;
  const AbstractTextureFormat format = framebuffer->GetColorFormat();
  if (m_passthrough_pipeline && m_passthrough_format == format)
    return;

  // Fullscreen-triangle copy, identical to MultipassPostProcessing's passthrough -- including the
  // conditional flip, which is only correct on the backends whose clip space is Y-down. Hardcoding
  // it went unnoticed while Vulkan was the only backend here; on D3D it inverts the whole frame.
  // This draw targets the presented framebuffer, so it asks SlangNeedsPresentClipYFlip and not
  // SlangNeedsClipYFlip: on OpenGL the latter is the answer for a texture target only.
  const std::string vertex_source = GenerateLibrashaderFullscreenVertexShader(
      SlangNeedsPresentClipYFlip(g_backend_info.api_type));
  const std::string pixel_source = GenerateLibrashaderPassthroughPixelShader();

  m_passthrough_vertex = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, vertex_source, nullptr,
                                                       "librashader passthrough vertex");
  m_passthrough_pixel = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pixel_source, nullptr,
                                                      "librashader passthrough pixel");
  m_passthrough_pipeline.reset();
  if (!m_passthrough_vertex || !m_passthrough_pixel)
    return;

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_passthrough_vertex.get();
  config.pixel_shader = m_passthrough_pixel.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(format);
  config.usage = AbstractPipelineUsage::Utility;
  m_passthrough_pipeline = g_gfx->CreatePipeline(config);
  m_passthrough_format = format;
}

void LibrashaderPostProcessing::BuildDownscalePipeline(const SlangSourceDownscalePlan& plan,
                                                       AbstractTextureFormat format)
{
  const u32 factor = plan.box_filter ? plan.factor : 0;
  if (m_downscale_pipeline && m_downscale_is_box == plan.box_filter &&
      m_downscale_factor == factor && m_downscale_format == format)
  {
    return;
  }

  // Fullscreen triangle. On the Y-down-clip-space backends the flip makes the bilinear path's
  // v_tex0 align with gl_FragCoord's top-left origin. The box shader reads gl_FragCoord directly
  // and has no texture-coordinate input, so do not export v_tex0 for it: a D3D12 driver can reject
  // that otherwise-unused stage output while creating the pipeline state.
  const std::string vertex_source = GenerateLibrashaderFullscreenVertexShader(
      SlangNeedsClipYFlip(g_backend_info.api_type), !plan.box_filter);
  const std::string pixel_source = GenerateLibrashaderSourceNormalizationPixelShader(plan);

  m_downscale_vertex = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, vertex_source, nullptr,
                                                     "librashader downscale vertex");
  m_downscale_pixel = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pixel_source, nullptr,
                                                    "librashader downscale pixel");
  m_downscale_pipeline.reset();
  if (!m_downscale_vertex || !m_downscale_pixel)
    return;

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_downscale_vertex.get();
  config.pixel_shader = m_downscale_pixel.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(format);
  config.usage = AbstractPipelineUsage::Utility;
  m_downscale_pipeline = g_gfx->CreatePipeline(config);

  m_downscale_is_box = plan.box_filter;
  m_downscale_factor = factor;
  m_downscale_format = format;
}

AbstractTexture*
LibrashaderPostProcessing::DownscaleToNativeSource(const SlangSourceDownscalePlan& plan,
                                                   const AbstractTexture* src_tex,
                                                   const MathUtil::Rectangle<int>& src,
                                                   int src_layer, u32 native_width,
                                                   u32 native_height)
{
  const AbstractTextureFormat format = src_tex->GetFormat();

  // (Re)allocate the native-res render target whenever its size or format changes.
  if (!m_native_source || m_native_source_width != native_width ||
      m_native_source_height != native_height || m_native_source_format != format)
  {
    m_native_source_fb.reset();
    m_native_source.reset();
    m_native_source_width = 0;
    m_native_source_height = 0;
    m_native_source_format = AbstractTextureFormat::Undefined;
    const TextureConfig config(native_width, native_height, 1, 1, 1, format,
                               AbstractTextureFlag_RenderTarget,
                               AbstractTextureType::Texture_2DArray);
    m_native_source = g_gfx->CreateTexture(config, "librashader native source");
    if (m_native_source)
      m_native_source_fb = g_gfx->CreateFramebuffer(m_native_source.get(), nullptr);
    // Record the dimensions only once BOTH objects exist. Recording them unconditionally meant a
    // CreateTexture that succeeded followed by a CreateFramebuffer that failed left the recreate
    // condition permanently unsatisfied, so this returned nullptr for that size for the rest of the
    // session instead of retrying on the next frame.
    if (m_native_source && m_native_source_fb)
    {
      m_native_source_width = native_width;
      m_native_source_height = native_height;
      m_native_source_format = format;
    }
  }
  if (!m_native_source || !m_native_source_fb)
    return nullptr;

  BuildDownscalePipeline(plan, format);
  if (!m_downscale_pipeline)
    return nullptr;

  // We overwrite every native texel, so discard the prior contents. Box averaging reads exact
  // texels (point sampler); the bilinear fallback needs a linear sampler.
  g_gfx->SetAndDiscardFramebuffer(m_native_source_fb.get());
  UploadSourceRectUniforms(src_tex, src, src_layer);
  g_gfx->SetTexture(0, src_tex);
  g_gfx->SetSamplerState(0, plan.box_filter ? RenderState::GetPointSamplerState() :
                                              RenderState::GetLinearSamplerState());
  g_gfx->SetViewportAndScissor(
      g_gfx->ConvertFramebufferRectangle(m_native_source->GetRect(), m_native_source_fb.get()));
  g_gfx->SetPipeline(m_downscale_pipeline.get());
  g_gfx->Draw(0, 3);

  // The result is handed to librashader as a shader-read source, but nothing is done about that
  // here: the caller's g_gfx->SetFramebuffer(framebuffer) restore ends the render pass this draw
  // opened, and RunFrame() transitions whichever source it is given, so the shader-read transition
  // is deferred to it rather than issued twice.
  return m_native_source.get();
}

AbstractFramebuffer* LibrashaderPostProcessing::EnsureOutputTarget(u32 width, u32 height,
                                                                   AbstractTextureFormat format)
{
  // No dimension may be zero: a texture that size is not allocatable. The built-in executor clamps
  // its own pass sizes the same way (MultipassPostProcessing::RecompilePipeline). No caller can
  // currently reach this with a zero extent, so it is hardening and adds no new failure path.
  width = std::max<u32>(1, width);
  height = std::max<u32>(1, height);

  if (!m_output_target || m_output_target_width != width || m_output_target_height != height ||
      m_output_target_format != format)
  {
    m_output_target_fb.reset();
    m_output_target.reset();
    m_output_target_width = 0;
    m_output_target_height = 0;
    m_output_target_format = AbstractTextureFormat::Undefined;
    const TextureConfig config(width, height, 1, 1, 1, format, AbstractTextureFlag_RenderTarget,
                               AbstractTextureType::Texture_2DArray);
    m_output_target = g_gfx->CreateTexture(config, "librashader chain output");
    if (m_output_target)
      m_output_target_fb = g_gfx->CreateFramebuffer(m_output_target.get(), nullptr);
    // Record the dimensions only once BOTH objects exist -- see DownscaleToNativeSource for why.
    if (m_output_target && m_output_target_fb)
    {
      m_output_target_width = width;
      m_output_target_height = height;
      m_output_target_format = format;
    }
  }
  return m_output_target_fb.get();
}

void LibrashaderPostProcessing::BlitFromTexture(const MathUtil::Rectangle<int>& dst,
                                                const MathUtil::Rectangle<int>& src,
                                                const AbstractTexture* src_tex, int src_layer,
                                                u32 native_width, u32 native_height)
{
  AbstractFramebuffer* const framebuffer = g_gfx->GetCurrentFramebuffer();
  if (framebuffer == nullptr)
    return;

  // Poll the generation counter for parameter edits. Save() bumps it after writing, so the dialog
  // doesn't have to signal manually. On a mismatch, reload and reapply all parameters.
  const u32 current_gen = LibrashaderParameters::CurrentGeneration();
  if (m_runtime->HasChain() && current_gen != m_last_seen_generation)
  {
    ApplyStoredOverrides();
  }

  // Drive the real librashader filter chain when it was created successfully. If there is no chain
  // (no preset, preset failed, or a required device extension is missing) we skip straight to the
  // passthrough copy so the screen never blanks.
  if (m_runtime->HasChain())
  {
    // librashader derives SourceSize/OriginalSize from the input image's dimensions, and CRT
    // presets (crt-royale, RetroCrisis) scale their scanline and phosphor-mask geometry by
    // SourceSize. The XFB source is at the internal (upscaled) resolution, so feeding it directly
    // reports SourceSize = internal res: the mask/scanline period shrinks with the IR multiplier
    // (moire, invisible scanlines) and every pass runs against the oversized frame. Worse,
    // librashader's single bilinear tap subsamples that upscaled frame, aliasing high-frequency
    // content into the NTSC/scanline bands. We instead materialize a REAL native-resolution source
    // by box-averaging the whole footprint (SSAA) for integer upscales and bilinear sampling for
    // every other case. The normalization also applies the selected crop rectangle and texture
    // layer, including at 1x, matching PCSX2's shader-chain input contract.
    const SlangSourceDownscalePlan plan = PlanSlangSourceDownscale(
        static_cast<u32>(src.GetWidth()), static_cast<u32>(src.GetHeight()), native_width,
        native_height);
    const AbstractTexture* source = src_tex;
    bool source_ready = !plan.normalize;
    if (plan.normalize)
    {
      if (const AbstractTexture* native =
              DownscaleToNativeSource(plan, src_tex, src, src_layer, native_width, native_height))
      {
        source = native;
        source_ready = true;
      }
      // The downscale draw left its own framebuffer bound; restore the caller's so that a
      // fall-through to the passthrough copy (on chain error) targets the screen, not the native
      // RT. This restore is also what ends the render pass the downscale opened, which is why
      // DownscaleToNativeSource does not end one itself -- do not remove or move it out of this
      // branch without putting that back, or the chain's barriers get recorded inside that pass.
      g_gfx->SetFramebuffer(framebuffer);
    }

    // UAT instrument for finding 3: dump chain input and output from one frame, once per run.
    const bool dump_images = source_ready && ShouldDumpChainImages();
    if (dump_images)
      DumpChainImage(source, "chain-input");

    // librashader derives OutputSize, FinalViewportSize, and every scale_type=viewport framebuffer
    // size from the OUTPUT IMAGE's dimensions, then merely scissors rendering to the viewport rect.
    // Handing it the full backbuffer plus a pillarboxed sub-rect would size the whole chain
    // (phosphor mask, scanline geometry, NTSC subcarrier) for the backbuffer width while the pixels
    // land in the narrower draw rect -- a fixed fractional mismatch that beats against the panel
    // pixel grid as vertical moire, independent of internal resolution. We instead render the chain
    // into a draw-rect-sized target at viewport origin (0,0) so OutputSize == the drawn extent,
    // then blit that 1:1 into the backbuffer at the draw rect. This mirrors how ARMSX2 drives
    // librashader.
    if (source_ready &&
        ShouldRenderChainDirectly(dst, framebuffer->GetWidth(), framebuffer->GetHeight(),
                                  framebuffer->GetColorAttachment() != nullptr))
    {
      // The draw rect is the entire backbuffer, so OutputSize is identical whether the chain
      // targets the intermediate texture or the backbuffer itself: render straight into the
      // backbuffer and skip the extra full-frame write + read of the 1:1 blit. The chain's final
      // pass covers every backbuffer pixel, which also makes the clear BindBackbuffer deferred
      // redundant.
      m_runtime->DiscardPendingTargetClear();
      // The counter advances only on success (both here and on the intermediate path below). A
      // post-increment in the argument list advanced it for a frame the chain never rendered, so a
      // preset keyed on FrameCount -- an interlacer, an animated NTSC phase -- saw a gap in the
      // sequence for every failed frame.
      if (m_runtime->RunFrame(source, framebuffer, m_frame_count))
      {
        ++m_frame_count;
        if (dump_images)
        {
          // UAT instrument: the output is the backbuffer, which cannot be read back reliably. A
          // windowed run (where ShouldRenderChainDirectly returns false) produces both images.
          INFO_LOG_FMT(VIDEO, "Librashader: chain-output dump skipped (direct-to-backbuffer path); "
                              "run windowed to force the intermediate texture path");
          NoteChainImagesDumped();
        }
        return;
      }
      // On error, fall through to the passthrough copy; it covers the full rect, so the discarded
      // clear is not missed.
    }
    else if (source_ready)
    {
      // GetColorFormat() rather than the attachment's own format: they agree whenever there is an
      // attachment, and OpenGL's window framebuffer has none but still reports the format it
      // presents (RGBA8), which is the format the 1:1 blit below has to match.
      AbstractFramebuffer* const chain_fb =
          EnsureOutputTarget(static_cast<u32>(dst.GetWidth()), static_cast<u32>(dst.GetHeight()),
                             framebuffer->GetColorFormat());
      if (chain_fb != nullptr && m_runtime->RunFrame(source, chain_fb, m_frame_count))
      {
        ++m_frame_count;
        AbstractTexture* const chain_output = chain_fb->GetColorAttachment();
        if (dump_images)
        {
          // UAT instrument: dump the chain output before presenting it to the backbuffer.
          DumpChainImage(chain_output, "chain-output");
          NoteChainImagesDumped();
        }

        // Present the chain output 1:1 into the backbuffer draw rect. Point sampling keeps the copy
        // exact (target and rect are equal size). SetTexture takes the chain output out of the
        // render-target state RunFrame left it in.
        BuildPassthroughPipeline();
        if (m_passthrough_pipeline)
        {
          g_gfx->SetFramebuffer(framebuffer);
          UploadSourceRectUniforms(chain_output, chain_output->GetRect(), 0);
          g_gfx->SetTexture(0, chain_output);
          g_gfx->SetSamplerState(0, RenderState::GetPointSamplerState());
          g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(dst, framebuffer));
          g_gfx->SetPipeline(m_passthrough_pipeline.get());
          g_gfx->Draw(0, 3);
        }
        return;
      }
      // On error, fall through to the passthrough copy for this frame so the screen never blanks.
    }
  }

  BuildPassthroughPipeline();
  if (!m_passthrough_pipeline)
    return;

  UploadSourceRectUniforms(src_tex, src, src_layer);
  g_gfx->SetTexture(0, src_tex);
  g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(dst, framebuffer));
  g_gfx->SetPipeline(m_passthrough_pipeline.get());
  g_gfx->Draw(0, 3);
}
}  // namespace VideoCommon
