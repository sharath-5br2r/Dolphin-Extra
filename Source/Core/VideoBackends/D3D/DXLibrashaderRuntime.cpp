// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This must stay above every #include in this file. LibrashaderLoader.h includes <librashader.h>
// deliberately without any LIBRA_RUNTIME_* macro, so it stays cheap for the rest of VideoCommon.
// Whichever translation unit includes it first thereby satisfies librashader.h's include guard, so
// a later `#include <librashader.h>` here would be a silent no-op leaving every libra_d3d11_*
// declaration out -- with no error, because the runtime sections are #ifdef'd, not #error'd.
#define LIBRA_RUNTIME_D3D11
#include <librashader.h>

#include "VideoBackends/D3D/DXLibrashaderRuntime.h"

#include <utility>

#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/Constants.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace DX11
{
namespace
{
// D3D11 chain entry points, resolved once. Absent symbols leave the pointers null, which
// IsSupported() reports rather than calling through.
struct D3D11Functions
{
  PFN_libra_d3d11_filter_chain_create create = nullptr;
  PFN_libra_d3d11_filter_chain_frame frame = nullptr;
  PFN_libra_d3d11_filter_chain_set_param set_param = nullptr;
  PFN_libra_d3d11_filter_chain_free free = nullptr;

  D3D11Functions()
  {
    using VideoCommon::Librashader::GetSymbol;
    create = reinterpret_cast<PFN_libra_d3d11_filter_chain_create>(
        GetSymbol("libra_d3d11_filter_chain_create"));
    frame = reinterpret_cast<PFN_libra_d3d11_filter_chain_frame>(
        GetSymbol("libra_d3d11_filter_chain_frame"));
    set_param = reinterpret_cast<PFN_libra_d3d11_filter_chain_set_param>(
        GetSymbol("libra_d3d11_filter_chain_set_param"));
    free = reinterpret_cast<PFN_libra_d3d11_filter_chain_free>(
        GetSymbol("libra_d3d11_filter_chain_free"));
  }

  bool Complete() const { return create && frame && set_param && free; }
};

const D3D11Functions& Functions()
{
  static const D3D11Functions s_functions;
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
}  // namespace

DXLibrashaderRuntime::DXLibrashaderRuntime() = default;

DXLibrashaderRuntime::~DXLibrashaderRuntime()
{
  DestroyChain();
}

bool DXLibrashaderRuntime::IsSupported() const
{
  return VideoCommon::Librashader::GetAvailability().available && Functions().Complete();
}

bool DXLibrashaderRuntime::CreateChain(libra_shader_preset_t preset)
{
  filter_chain_d3d11_opt_t options = {};
  options.version = LIBRASHADER_CURRENT_VERSION;
  options.force_no_mipmaps = false;
  options.disable_cache = false;

  libra_d3d11_filter_chain_t chain = nullptr;
  // create() invalidates `preset` on success and on failure alike, so it is never freed here.
  //
  // Success is decided by CheckError, i.e. by the error handle, not by whether its description
  // renders non-empty: an error with an empty message read as success left m_chain null while the
  // log said the chain had been created, so the user got silent passthrough with no error line.
  if (CheckError(Functions().create(&preset, D3D::device.Get(), &options, &chain),
                 "d3d11_filter_chain_create"))
  {
    m_chain = nullptr;
    return false;
  }

  m_chain = chain;
  INFO_LOG_FMT(VIDEO, "Librashader: Direct3D 11 filter chain created");
  return true;
}

bool DXLibrashaderRuntime::EnsureInputSRV(const DXTexture* in_tex)
{
  ID3D11Texture2D* const resource = in_tex->GetD3DTexture();
  if (m_input_srv && m_input_srv_source.Get() == resource)
    return true;

  // One slot, so the previous view goes before the replacement is made rather than being left to
  // the assignment. If creation then fails the cache is simply empty and the next frame retries.
  m_input_srv.Reset();
  m_input_srv_source.Reset();

  // The format has to be Dolphin's own SRV format for this texture, not something derived from the
  // resource description -- render targets are created typeless, and a different typed format would
  // change what the chain samples. Mip range matches DXTexture::CreateSRV(); a TEXTURE2D view has
  // no array-slice field and addresses the first slice implicitly, which is the slice the chain
  // wants.
  const TextureConfig& config = in_tex->GetConfig();
  const CD3D11_SHADER_RESOURCE_VIEW_DESC desc(
      D3D11_SRV_DIMENSION_TEXTURE2D, D3DCommon::GetSRVFormatForAbstractFormat(config.format), 0,
      config.levels);

  ComPtr<ID3D11ShaderResourceView> srv;
  const HRESULT hr = D3D::device->CreateShaderResourceView(resource, &desc, &srv);
  if (FAILED(hr))
  {
    ERROR_LOG_FMT(VIDEO,
                  "Librashader: failed to create a non-array input SRV for a {}x{}x{} source: {}",
                  config.width, config.height, config.layers, DX11HRWrap(hr));
    return false;
  }

  m_input_srv = std::move(srv);
  m_input_srv_source = resource;
  return true;
}

void DXLibrashaderRuntime::DestroyChain()
{
  // Release the cached input view and its retained source unconditionally, before the early return:
  // only RunFrame() creates them and it returns early without a chain, so nothing could be stranded
  // today, but doing it up front removes the reachability argument rather than relying on it.
  m_input_srv.Reset();
  m_input_srv_source.Reset();

  if (m_chain == nullptr)
    return;

  auto chain = static_cast<libra_d3d11_filter_chain_t>(m_chain);
  Functions().free(&chain);
  m_chain = nullptr;
}

bool DXLibrashaderRuntime::HasChain() const
{
  return m_chain != nullptr;
}

bool DXLibrashaderRuntime::RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                                    u64 frame_count)
{
  if (m_chain == nullptr)
    return false;

  const auto* in_tex = static_cast<const DXTexture*>(source);
  auto* out_fb = static_cast<DXFramebuffer*>(target);

  // Not in_tex->GetD3DSRV(): that one is TEXTURE2DARRAY, and librashader's HLSL declares a
  // non-array Texture2D. Reading an array view through a non-array declaration is undefined by the
  // D3D spec even though every driver tested tolerates it, so the chain gets a view of its own.
  if (!EnsureInputSRV(in_tex))
    return false;

  ID3D11ShaderResourceView* srv = m_input_srv.Get();
  ID3D11RenderTargetView* rtv = out_fb->GetRTVArray()[0];

  libra_viewport_t vp{0.0f, 0.0f, static_cast<u32>(target->GetWidth()),
                      static_cast<u32>(target->GetHeight())};

  // The chain binds `target` as a render target and `source` as a shader resource, and both may
  // still be bound the other way round on the context: on the downscale path the native-source
  // framebuffer's RTV is often still live (LibrashaderPostProcessing restores the caller's
  // framebuffer, but that only marks our state pending -- Apply() has not run), and on the
  // intermediate path last frame's passthrough blit left the chain target bound as an SRV in slot
  // 0. D3D11 resolves such a hazard by silently dropping one binding, so clear both up front.
  ID3D11ShaderResourceView* const null_srvs[VideoCommon::MAX_PIXEL_SHADER_SAMPLERS] = {};
  D3D::context->PSSetShaderResources(0, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS, null_srvs);
  D3D::context->OMSetRenderTargets(0, nullptr, nullptr);

  auto chain = static_cast<libra_d3d11_filter_chain_t>(m_chain);
  const libra_error_t err =
      Functions().frame(&chain, D3D::context.Get(), frame_count, srv, rtv, &vp, nullptr, nullptr);

  // librashader's D3D11StateSaveGuard restores only the rasterizer and blend state. Topology, input
  // layout, vertex buffer, VS/PS and their constant buffers, samplers, shader resources, render
  // targets and the viewport are left as the chain used them.
  D3D::stateman->InvalidateCachedState();

  return !CheckError(err, "d3d11_filter_chain_frame");
}

void DXLibrashaderRuntime::SetParameter(const char* name, float value)
{
  if (m_chain == nullptr)
    return;

  auto chain = static_cast<libra_d3d11_filter_chain_t>(m_chain);
  CheckError(Functions().set_param(&chain, name, value), "d3d11_filter_chain_set_param");
}
}  // namespace DX11
