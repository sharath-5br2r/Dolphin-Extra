// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This must stay above every #include in this file. LibrashaderLoader.h includes <librashader.h>
// deliberately without any LIBRA_RUNTIME_* macro, so it stays cheap for the rest of VideoCommon.
// Whichever translation unit includes it first thereby satisfies librashader.h's include guard, so
// a later `#include <librashader.h>` here would be a silent no-op leaving every libra_d3d12_*
// declaration out -- with no error, because the runtime sections are #ifdef'd, not #error'd.
#define LIBRA_RUNTIME_D3D12
#include <librashader.h>

#include "VideoBackends/D3D12/DX12LibrashaderRuntime.h"

#include "Common/Logging/Log.h"

#include "VideoBackends/D3D12/D3D12Gfx.h"
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoBackends/D3D12/DX12Texture.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace DX12
{
namespace
{
// D3D12 chain entry points, resolved once. Absent symbols leave the pointers null, which
// IsSupported() reports rather than calling through. Only the four the adapter calls are resolved:
// an unused required symbol is one more way for IsSupported() to fail for no reason.
struct D3D12Functions
{
  PFN_libra_d3d12_filter_chain_create create = nullptr;
  PFN_libra_d3d12_filter_chain_frame frame = nullptr;
  PFN_libra_d3d12_filter_chain_set_param set_param = nullptr;
  PFN_libra_d3d12_filter_chain_free free = nullptr;

  D3D12Functions()
  {
    using VideoCommon::Librashader::GetSymbol;
    create = reinterpret_cast<PFN_libra_d3d12_filter_chain_create>(
        GetSymbol("libra_d3d12_filter_chain_create"));
    frame = reinterpret_cast<PFN_libra_d3d12_filter_chain_frame>(
        GetSymbol("libra_d3d12_filter_chain_frame"));
    set_param = reinterpret_cast<PFN_libra_d3d12_filter_chain_set_param>(
        GetSymbol("libra_d3d12_filter_chain_set_param"));
    free = reinterpret_cast<PFN_libra_d3d12_filter_chain_free>(
        GetSymbol("libra_d3d12_filter_chain_free"));
  }

  bool Complete() const { return create && frame && set_param && free; }
};

const D3D12Functions& Functions()
{
  static const D3D12Functions s_functions;
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

DX12LibrashaderRuntime::DX12LibrashaderRuntime() = default;

DX12LibrashaderRuntime::~DX12LibrashaderRuntime()
{
  DestroyChain();
}

bool DX12LibrashaderRuntime::IsSupported() const
{
  return VideoCommon::Librashader::GetAvailability().available && Functions().Complete();
}

bool DX12LibrashaderRuntime::CreateChain(libra_shader_preset_t preset)
{
  // force_hlsl_pipeline would trade librashader's SPIR-V pipeline for an HLSL one, which its own
  // header warns reduces shader compatibility. frames_in_flight 0 selects librashader's default of
  // three, which matches Dolphin's own NUM_COMMAND_LISTS (3): librashader recycles a pass's
  // per-frame objects three of its frames later, by which point the command list that recorded the
  // older frame has been waited on, because Dolphin waits on a command list's fence before reusing
  // its slot.
  filter_chain_d3d12_opt_t options = {};
  options.version = LIBRASHADER_CURRENT_VERSION;
  options.force_hlsl_pipeline = false;
  options.force_no_mipmaps = false;
  options.disable_cache = false;
  options.frames_in_flight = 0;

  libra_d3d12_filter_chain_t chain = nullptr;
  // create() invalidates `preset` on success and on failure alike, so it is never freed here. It
  // also does its own LUT uploads on a command queue of its own and blocks until they complete --
  // that is what distinguishes it from _create_deferred, which hands that work to the caller's
  // command list -- so unlike PCSX2 there is nothing to flush of ours first.
  //
  // Success is decided by CheckError, i.e. by the error handle, not by whether its description
  // renders non-empty: an error with an empty message read as success left m_chain null while the
  // log said the chain had been created, so the user got silent passthrough with no error line.
  if (CheckError(Functions().create(&preset, g_dx_context->GetDevice(), &options, &chain),
                 "d3d12_filter_chain_create"))
  {
    m_chain = nullptr;
    return false;
  }

  m_chain = chain;
  INFO_LOG_FMT(VIDEO, "Librashader: Direct3D 12 filter chain created");
  return true;
}

bool DX12LibrashaderRuntime::EnsureInputDescriptor(const DXTexture* in_tex)
{
  ID3D12Resource* const resource = in_tex->GetResource();
  if (m_input_srv_descriptor && m_input_srv_source.Get() == resource)
    return true;

  // One slot: the previous descriptor is released before the replacement is allocated. The heap is
  // a finite pool (MAX_SRVS), so an insert-only cache would exhaust it rather than merely grow.
  ReleaseInputDescriptor();

  DescriptorHandle descriptor = {};
  if (!g_dx_context->GetDescriptorHeapManager().Allocate(&descriptor))
  {
    ERROR_LOG_FMT(VIDEO, "Librashader: failed to allocate a non-array input SRV descriptor");
    return false;
  }

  // The format has to be Dolphin's own SRV format for this texture, not something derived from the
  // resource description -- render targets are created typeless, and a different typed format would
  // change what the chain samples. Mip range matches DXTexture::CreateSRVDescriptor(); a TEXTURE2D
  // view has no array-slice field and addresses the first slice implicitly, which is the slice the
  // chain wants.
  const TextureConfig& config = in_tex->GetConfig();
  D3D12_SHADER_RESOURCE_VIEW_DESC desc = {D3DCommon::GetSRVFormatForAbstractFormat(config.format),
                                          D3D12_SRV_DIMENSION_TEXTURE2D,
                                          D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
  desc.Texture2D.MostDetailedMip = 0;
  desc.Texture2D.MipLevels = config.levels;
  g_dx_context->GetDevice()->CreateShaderResourceView(resource, &desc, descriptor.cpu_handle);

  m_input_srv_descriptor = descriptor;
  m_input_srv_source = resource;
  return true;
}

void DX12LibrashaderRuntime::ReleaseInputDescriptor()
{
  // DescriptorHeapManager, not DescriptorAllocator: the former is the persistent bitset allocator
  // that DXTexture itself uses, while the latter is a per-frame linear allocator whose Reset()
  // would recycle the descriptor out from under the chain.
  //
  // Deferred to the current command list's fence rather than freed at once, which is what
  // ~DXTexture does with its own SRV descriptor out of the same heap manager -- and what the
  // descriptor this replaced (DXTexture's) was covered by. The slot itself is never read by the GPU
  // (the heap is created D3D12_DESCRIPTOR_HEAP_FLAG_NONE, so it is not shader-visible and can only
  // be copied from on the CPU), but librashader is a prebuilt binary whose header states a lifetime
  // rule for resource pointers and says nothing about descriptor handles, so matching the backend's
  // own idiom is preferable to reasoning about when it stops reading the handle. Still bounded: one
  // slot, deferred by at most NUM_COMMAND_LISTS frames. A descriptor deferred from DestroyChain()
  // during shutdown is never drained, which is harmless -- the heap is destroyed moments later by
  // DXContext::Destroy(), exactly as for every texture still alive at that point.
  if (m_input_srv_descriptor)
  {
    g_dx_context->DeferDescriptorDestruction(g_dx_context->GetDescriptorHeapManager(),
                                             m_input_srv_descriptor.index);
  }

  m_input_srv_descriptor = {};
  m_input_srv_source.Reset();
}

void DX12LibrashaderRuntime::DestroyChain()
{
  // Release the cached descriptor and its retained source unconditionally, before the early return:
  // only RunFrame() creates them and it returns early without a chain, so nothing could be stranded
  // today, but doing it up front removes the reachability argument rather than relying on it.
  ReleaseInputDescriptor();

  if (m_chain == nullptr)
    return;

  auto chain = static_cast<libra_d3d12_filter_chain_t>(m_chain);
  Functions().free(&chain);
  m_chain = nullptr;
}

bool DX12LibrashaderRuntime::HasChain() const
{
  return m_chain != nullptr;
}

bool DX12LibrashaderRuntime::RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                                      u64 frame_count)
{
  if (m_chain == nullptr)
    return false;

  const auto* in_tex = static_cast<const DXTexture*>(source);
  auto* out_fb = static_cast<DXFramebuffer*>(target);
  auto* out_tex = static_cast<DXTexture*>(out_fb->GetColorAttachment());
  if (out_tex == nullptr || out_fb->GetRTVDescriptorCount() == 0)
    return false;

  // Not in_tex->GetSRVDescriptor(): that one is TEXTURE2DARRAY, and librashader's HLSL declares a
  // non-array Texture2D. Reading an array view through a non-array declaration is undefined by the
  // D3D spec even though every driver tested tolerates it, so the chain gets a descriptor of its
  // own. Done before the barriers so a failure costs nothing.
  if (!EnsureInputDescriptor(in_tex))
    return false;

  // librashader requires the source in PIXEL_SHADER_RESOURCE and the target in RENDER_TARGET, and
  // records no closing barrier: its header states the output "will remain in
  // D3D12_RESOURCE_STATE_RENDER_TARGET after all shader passes". Because that is the state we asked
  // for here, DXTexture's own tracking already describes what the chain leaves behind, so there is
  // nothing to reconcile the way the Vulkan runtime has to override its image layout. Both
  // transitions are no-ops when the resource is already in the requested state. (`source` is const,
  // which is fine: TransitionToState is a const member function over a mutable state field.)
  //
  // Nothing else needs closing first. PCSX2 ends a render pass and commits both textures' deferred
  // clears at this point; Dolphin's D3D12 backend has neither -- BindFramebuffer() is a bare
  // OMSetRenderTargets, and clears are issued immediately.
  in_tex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  out_tex->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

  // The chain is handed our own views, not the bare resources. IMAGE_TYPE_RESOURCE, where the chain
  // creates and owns the descriptors itself, cannot work here: it derives the view formats from
  // ID3D12Resource::GetDesc(), and every Dolphin render target is created with a *typeless* DXGI
  // format -- DXTexture::Create() passes config.IsRenderTarget() as
  // GetDXGIFormatForAbstractFormat's `typeless` argument, so the typed format exists only in the
  // views. A view cannot be created with a typeless format, so that path made librashader issue
  // invalid view creations, which the driver answered by removing the device -- the debug layer's
  // account of it was "CreateRenderTargetView: The Format (0x17, R10G10B10A2_TYPELESS) is invalid,
  // when creating a View" followed by "RemoveDevice: ... (DXGI_ERROR_INVALID_CALL)".
  // PCSX2 does use IMAGE_TYPE_RESOURCE, because GSTexture12 creates typed resources; Dolphin's
  // typeless-resource-plus-typed-view architecture wins here. It also puts D3D12 on the same
  // footing as the D3D11 runtime, whose frame entry point only ever takes an SRV and an RTV.
  //
  // Passing a CPU handle out of Dolphin's shadow heap is what librashader expects: the heap is
  // created with D3D12_DESCRIPTOR_HEAP_FLAG_NONE, and the chain copies the descriptor into its own
  // shader-visible heap, which is legal only from a non-shader-visible source heap.
  libra_image_d3d12_t in = {};
  in.image_type = LIBRA_D3D12_IMAGE_TYPE_SOURCE_IMAGE;
  in.handle.source.descriptor = m_input_srv_descriptor.cpu_handle;
  in.handle.source.resource = in_tex->GetResource();

  // The output carries its own format and size because an RTV alone describes neither. RTV zero is
  // the color attachment's, created with exactly this format by DXFramebuffer::CreateRTVDescriptor.
  libra_image_d3d12_t out = {};
  out.image_type = LIBRA_D3D12_IMAGE_TYPE_OUTPUT_IMAGE;
  out.handle.output.descriptor = out_fb->GetRTVDescriptorArray()[0];
  out.handle.output.format =
      D3DCommon::GetRTVFormatForAbstractFormat(out_fb->GetColorFormat(), false);
  out.handle.output.width = out_fb->GetWidth();
  out.handle.output.height = out_fb->GetHeight();

  libra_viewport_t vp{0.0f, 0.0f, static_cast<u32>(target->GetWidth()),
                      static_cast<u32>(target->GetHeight())};

  auto chain = static_cast<libra_d3d12_filter_chain_t>(m_chain);
  const libra_error_t err =
      Functions().frame(&chain, g_dx_context->GetCommandList(), static_cast<size_t>(frame_count),
                        in, out, &vp, nullptr, nullptr);

  // The chain recorded its own root signature, pipeline state, descriptor heaps and viewport onto
  // our command list and restored none of them, so nothing Gfx believes is bound still is. That is
  // exactly the situation ExecuteCommandList() faces, and one DirtyState_All is how it answers it.
  // Two things D3D11 and PCSX2 need here have no counterpart: there is no second "what the command
  // list currently has" mirror to reset, because ApplyState() tests one dirty bit per binding and
  // then issues from m_state unconditionally; and DirtyState_All includes
  // DirtyState_DescriptorHeaps, which SetDescriptorHeaps() acts on at the top of both ApplyState()
  // and DispatchComputeShader(), so PCSX2's manual heap rebind -- which it needs only because it
  // binds heaps at command list reset and nowhere else -- would achieve nothing here.
  Gfx::GetInstance()->InvalidateCachedState();

  return !CheckError(err, "d3d12_filter_chain_frame");
}

void DX12LibrashaderRuntime::SetParameter(const char* name, float value)
{
  if (m_chain == nullptr)
    return;

  auto chain = static_cast<libra_d3d12_filter_chain_t>(m_chain);
  CheckError(Functions().set_param(&chain, name, value), "d3d12_filter_chain_set_param");
}
}  // namespace DX12
