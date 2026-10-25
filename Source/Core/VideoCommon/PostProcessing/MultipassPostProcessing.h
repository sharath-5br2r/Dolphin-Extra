// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/MipChainBuilder.h"
#include "VideoCommon/PostProcessing/IPostProcessor.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"
#include "VideoCommon/PostProcessing/SlangShader.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;

namespace VideoCommon
{
// Multi-pass RetroArch-style .slangp post-processor. Replaces the old single-pass PostProcessing.
class MultipassPostProcessing final : public IPostProcessor
{
public:
  MultipassPostProcessing();
  ~MultipassPostProcessing();

  // *.slangp presets discovered under the user + sys Shaders dirs, as identifiers relative to
  // the containing Shaders dir with the extension stripped (e.g. "crt/crt-royale"). This is the
  // value stored in GFX_ENHANCE_POST_SHADER and resolved back by LoadPreset.
  static std::vector<std::string> GetPresetList();

  // Loads the preset named by GFX_ENHANCE_POST_SHADER (a .slangp under the Shaders dir).
  // Empty/failed -> pass-through mode.
  bool Initialize(AbstractTextureFormat format) override;
  void RecompileShader() override;    // reload preset (config change)
  void RecompilePipeline() override;  // rebuild pipelines only (format change)

  // Runs the pass chain, presenting into the currently-bound framebuffer over dst. `src` is the
  // (internal-resolution-scaled) region of src_tex to read; `native_width`/`native_height` are
  // the game's native resolution (before internal-resolution upscaling). The shader's SourceSize
  // and the pass-chain sizing use the NATIVE size so effects like crt-royale render the same
  // scanline/mask geometry regardless of internal resolution, while still sampling the high-res
  // texture. Pass 0 for native size to fall back to the src rect size.
  void BlitFromTexture(const MathUtil::Rectangle<int>& dst, const MathUtil::Rectangle<int>& src,
                       const AbstractTexture* src_tex, int src_layer, u32 native_width,
                       u32 native_height) override;

private:
  struct Pass
  {
    SlangPassConfig config;
    std::vector<std::string> sampler_names;  // binding index -> input name
    std::vector<UboMember> ubo_members;      // merged PSBlock layout, in declaration order
    std::vector<SlangParameter> parameters;  // #pragma parameter defaults for this pass
    std::map<std::string, float> parameter_overrides;  // preset-level overrides (flat namespace)
    SamplerState input_sampler;              // sampler applied to this pass's inputs
    std::unique_ptr<AbstractShader> vertex_shader;
    std::unique_ptr<AbstractShader> pixel_shader;
    // This pass's vertex stage translated for a draw that targets the presented framebuffer
    // instead of one of our textures, i.e. with SlangNeedsPresentClipYFlip in place of
    // SlangNeedsClipYFlip. Only the chain's last pass draws there, and which pass that is is not
    // known while a preset is being appended (a later preset appends more passes, a failing one
    // rolls its own back), so every pass carries the alternative source and
    // RetargetFinalPassToPresent compiles the one that turns out to be last. Empty on the backends
    // whose two answers agree -- every backend except OpenGL -- so they translate and compile
    // exactly what they did before.
    std::string present_vertex_glsl;
    std::unique_ptr<AbstractPipeline> pipeline;
    std::unique_ptr<AbstractTexture> output_texture;  // null for final pass; this frame's output
    std::unique_ptr<AbstractFramebuffer> output_framebuffer;
    // Previous frame's copy of this pass's output, bound when its alias is sampled as
    // "<Alias>Feedback". Allocated and swapped with output_texture each frame only when
    // has_feedback; otherwise null.
    std::unique_ptr<AbstractTexture> feedback_texture;
    std::unique_ptr<AbstractFramebuffer> feedback_framebuffer;
    std::string alias;
    // Logical (native-derived) output size reported to the shader as SourceSize/OutputSize, so
    // CRT geometry is resolution-independent. The output_texture is allocated at the larger
    // internal-resolution-scaled size, so higher internal resolution still supersamples the
    // content fed through the effect.
    u32 logical_width = 0;
    u32 logical_height = 0;
    // Some later pass's alias == this pass's alias is sampled as "<Alias>Feedback": double-buffer
    // this pass's output across frames.
    bool has_feedback = false;
    // A later pass samples this pass's output with mipmapping (its mipmap_input=true): allocate a
    // full mip chain for output_texture and call GenerateMipmaps() after rendering it.
    bool generate_mips = false;
  };
  struct Lut
  {
    std::string name;
    std::unique_ptr<AbstractTexture> texture;
    SamplerState sampler;
  };

  // Loads the preset spec (a single preset name, or a ';'-separated chain) into the pass chain,
  // falling back to pass-through if nothing loads (a bad/unsupported preset must never block
  // game boot).
  void LoadPreset(const std::string& preset_spec);
  // Appends one preset's LUTs + passes to the current chain (used by LoadPreset for each preset
  // in a chain). Rolls back its own additions on failure, leaving earlier presets intact.
  void AppendPreset(const std::string& preset_name);
  // Swaps the assembled chain's last pass onto its present-target vertex shader (see
  // Pass::present_vertex_glsl). That pass renders straight into the framebuffer being presented,
  // where OpenGL needs no clip-space Y flip, unlike every earlier pass, which renders into a
  // texture. No-op when the chain is empty and on the backends whose two answers agree.
  void RetargetFinalPassToPresent();
  void ClearChain();
  // Builds the built-in pass-through pipeline (a plain copy of the input) for the current
  // framebuffer format, used when no user preset is active or a preset failed to load.
  void BuildPassthroughPipeline();
  // Scans the assembled pass chain and sets per-pass render-stage flags (has_feedback,
  // generate_mips) and m_max_history from the passes' sampler references. Called once per load.
  void AnalyzeRenderStages();
  // Lazily (re)allocates the frame-history ring to match the current Original frame. No-op unless
  // the chain references OriginalHistoryN with N>=1.
  void EnsureHistoryTextures(const AbstractTexture* original);
  // Rotates the frame-history ring and copies the current Original frame into the newest slot.
  void ShiftHistory(const AbstractTexture* original);

  std::vector<Pass> m_passes;
  std::vector<Lut> m_luts;
  AbstractTextureFormat m_framebuffer_format = AbstractTextureFormat::Undefined;
  // Two distinct reasons to blit passthrough instead of running the chain, deliberately kept apart:
  //
  //  - m_passthrough is the PERMANENT one: this preset has no passes to run (no preset configured,
  //    or the preset failed to load or translate). Nothing but another LoadPreset can change that,
  //    so BlitFromTexture returns straight away without even considering a rebuild.
  //  - m_pipeline_creation_failed is the RETRYABLE one: the passes exist, but the last
  //    RecompilePipeline could not create a pipeline or a render target for one of them -- a 4K
  //    window, a ten-pass preset, VRAM pressure. That is a property of the current size and format,
  //    not of the preset, so it must not outlive the size or format that caused it. Cleared on
  //    entry to RecompilePipeline so shrinking the window or lowering internal resolution actually
  //    recovers; latching it into m_passthrough instead disabled post-processing for the whole
  //    session after one transient allocation failure.
  bool m_passthrough = true;
  bool m_pipeline_creation_failed = false;
  u32 m_frame_count = 0;
  u32 m_target_width = 0;
  u32 m_target_height = 0;
  // Native (pre-upscale) source resolution -> drives logical SourceSize (stable CRT geometry).
  u32 m_source_width = 0;
  u32 m_source_height = 0;
  // Internal-resolution-scaled source resolution -> drives physical RT allocation (so higher
  // internal resolution supersamples the content through the effect).
  u32 m_scaled_source_width = 0;
  u32 m_scaled_source_height = 0;
  // Ring of past-frame copies of the Original (game) frame. m_history_textures[k] holds the frame
  // from (k+1) frames ago; "OriginalHistoryN" (N>=1) binds m_history_textures[N-1].
  // OriginalHistory0 == Original (the current frame). Empty unless the chain uses N>=1.
  std::vector<std::unique_ptr<AbstractTexture>> m_history_textures;
  u32 m_max_history = 0;

  std::unique_ptr<AbstractShader> m_passthrough_vertex;
  std::unique_ptr<AbstractShader> m_passthrough_pixel;
  std::unique_ptr<AbstractPipeline> m_passthrough_pipeline;
  AbstractTextureFormat m_passthrough_format = AbstractTextureFormat::Undefined;

  // Used on backends where AbstractTexture::GenerateMipmaps() is a no-op.
  VideoCommon::MipChainBuilder m_mip_builder;

  // One-shot diagnostic for pass pipeline / render-target creation failure. Reset only by
  // ClearChain(), i.e. once per preset load, NOT on a successful rebuild: RecompilePipeline runs on
  // every size and format change, so a chain that fails at large sizes and succeeds at small ones
  // would otherwise log again on every window drag. One line per preset load is the diagnostic; the
  // recovery itself is silent by design.
  bool m_reported_pipeline_failure = false;

  // Performs a passthrough copy of src_tex to the given framebuffer over dst, using the built-in
  // passthrough pipeline (a plain linear-sampled blit). Used when no user preset is active, when
  // a preset failed to load/compile, or when RecompilePipeline could not create a pass's pipeline
  // or render target at the current size.
  void BlitPassthrough(const MathUtil::Rectangle<int>& dst, const AbstractTexture* src_tex,
                       AbstractFramebuffer* framebuffer);
};

// Computes the preset identifier for a discovered .slangp path: the path relative to whichever
// root in `roots` contains it, with separators normalized to '/' and the ".slangp" extension
// stripped (e.g. "/sys/Shaders/crt/crt-royale.slangp" -> "crt/crt-royale"). If no root matches,
// falls back to the bare filename without extension. Pure/testable.
std::string PresetNameFromPath(const std::string& full_path,
                               const std::vector<std::string>& roots);
}  // namespace VideoCommon
