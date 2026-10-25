// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/PostProcessing/LutTexture.h"
#include "VideoCommon/PostProcessing/MipGen.h"
#include "VideoCommon/PostProcessing/PassGraph.h"
#include "VideoCommon/PostProcessing/PassSizing.h"
#include "VideoCommon/PostProcessing/PostProcessingConfig.h"
#include "VideoCommon/PostProcessing/RetroCrisisInstall.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"
#include "VideoCommon/PostProcessing/SlangSamplers.h"
#include "VideoCommon/PostProcessing/SlangShader.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
namespace
{
constexpr AbstractTextureFormat INTERMEDIATE_FORMAT = AbstractTextureFormat::RGBA16F;

std::string DirectoryOf(const std::string& path)
{
  const auto slash = path.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

// Identity MVP: the shader multiplies it by the synthesized fullscreen-triangle Position.
std::array<float, 16> IdentityMvp()
{
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
}  // namespace

MultipassPostProcessing::MultipassPostProcessing() = default;
MultipassPostProcessing::~MultipassPostProcessing() = default;

std::string PresetNameFromPath(const std::string& full_path,
                               const std::vector<std::string>& roots)
{
  std::string path = full_path;
  std::replace(path.begin(), path.end(), '\\', '/');

  std::string best;  // longest matching root's relative remainder
  for (const std::string& raw_root : roots)
  {
    std::string root = raw_root;
    std::replace(root.begin(), root.end(), '\\', '/');
    if (!root.empty() && root.back() != '/')
      root += '/';
    if (path.size() > root.size() && path.compare(0, root.size(), root) == 0)
    {
      const std::string remainder = path.substr(root.size());
      if (best.empty() || remainder.size() < best.size())
        best = remainder;
    }
  }

  std::string relative = best.empty() ? path : best;

  // Strip a leading slash and the ".slangp" extension.
  if (best.empty())
  {
    // No root matched: fall back to the bare filename.
    const auto slash = relative.find_last_of('/');
    if (slash != std::string::npos)
      relative = relative.substr(slash + 1);
  }
  constexpr std::string_view kExt = ".slangp";
  if (relative.size() >= kExt.size() &&
      relative.compare(relative.size() - kExt.size(), kExt.size(), kExt) == 0)
  {
    relative = relative.substr(0, relative.size() - kExt.size());
  }
  return relative;
}

std::vector<std::string> MultipassPostProcessing::GetPresetList()
{
  const std::string user_dir = File::GetUserPath(D_SHADERS_IDX);
  const std::string sys_dir = File::GetSysDirectory() + SHADERS_DIR DIR_SEP;
  const std::array<std::string_view, 2> dirs = {user_dir, sys_dir};
  const std::array<std::string_view, 1> exts = {".slangp"};
  const std::vector<std::string> paths = Common::DoFileSearch(dirs, exts, /*recursive=*/true);

  const std::string user_slang = user_dir + "shaders_slang" DIR_SEP;
  const std::string sys_slang = sys_dir + "shaders_slang" DIR_SEP;
  const std::vector<std::string> roots = {user_slang, sys_slang, user_dir, sys_dir};
  std::vector<std::string> result;
  result.reserve(paths.size());
  const std::string rc_root = File::GetUserPath(D_SHADERS_IDX) + "RetroCrisis";
  const std::string rc_profile = ReadRetroCrisisProfile(rc_root);
  for (const std::string& path : paths)
  {
    if (!rc_profile.empty() && IsHiddenRetroCrisisPreset(path, rc_root, rc_profile))
      continue;
    result.push_back(PresetNameFromPath(path, roots));
  }
  return result;
}

bool MultipassPostProcessing::Initialize(AbstractTextureFormat format)
{
  m_framebuffer_format = format;
  LoadPreset(Config::Get(Config::GFX_ENHANCE_POST_SHADER));
  if (!m_passthrough)
    RecompilePipeline();
  // A post-processing preset failing to load must never block game boot: on any failure
  // LoadPreset falls back to pass-through (and has already surfaced a panic). Always report
  // success so Presenter::Initialize continues.
  return true;
}

void MultipassPostProcessing::RecompileShader()
{
  ClearChain();
  LoadPreset(Config::Get(Config::GFX_ENHANCE_POST_SHADER));
  if (!m_passthrough)
    RecompilePipeline();
}

void MultipassPostProcessing::ClearChain()
{
  m_passes.clear();
  m_luts.clear();
  m_history_textures.clear();
  m_max_history = 0;
  m_passthrough = true;
  m_pipeline_creation_failed = false;
  m_reported_pipeline_failure = false;
}

void MultipassPostProcessing::EnsureHistoryTextures(const AbstractTexture* original)
{
  const TextureConfig& src = original->GetConfig();
  const bool matches = m_history_textures.size() == m_max_history &&
                       (m_history_textures.empty() ||
                        (m_history_textures[0] && m_history_textures[0]->GetWidth() == src.width &&
                         m_history_textures[0]->GetHeight() == src.height &&
                         m_history_textures[0]->GetFormat() == src.format));
  if (matches)
    return;

  // The clone below inherits src.type, so this is the one place that can tell whether the frame the
  // chain was handed still matches what the passes declare. See SLANG_INPUT_TEXTURE_TYPE for why
  // the type is checked here rather than written at the allocation sites. Rebuilds only on a size
  // or format change, so this is not a per-frame check.
  ASSERT_MSG(VIDEO, src.type == SLANG_INPUT_TEXTURE_TYPE,
             "slang history source is {}, but the passes sample {}", src.type,
             SLANG_INPUT_TEXTURE_TYPE);

  m_history_textures.clear();
  m_history_textures.resize(m_max_history);
  for (u32 k = 0; k < m_max_history; ++k)
  {
    // Match the Original frame's format/size so CopyRectangleFromTexture (a straight copy) works.
    TextureConfig config = src;
    config.levels = 1;
    config.flags |= AbstractTextureFlag_RenderTarget;
    m_history_textures[k] = g_gfx->CreateTexture(config, "slang history " + std::to_string(k));
  }
}

void MultipassPostProcessing::ShiftHistory(const AbstractTexture* original)
{
  if (m_history_textures.empty())
    return;

  // Rotate the oldest slot to the front, then overwrite it with this frame's Original. Afterwards
  // m_history_textures[k] holds the frame from (k+1) frames ago.
  std::rotate(m_history_textures.begin(), m_history_textures.end() - 1, m_history_textures.end());
  AbstractTexture* const dst = m_history_textures.front().get();
  if (dst == nullptr)
    return;

  const u32 copy_width = std::min(dst->GetWidth(), original->GetWidth());
  const u32 copy_height = std::min(dst->GetHeight(), original->GetHeight());
  const MathUtil::Rectangle<int> rect(0, 0, static_cast<int>(copy_width),
                                      static_cast<int>(copy_height));
  dst->CopyRectangleFromTexture(original, rect, 0, 0, rect, 0, 0);
}

void MultipassPostProcessing::LoadPreset(const std::string& preset_spec)
{
  ClearChain();

  // Load a single preset. Configs written before the chain feature was removed may hold a
  // ';'-separated list; ResolveConfiguredPreset takes the first entry and warns.
  const std::string name = ResolveConfiguredPreset(preset_spec);
  if (!name.empty())
    AppendPreset(name);

  // Only now is it settled which pass is last, and the last pass is the one that draws to the
  // screen rather than into a texture.
  RetargetFinalPassToPresent();
  AnalyzeRenderStages();
  m_passthrough = m_passes.empty();
}

void MultipassPostProcessing::AnalyzeRenderStages()
{
  m_max_history = 0;
  for (Pass& pass : m_passes)
  {
    pass.has_feedback = false;
    pass.generate_mips = false;
  }
  if (m_passes.empty())
    return;

  std::vector<std::vector<std::string>> all_sampler_names;
  std::vector<bool> mipmap_input_flags;
  all_sampler_names.reserve(m_passes.size());
  mipmap_input_flags.reserve(m_passes.size());
  for (const Pass& pass : m_passes)
  {
    all_sampler_names.push_back(pass.sampler_names);
    mipmap_input_flags.push_back(pass.config.mipmap_input);
  }

  const std::set<std::string> feedback_aliases = ComputeFeedbackAliases(all_sampler_names);
  for (Pass& pass : m_passes)
    pass.has_feedback = !pass.alias.empty() && feedback_aliases.count(pass.alias) != 0;

  const std::set<size_t> mip_sources = ComputeMipmapSourcePasses(mipmap_input_flags);
  for (size_t i = 0; i < m_passes.size(); ++i)
    m_passes[i].generate_mips = mip_sources.count(i) != 0;

  m_max_history = ComputeMaxHistoryIndex(all_sampler_names);
}

void MultipassPostProcessing::AppendPreset(const std::string& preset_name)
{
  if (preset_name.empty())
    return;

  // Resolve preset path: try shaders_slang/ location first, then legacy flat location.
  std::string path = File::GetUserPath(D_SHADERS_IDX) + "shaders_slang" DIR_SEP + preset_name + ".slangp";
  if (!File::Exists(path))
    path = File::GetUserPath(D_SHADERS_IDX) + preset_name + ".slangp";
  if (!File::Exists(path))
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP "shaders_slang" DIR_SEP + preset_name + ".slangp";
  if (!File::Exists(path))
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + preset_name + ".slangp";
  if (!File::Exists(path))
    return;

  std::string text;
  if (!File::ReadFileToString(path, text))
    return;

  const std::string base_dir = DirectoryOf(path);
  std::string error;
  const SlangPresetReader preset_reader = [](const std::string& p, std::string* out) {
    return File::ReadFileToString(p, *out);
  };
  const auto config = ParseSlangPreset(text, base_dir, &error, preset_reader);
  if (!config)
  {
    ERROR_LOG_FMT(VIDEO, "Post-processing: failed to load preset {}: {}", preset_name, error);
    return;
  }

  // Load LUTs.
  std::vector<std::string> lut_names;
  for (const SlangLutConfig& lut_config : config->luts)
  {
    Lut lut;
    lut.name = lut_config.name;
    lut.texture = LoadLutTexture(lut_config);
    lut.sampler =
        MakeSlangSamplerState(lut_config.wrap_mode, lut_config.linear, lut_config.mipmap);
    lut_names.push_back(lut.name);
    m_luts.push_back(std::move(lut));
  }

  // On any failure, roll back just this preset's passes/LUTs so a bad preset later in a chain
  // doesn't discard earlier ones that loaded fine.
  const size_t passes_before = m_passes.size();
  const size_t luts_before = m_luts.size() - config->luts.size();
  const auto rollback = [&] {
    m_passes.resize(passes_before);
    m_luts.resize(luts_before);
  };

  // Build passes, accumulating known aliases as we go.
  std::vector<std::string> known_aliases;
  for (const SlangPassConfig& pass_config : config->passes)
  {
    std::string shader_text;
    if (!File::ReadFileToString(pass_config.shader_path, shader_text))
    {
      ERROR_LOG_FMT(VIDEO, "Post-processing: failed to read slang shader {}; skipping preset {}",
                    pass_config.shader_path, preset_name);
      rollback();
      return;
    }

    // Expand #includes before stage-splitting: some crt-royale passes keep their
    // #pragma stage bodies in an included header.
    const SlangFileReader reader = [](const std::string& p, std::string* out) {
      return File::ReadFileToString(p, *out);
    };
    shader_text = ExpandSlangIncludes(shader_text, DirectoryOf(pass_config.shader_path), reader);

    const auto parsed = ParseSlangShader(shader_text, &error);
    if (!parsed)
    {
      ERROR_LOG_FMT(VIDEO, "Post-processing: failed to parse {}: {}; skipping preset {}",
                    pass_config.shader_path, error, preset_name);
      rollback();
      return;
    }

    const APIType api_type = g_backend_info.api_type;
    TranslatedPass translated =
        TranslateSlangPass(*parsed, known_aliases, lut_names, SlangNeedsClipYFlip(api_type));
    if (!translated.ok)
    {
      ERROR_LOG_FMT(VIDEO, "Post-processing: cannot translate {}: {}; skipping preset {}",
                    pass_config.shader_path, translated.error, preset_name);
      rollback();
      return;
    }

    CompiledPassShaders shaders =
        CompileTranslatedPass(translated, DirectoryOf(pass_config.shader_path));
    if (!shaders.vertex || !shaders.pixel)
    {
      ERROR_LOG_FMT(VIDEO, "Post-processing: failed to compile {}; skipping preset {}",
                    pass_config.shader_path, preset_name);
      rollback();
      return;
    }

    Pass pass;
    pass.config = pass_config;
    pass.alias = pass_config.alias;
    pass.sampler_names = std::move(translated.sampler_names);
    pass.ubo_members = std::move(translated.ubo_members);
    pass.parameters = parsed->parameters;
    pass.parameter_overrides = config->parameter_overrides;
    pass.input_sampler =
        MakeSlangSamplerState(pass_config.wrap_mode, pass_config.filter_linear,
                              pass_config.mipmap_input);
    pass.vertex_shader = std::move(shaders.vertex);
    pass.pixel_shader = std::move(shaders.pixel);

    // Keep the present-target translation of this pass's vertex stage for
    // RetargetFinalPassToPresent to compile if this pass ends up last. Nothing to keep where the
    // two answers agree (every backend but OpenGL), and only the vertex source is kept: the
    // fragment stage, sampler bindings and UBO layout are identical either way -- the whole
    // difference is one line in the injected main().
    if (SlangNeedsPresentClipYFlip(api_type) != SlangNeedsClipYFlip(api_type))
    {
      TranslatedPass present = TranslateSlangPass(*parsed, known_aliases, lut_names,
                                                  SlangNeedsPresentClipYFlip(api_type));
      if (present.ok)
      {
        pass.present_vertex_glsl = std::move(present.vertex_glsl);
      }
      else
      {
        // Silence here was the whole bug: RetargetFinalPassToPresent returns without a word when
        // present_vertex_glsl is empty, so if this pass turned out to be the last one the frame
        // presented upside down on OpenGL with nothing in the log. Same symptom, same wording as
        // the compile-failure path in RetargetFinalPassToPresent.
        ERROR_LOG_FMT(VIDEO,
                      "Post-processing: failed to translate the present-target vertex shader for "
                      "pass '{}': {}; if this is the final pass the presented frame will be "
                      "vertically flipped",
                      pass_config.shader_path, present.error);
      }
    }

    m_passes.push_back(std::move(pass));

    if (!pass_config.alias.empty())
      known_aliases.push_back(pass_config.alias);
  }
}

void MultipassPostProcessing::RetargetFinalPassToPresent()
{
  if (m_passes.empty())
    return;

  // BlitFromTexture is only ever called with the backbuffer bound (Presenter::Present binds it,
  // then RenderXFBToScreen calls us), and it draws the last pass into that framebuffer -- the
  // is_final branches in RecompilePipeline and in the draw loop both key off this same
  // m_passes.size() - 1, and nothing adds or drops a pass between here and there: only LoadPreset
  // and ClearChain touch m_passes, and a per-preset rollback has already run by now.
  Pass& final_pass = m_passes.back();
  const std::string vertex_glsl = std::move(final_pass.present_vertex_glsl);
  // Every pass kept one of these because any of them could have turned out to be last. Now that
  // the answer is known the rest are dead weight, and this function is their only consumer.
  for (Pass& pass : m_passes)
    pass.present_vertex_glsl.clear();
  if (vertex_glsl.empty())
    return;

  std::unique_ptr<AbstractShader> vertex =
      CompileTranslatedVertex(vertex_glsl, DirectoryOf(final_pass.config.shader_path));
  if (!vertex)
  {
    // The pass keeps the vertex shader it already has, which flips: the frame is presented upside
    // down, the way it was before this was split, rather than not presented at all.
    ERROR_LOG_FMT(VIDEO,
                  "Post-processing: failed to compile the present-target vertex shader for final "
                  "pass '{}'; the presented frame will be vertically flipped",
                  final_pass.config.shader_path);
    return;
  }
  final_pass.vertex_shader = std::move(vertex);
}

void MultipassPostProcessing::RecompilePipeline()
{
  if (m_passthrough || m_framebuffer_format == AbstractTextureFormat::Undefined)
    return;

  // Clear the retryable failure latch before rebuilding: this call is the retry, and leaving the
  // latch set would make a rebuild that now succeeds still blit passthrough. m_passthrough is not
  // touched -- a preset with no passes never gets here.
  m_pipeline_creation_failed = false;

  // Clearing freshly-allocated feedback buffers below binds framebuffers; remember the currently
  // bound one so callers (e.g. mid-frame in BlitFromTexture) see no surprise framebuffer change.
  AbstractFramebuffer* const restore_framebuffer = g_gfx->GetCurrentFramebuffer();

  const u32 viewport_width = std::max<u32>(1, m_target_width);
  const u32 viewport_height = std::max<u32>(1, m_target_height);
  const u32 source_width = std::max<u32>(1, m_source_width);
  const u32 source_height = std::max<u32>(1, m_source_height);
  const u32 scaled_source_width = std::max<u32>(1, m_scaled_source_width);
  const u32 scaled_source_height = std::max<u32>(1, m_scaled_source_height);

  std::vector<SlangPassConfig> configs;
  configs.reserve(m_passes.size());
  for (const Pass& pass : m_passes)
    configs.push_back(pass.config);

  // Two parallel size chains:
  //  - logical_sizes: seeded from the NATIVE resolution -> reported to shaders as SourceSize so
  //    CRT scanline/mask geometry is identical at any internal resolution.
  //  - physical_sizes: seeded from the internal-resolution-SCALED resolution -> the actual RT
  //    allocation, so higher internal resolution supersamples the content through the effect.
  // Both use the same viewport for viewport-scaled passes.
  const std::vector<PassSize> logical_sizes =
      ComputePassChainSizes(configs, source_width, source_height, viewport_width, viewport_height);
  const std::vector<PassSize> physical_sizes = ComputePassChainSizes(
      configs, scaled_source_width, scaled_source_height, viewport_width, viewport_height);

  // Passes sampled with mipmapping always get a real chain: backends that can generate one on the
  // GPU do it in AbstractTexture::GenerateMipmaps(), the rest go through MipChainBuilder.
  constexpr bool mips_supported = true;

  const size_t pass_count = m_passes.size();
  for (size_t i = 0; i < pass_count; ++i)
  {
    Pass& pass = m_passes[i];
    const bool is_final = i == pass_count - 1;
    const AbstractTextureFormat output_format =
        is_final ? m_framebuffer_format : INTERMEDIATE_FORMAT;

    // Logical size is what the shader sees; final pass's logical output is the viewport.
    pass.logical_width = is_final ? viewport_width : logical_sizes[i].width;
    pass.logical_height = is_final ? viewport_height : logical_sizes[i].height;

    if (!is_final)
    {
      // Allocate at the physical (internal-res-scaled) size, but never smaller than logical.
      const u32 out_w = std::max(physical_sizes[i].width, logical_sizes[i].width);
      const u32 out_h = std::max(physical_sizes[i].height, logical_sizes[i].height);

      // A later pass sampling this output with mipmap_input=true needs a full mip chain here.
      const u32 levels =
          (pass.generate_mips && mips_supported) ? VideoCommon::MipLevelCount(out_w, out_h) : 1;

      const TextureConfig texture_config(out_w, out_h, levels, 1, 1, INTERMEDIATE_FORMAT,
                                         AbstractTextureFlag_RenderTarget,
                                         SLANG_INPUT_TEXTURE_TYPE);
      pass.output_texture =
          g_gfx->CreateTexture(texture_config, "slang pass " + std::to_string(i));
      pass.output_framebuffer =
          pass.output_texture ? g_gfx->CreateFramebuffer(pass.output_texture.get(), nullptr)
                              : nullptr;

      if (pass.has_feedback)
      {
        // Double-buffer: a second render target holds the previous frame's output.
        pass.feedback_texture =
            g_gfx->CreateTexture(texture_config, "slang feedback " + std::to_string(i));
        pass.feedback_framebuffer =
            pass.feedback_texture ? g_gfx->CreateFramebuffer(pass.feedback_texture.get(), nullptr)
                                  : nullptr;
        // Clear both buffers so the first frame's feedback sample is defined (black), not
        // undefined memory (which could feed NaNs into an afterglow accumulator).
        if (pass.output_framebuffer)
          g_gfx->SetAndClearFramebuffer(pass.output_framebuffer.get());
        if (pass.feedback_framebuffer)
          g_gfx->SetAndClearFramebuffer(pass.feedback_framebuffer.get());
      }
      else
      {
        pass.feedback_texture.reset();
        pass.feedback_framebuffer.reset();
      }
    }
    else
    {
      pass.output_texture.reset();
      pass.output_framebuffer.reset();
      pass.feedback_texture.reset();
      pass.feedback_framebuffer.reset();
    }

    AbstractPipelineConfig pipeline_config = {};
    pipeline_config.vertex_shader = pass.vertex_shader.get();
    pipeline_config.pixel_shader = pass.pixel_shader.get();
    pipeline_config.rasterization_state =
        RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
    pipeline_config.depth_state = RenderState::GetNoDepthTestingDepthState();
    pipeline_config.blending_state = RenderState::GetNoBlendingBlendState();
    pipeline_config.framebuffer_state = RenderState::GetColorFramebufferState(output_format);
    pipeline_config.usage = AbstractPipelineUsage::Utility;
    pass.pipeline = g_gfx->CreatePipeline(pipeline_config);

    // A pass whose pipeline or render target (for non-final passes) failed to create cannot be
    // skipped silently: if it is the final pass nothing reaches the backbuffer at all (a black
    // screen). Report it once per preset load and set the retryable latch so BlitFromTexture falls
    // back to the passthrough copy rather than presenting a chain with a hole in it. Not
    // m_passthrough: that one is permanent, and a size that cannot be allocated today may well be
    // allocatable after the user shrinks the window or lowers internal resolution.
    if (!pass.pipeline || (!is_final && !pass.output_framebuffer))
    {
      if (!m_reported_pipeline_failure)
      {
        ERROR_LOG_FMT(VIDEO,
                      "Post-processing: pass {} ('{}') has no {} at {}x{}; blitting passthrough "
                      "until a rebuild succeeds",
                      i, pass.config.shader_path, !pass.pipeline ? "pipeline" : "render target",
                      viewport_width, viewport_height);
        m_reported_pipeline_failure = true;
      }
      m_pipeline_creation_failed = true;
      if (restore_framebuffer != nullptr)
        g_gfx->SetFramebuffer(restore_framebuffer);
      return;
    }
  }

  // Restore whatever framebuffer was bound before any feedback-buffer clears above.
  if (restore_framebuffer != nullptr)
    g_gfx->SetFramebuffer(restore_framebuffer);
}

void MultipassPostProcessing::BuildPassthroughPipeline()
{
  AbstractFramebuffer* const framebuffer = g_gfx->GetCurrentFramebuffer();
  if (framebuffer == nullptr)
    return;
  const AbstractTextureFormat format = framebuffer->GetColorFormat();
  if (m_passthrough_pipeline && m_passthrough_format == format)
    return;

  // Fullscreen-triangle vertex shader + a plain textured copy. Uses Dolphin's per-backend
  // shader macros (defined by the backend header CreateShaderFromSource prepends), so it works
  // for any backbuffer format -- unlike ScaleTexture, which only supports RGBA8 targets.
  // BlitPassthrough only ever draws into the framebuffer being presented, so this asks
  // SlangNeedsPresentClipYFlip and not SlangNeedsClipYFlip: on OpenGL the latter is the answer for
  // a texture target only.
  const std::string flip_y = SlangNeedsPresentClipYFlip(g_backend_info.api_type) ?
                                 "  gl_Position.y = -gl_Position.y;\n" :
                                 "";
  const std::string vertex_source =
      "VARYING_LOCATION(0) out float2 v_tex0;\n"
      "void main() {\n"
      "  v_tex0 = float2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
      "  gl_Position = float4(v_tex0 * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n" +
      flip_y +
      "}\n";
  const char* const pixel_source =
      "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
      "VARYING_LOCATION(0) in float2 v_tex0;\n"
      "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n"
      "void main() {\n"
      "  ocol0 = texture(samp0, float3(v_tex0, 0.0));\n"
      "}\n";

  m_passthrough_vertex = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, vertex_source, nullptr,
                                                       "slang passthrough vertex");
  m_passthrough_pixel = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pixel_source, nullptr,
                                                      "slang passthrough pixel");
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

void MultipassPostProcessing::BlitPassthrough(const MathUtil::Rectangle<int>& dst,
                                              const AbstractTexture* src_tex,
                                              AbstractFramebuffer* framebuffer)
{
  BuildPassthroughPipeline();
  if (!m_passthrough_pipeline)
    return;

  g_gfx->SetTexture(0, src_tex);
  g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(dst, framebuffer));
  g_gfx->SetPipeline(m_passthrough_pipeline.get());
  g_gfx->Draw(0, 3);
}

void MultipassPostProcessing::BlitFromTexture(const MathUtil::Rectangle<int>& dst,
                                              const MathUtil::Rectangle<int>& src,
                                              const AbstractTexture* src_tex, int src_layer,
                                              u32 native_width, u32 native_height)
{
  // Only the permanent condition returns here. A chain whose last rebuild failed deliberately falls
  // through to the size/format check below, because that is the only per-frame caller of
  // RecompilePipeline and therefore the only way the failure can ever be retried.
  if (m_passthrough || m_passes.empty())
  {
    BlitPassthrough(dst, src_tex, g_gfx->GetCurrentFramebuffer());
    return;
  }

  // The incoming frame has to be the type the translated passes declare their samplers as -- see
  // SLANG_INPUT_TEXTURE_TYPE for why that is checked here instead of written where the frame is
  // allocated. The passthrough path above needs the same thing (its pixel shader spells out
  // sampler2DArray) but returns before this point, so this covers the translated chain only.
  // Per-frame, hence debug-only. It exists for the one case the release-live check in the rebuild
  // branch below cannot see: a source type that changes while every size and format stays put.
  DEBUG_ASSERT_MSG(VIDEO, src_tex->GetConfig().type == SLANG_INPUT_TEXTURE_TYPE,
                   "slang chain source is {}, but the passes sample {}", src_tex->GetConfig().type,
                   SLANG_INPUT_TEXTURE_TYPE);

  // Two source sizes drive the chain:
  //  - native (m_source_*): reported to the shader as SourceSize so CRT scanline/mask geometry
  //    is identical at any internal resolution.
  //  - scaled (m_scaled_source_*): the actual src_tex region, used to allocate render targets so
  //    higher internal resolution supersamples the game content fed through the effect.
  // Fall back to the src rect when the caller didn't supply a native size.
  const u32 target_width = static_cast<u32>(dst.GetWidth());
  const u32 target_height = static_cast<u32>(dst.GetHeight());
  const u32 scaled_source_width = static_cast<u32>(src.GetWidth());
  const u32 scaled_source_height = static_cast<u32>(src.GetHeight());
  const u32 source_width = native_width != 0 ? native_width : scaled_source_width;
  const u32 source_height = native_height != 0 ? native_height : scaled_source_height;
  const AbstractTextureFormat current_format =
      g_gfx->GetCurrentFramebuffer()->GetColorFormat();
  if (current_format != m_framebuffer_format || target_width != m_target_width ||
      target_height != m_target_height || source_width != m_source_width ||
      source_height != m_source_height || scaled_source_width != m_scaled_source_width ||
      scaled_source_height != m_scaled_source_height)
  {
    // Release-live counterpart of the debug assert above, placed where the sampler declarations are
    // about to be fixed: RecompilePipeline() re-translates every pass, emitting the sampler type
    // from SLANG_INPUT_TEXTURE_TYPE. Every preset that has a translated chain reaches this on its
    // first frame -- the passthrough path returns before both asserts, so neither covers it -- and
    // that is what makes this the check that actually covers the chain: the one in
    // EnsureHistoryTextures is skipped entirely by presets that never ask for OriginalHistoryN.
    ASSERT_MSG(VIDEO, src_tex->GetConfig().type == SLANG_INPUT_TEXTURE_TYPE,
               "slang chain source is {}, but the passes sample {}", src_tex->GetConfig().type,
               SLANG_INPUT_TEXTURE_TYPE);

    m_framebuffer_format = current_format;
    m_target_width = target_width;
    m_target_height = target_height;
    m_source_width = source_width;
    m_source_height = source_height;
    m_scaled_source_width = scaled_source_width;
    m_scaled_source_height = scaled_source_height;
    RecompilePipeline();
  }

  // If the rebuild above (or an earlier one at this same size) could not create a pass's pipeline
  // or render target, fall back to the passthrough copy rather than executing the chain with null
  // pipelines or framebuffers. The next size or format change retries.
  if (m_pipeline_creation_failed)
  {
    BlitPassthrough(dst, src_tex, g_gfx->GetCurrentFramebuffer());
    return;
  }

  ++m_frame_count;

  // Frame-history ring: keep copies of the Original frame for OriginalHistoryN (N>=1). No-op for
  // presets that only use OriginalHistory0 / Original.
  if (m_max_history >= 1)
    EnsureHistoryTextures(src_tex);

  AbstractFramebuffer* const entry_framebuffer = g_gfx->GetCurrentFramebuffer();
  const AbstractTexture* const original_tex = src_tex;
  const AbstractTexture* prev_output = src_tex;
  // SourceSize/OriginalSize use the native resolution (m_source_width/height), so scanline and
  // mask geometry are resolution-independent even though we sample the upscaled src_tex.
  const MathUtil::Rectangle<int> native_rect(0, 0, static_cast<int>(m_source_width),
                                             static_cast<int>(m_source_height));
  MathUtil::Rectangle<int> prev_rect = native_rect;

  const size_t pass_count = m_passes.size();
  for (size_t i = 0; i < pass_count; ++i)
  {
    Pass& pass = m_passes[i];
    // Defensive guard: the real detection happens in RecompilePipeline, which sets
    // m_pipeline_creation_failed and returns early above. This cannot be reached with a failed
    // chain.
    if (!pass.pipeline)
      continue;

    const bool is_final = i == pass_count - 1;

    // Resolve and bind this pass's input samplers.
    for (u32 binding = 0; binding < pass.sampler_names.size(); ++binding)
    {
      const std::string& name = pass.sampler_names[binding];
      const AbstractTexture* texture = prev_output;
      SamplerState sampler = pass.input_sampler;

      if (name == "Original")
      {
        texture = original_tex;
      }
      else if (name == "Source")
      {
        texture = prev_output;
      }
      else if (const std::optional<u32> history_index = ParseOriginalHistoryIndex(name))
      {
        // OriginalHistory0 == the current Original frame; N>=1 is N frames earlier. Fall back to
        // the current frame before that many frames have elapsed (ring not yet populated).
        if (*history_index == 0)
          texture = original_tex;
        else if (*history_index <= m_frame_count && *history_index - 1 < m_history_textures.size() &&
                 m_history_textures[*history_index - 1])
          texture = m_history_textures[*history_index - 1].get();
        else
          texture = original_tex;
      }
      else
      {
        bool resolved = false;
        // "<Alias>Feedback": the previous frame's copy of that alias's output.
        constexpr std::string_view kFeedback = "Feedback";
        if (name.size() > kFeedback.size() &&
            name.compare(name.size() - kFeedback.size(), kFeedback.size(), kFeedback) == 0)
        {
          const std::string base = name.substr(0, name.size() - kFeedback.size());
          for (Pass& other : m_passes)
          {
            if (other.has_feedback && other.alias == base)
            {
              // feedback_texture holds last frame's output; before the first swap it is the
              // cleared buffer, which is fine (defined black).
              texture = other.feedback_texture ? other.feedback_texture.get()
                                                : other.output_texture.get();
              resolved = true;
              break;
            }
          }
        }
        // Alias of an earlier pass (this frame's output)?
        if (!resolved)
        {
          for (size_t j = 0; j < i; ++j)
          {
            if (!m_passes[j].alias.empty() && m_passes[j].alias == name &&
                m_passes[j].output_texture)
            {
              texture = m_passes[j].output_texture.get();
              resolved = true;
              break;
            }
          }
        }
        // LUT?
        if (!resolved)
        {
          for (const Lut& lut : m_luts)
          {
            if (lut.name == name && lut.texture)
            {
              texture = lut.texture.get();
              sampler = lut.sampler;
              resolved = true;
              break;
            }
          }
        }
      }

      if (texture == nullptr)
        texture = original_tex;

      // Ensure any render-target input has finished rendering (Vulkan layout transition).
      if (texture != original_tex && texture != src_tex)
        const_cast<AbstractTexture*>(texture)->FinishedRendering();

      g_gfx->SetTexture(binding, texture);
      g_gfx->SetSamplerState(binding, sampler);
    }

    AbstractFramebuffer* target =
        is_final ? entry_framebuffer : pass.output_framebuffer.get();
    if (target == nullptr)
      continue;

    // Physical target rect (actual RT/backbuffer pixels) drives the GPU viewport; logical rect
    // (native-derived) is what the shader's *Size uniforms report.
    const MathUtil::Rectangle<int> target_rect =
        is_final ? dst : pass.output_texture->GetRect();
    const MathUtil::Rectangle<int> logical_target_rect(
        0, 0, static_cast<int>(pass.logical_width), static_cast<int>(pass.logical_height));

    // Fill + upload the merged PSBlock, packed to match the shader's declared std140 layout.
    // Resolve each member by name: MVP, the *Size semantics, FrameCount, per-input <Name>Size,
    // and #pragma parameter defaults.
    const auto size_vec = [](u32 w, u32 h, float* out) {
      out[0] = static_cast<float>(w);
      out[1] = static_cast<float>(h);
      out[2] = w != 0 ? 1.0f / w : 0.0f;
      out[3] = h != 0 ? 1.0f / h : 0.0f;
    };
    // Resolve a texture size by input name (Source/Original/alias/LUT), for "<Name>Size".
    const auto input_size = [&](const std::string& name, float* out) -> bool {
      if (name == "Source")
      {
        size_vec(static_cast<u32>(prev_rect.GetWidth()), static_cast<u32>(prev_rect.GetHeight()),
                 out);
        return true;
      }
      if (name == "Original")
      {
        size_vec(static_cast<u32>(native_rect.GetWidth()),
                 static_cast<u32>(native_rect.GetHeight()), out);
        return true;
      }
      // OriginalHistoryN shares the Original's (native) logical size.
      if (ParseOriginalHistoryIndex(name))
      {
        size_vec(static_cast<u32>(native_rect.GetWidth()),
                 static_cast<u32>(native_rect.GetHeight()), out);
        return true;
      }
      // "<Alias>Feedback" reports the aliased pass's logical size (same as its current output).
      constexpr std::string_view kFeedback = "Feedback";
      if (name.size() > kFeedback.size() &&
          name.compare(name.size() - kFeedback.size(), kFeedback.size(), kFeedback) == 0)
      {
        const std::string base = name.substr(0, name.size() - kFeedback.size());
        for (const Pass& other : m_passes)
        {
          if (other.has_feedback && other.alias == base)
          {
            size_vec(other.logical_width, other.logical_height, out);
            return true;
          }
        }
      }
      for (size_t j = 0; j < i; ++j)
      {
        if (m_passes[j].alias == name && m_passes[j].output_texture)
        {
          // Report the alias's LOGICAL size (native-derived), not the physical RT size.
          size_vec(m_passes[j].logical_width, m_passes[j].logical_height, out);
          return true;
        }
      }
      for (const Lut& lut : m_luts)
      {
        if (lut.name == name && lut.texture)
        {
          size_vec(lut.texture->GetWidth(), lut.texture->GetHeight(), out);
          return true;
        }
      }
      return false;
    };

    const auto resolver = [&](const std::string& name, float* out, int count) -> bool {
      if (name == "MVP")
      {
        const std::array<float, 16> mvp = IdentityMvp();
        std::copy(mvp.begin(), mvp.end(), out);
        return true;
      }
      if (name == "SourceSize")
      {
        size_vec(static_cast<u32>(prev_rect.GetWidth()), static_cast<u32>(prev_rect.GetHeight()),
                 out);
        return true;
      }
      if (name == "OriginalSize")
      {
        size_vec(static_cast<u32>(native_rect.GetWidth()),
                 static_cast<u32>(native_rect.GetHeight()), out);
        return true;
      }
      if (name == "OutputSize" || name == "FinalViewportSize")
      {
        size_vec(static_cast<u32>(logical_target_rect.GetWidth()),
                 static_cast<u32>(logical_target_rect.GetHeight()), out);
        return true;
      }
      if (name == "FrameCount")
      {
        out[0] = static_cast<float>(m_frame_count);
        return true;
      }
      // "<InputName>Size" -> that input's texture size.
      constexpr std::string_view kSize = "Size";
      if (name.size() > kSize.size() &&
          name.compare(name.size() - kSize.size(), kSize.size(), kSize) == 0)
      {
        if (input_size(name.substr(0, name.size() - kSize.size()), out))
          return true;
      }
      // Preset override wins over the #pragma parameter default.
      float param_value = 0.0f;
      if (ResolveShaderParameter(pass.parameter_overrides, pass.parameters, name, &param_value))
      {
        out[0] = param_value;
        return true;
      }
      (void)count;
      return false;  // zero-fill unknown members
    };

    const std::vector<u8> ubo_data = PackSlangUniforms(pass.ubo_members, resolver);
    if (!ubo_data.empty())
      g_vertex_manager->UploadUtilityUniforms(ubo_data.data(), static_cast<u32>(ubo_data.size()));

    g_gfx->SetFramebuffer(target);
    g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(target_rect, target));
    g_gfx->SetPipeline(pass.pipeline.get());
    g_gfx->Draw(0, 3);

    if (!is_final)
    {
      // A later pass samples this output with mipmapping: build its mip chain now, from the level-0
      // content just rendered.
      if (pass.generate_mips && pass.output_texture)
      {
        if (g_backend_info.bSupportsGPUMipGeneration)
          pass.output_texture->GenerateMipmaps();
        else
          m_mip_builder.Generate(pass.output_texture.get());
      }

      prev_output = pass.output_texture.get();
      // Advance the logical "Source" size to this pass's logical output (native-derived), so the
      // next pass's SourceSize is resolution-independent even though prev_output is a larger RT.
      prev_rect = logical_target_rect;
    }
  }

  // End-of-frame bookkeeping for cross-frame features:
  //  - Feedback passes: swap this frame's output into the feedback slot so next frame's
  //    "<Alias>Feedback" sample sees it, and render next frame into the old feedback buffer.
  //  - History ring: record this frame's Original for OriginalHistoryN.
  for (Pass& pass : m_passes)
  {
    if (pass.has_feedback)
    {
      std::swap(pass.output_texture, pass.feedback_texture);
      std::swap(pass.output_framebuffer, pass.feedback_framebuffer);
    }
  }
  if (m_max_history >= 1)
    ShiftHistory(src_tex);

  // Ensure the final target is the framebuffer we entered with.
  g_gfx->SetFramebuffer(entry_framebuffer);
  (void)src_layer;
}
}  // namespace VideoCommon
