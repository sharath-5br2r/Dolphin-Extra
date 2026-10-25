// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/PostProcessing/IPostProcessor.h"
#include "VideoCommon/PostProcessing/LibrashaderParameters.h"
#include "VideoCommon/PostProcessing/SlangSourceDownscale.h"

class AbstractShader;
class AbstractPipeline;
class AbstractTexture;
class AbstractFramebuffer;

namespace VideoCommon
{
// Forward-declared rather than included: LibrashaderRuntime.h reaches <librashader.h>, and the
// fewer translation units that can see that header the harder it is to trip over its include-order
// trap (see the top of VKLibrashaderRuntime.cpp). unique_ptr only needs the complete type where the
// destructor is defined, which is LibrashaderPostProcessing.cpp.
class LibrashaderRuntime;

// Resolves a post-processing preset name to an absolute .slangp path using the identical search
// order as MultipassPostProcessing::AppendPreset(), so both engines consume the same preset file.
// librashader accepts a single preset, so only the first entry of a ';'-separated chain is used;
// the dropped entries are named in a warning. Returns "" when no candidate path exists.
std::string ResolvePresetPath(const std::string& preset_spec);

// Post-processing engine backed by a librashader native runtime. Resolves the selected preset,
// asks the runtime to build a filter chain from it, and per frame renders the internally-upscaled
// source down to native resolution, records the chain into a draw-rect-sized target and presents
// that 1:1 into the backbuffer. Everything here is backend-agnostic; the handful of calls that are
// not live behind LibrashaderRuntime. Without a chain (no preset, preset failed, library missing)
// and on any per-frame chain error, BlitFromTexture falls back to a passthrough copy so the screen
// never blanks.
class LibrashaderPostProcessing final : public IPostProcessor
{
public:
  explicit LibrashaderPostProcessing(std::unique_ptr<LibrashaderRuntime> runtime);
  ~LibrashaderPostProcessing() override;

  bool Initialize(AbstractTextureFormat format) override;
  void RecompileShader() override;
  void RecompilePipeline() override;
  void BlitFromTexture(const MathUtil::Rectangle<int>& dst, const MathUtil::Rectangle<int>& src,
                       const AbstractTexture* src_tex, int src_layer, u32 native_width,
                       u32 native_height) override;

private:
  // Builds (or rebuilds, on framebuffer-format change) the fullscreen-triangle copy pipeline used
  // for passthrough rendering. Mirrors MultipassPostProcessing::BuildPassthroughPipeline().
  void BuildPassthroughPipeline();

  // Loads the stored overrides for the current preset and applies them to the live chain. Called
  // once at chain creation and again whenever the generation counter changes.
  void ApplyStoredOverrides();

  // Renders the internally-upscaled source down to a native-resolution texture per `plan`, so the
  // filter chain derives its geometry from native pixels and the discarded upscale detail becomes
  // supersampling (box) rather than aliasing (single bilinear tap). Returns the native-res source
  // texture, or nullptr on allocation/pipeline failure. The shader-read transition is the
  // runtime's job: RunFrame() transitions whichever texture it is handed.
  AbstractTexture* DownscaleToNativeSource(const SlangSourceDownscalePlan& plan,
                                           const AbstractTexture* src_tex,
                                           const MathUtil::Rectangle<int>& src, int src_layer,
                                           u32 native_width, u32 native_height);

  // (Re)builds the downscale pipeline. The box pixel shader bakes the factor as a literal, so it is
  // rebuilt whenever the factor, filter kind (box vs bilinear), or color format changes.
  void BuildDownscalePipeline(const SlangSourceDownscalePlan& plan, AbstractTextureFormat format);

  // (Re)allocates the draw-rect-sized target the filter chain renders into (at viewport origin
  // 0,0), so librashader's OutputSize/FinalViewportSize and every scale_type=viewport pass match
  // the actually-drawn extent instead of the full backbuffer. Returns the target's framebuffer,
  // which is what RunFrame() takes, or nullptr on allocation failure. See BlitFromTexture for why
  // this matters (vertical-moire root cause).
  AbstractFramebuffer* EnsureOutputTarget(u32 width, u32 height, AbstractTextureFormat format);

  // The backend's binding to librashader. Never null; HasChain() false means "passthrough".
  std::unique_ptr<LibrashaderRuntime> m_runtime;

  // No cached backbuffer format here on purpose. Every format decision is taken per frame from the
  // live framebuffer (BuildPassthroughPipeline, EnsureOutputTarget), because the backbuffer format
  // changes when HDR is toggled -- a value latched at Initialize() would be stale from that moment
  // on.
  u64 m_frame_count = 0;
  bool m_available = false;

  // Passthrough copy resources.
  std::unique_ptr<AbstractShader> m_passthrough_vertex;
  std::unique_ptr<AbstractShader> m_passthrough_pixel;
  std::unique_ptr<AbstractPipeline> m_passthrough_pipeline;
  AbstractTextureFormat m_passthrough_format = AbstractTextureFormat::Undefined;

  // Native-resolution source produced by downscaling the upscaled frame before the chain runs.
  std::unique_ptr<AbstractTexture> m_native_source;
  std::unique_ptr<AbstractFramebuffer> m_native_source_fb;
  u32 m_native_source_width = 0;
  u32 m_native_source_height = 0;
  AbstractTextureFormat m_native_source_format = AbstractTextureFormat::Undefined;

  // Downscale pipeline. m_downscale_factor caches the box factor the current pixel shader was baked
  // for (0 = bilinear fallback); the shader is rebuilt only when the factor/kind or format changes.
  std::unique_ptr<AbstractShader> m_downscale_vertex;
  std::unique_ptr<AbstractShader> m_downscale_pixel;
  std::unique_ptr<AbstractPipeline> m_downscale_pipeline;
  u32 m_downscale_factor = 0;
  bool m_downscale_is_box = false;
  AbstractTextureFormat m_downscale_format = AbstractTextureFormat::Undefined;

  // Draw-rect-sized target the filter chain renders into, then blitted 1:1 into the backbuffer.
  std::unique_ptr<AbstractTexture> m_output_target;
  std::unique_ptr<AbstractFramebuffer> m_output_target_fb;
  u32 m_output_target_width = 0;
  u32 m_output_target_height = 0;
  AbstractTextureFormat m_output_target_format = AbstractTextureFormat::Undefined;

  // Parameter live-update: enumerated list (cached at chain creation) and the last-seen generation.
  std::vector<LibrashaderParameters::ParameterInfo> m_parameters;
  std::string m_preset_relative_path;
  u32 m_last_seen_generation = 0;
};
}  // namespace VideoCommon
