// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"

#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"

namespace DX12
{
class DXTexture;

// Drives librashader's Direct3D 12 runtime. librashader_ld.h is unused; the libra_d3d12_*
// entry points are resolved through VideoCommon::Librashader::GetSymbol in the .cpp, which is
// the only translation unit that defines LIBRA_RUNTIME_D3D12.
class DX12LibrashaderRuntime final : public VideoCommon::LibrashaderRuntime
{
public:
  DX12LibrashaderRuntime();
  ~DX12LibrashaderRuntime() override;

  bool IsSupported() const override;
  bool CreateChain(libra_shader_preset_t preset) override;
  void DestroyChain() override;
  bool HasChain() const override;
  bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                u64 frame_count) override;
  void SetParameter(const char* name, float value) override;

  // DiscardPendingTargetClear() is deliberately not overridden: Dolphin's D3D12 backend has no
  // deferred clear to drop. SetAndDiscardFramebuffer() only transitions render targets and
  // DXFramebuffer exposes nothing but the immediate ClearRenderTargets()/ClearDepth(), so there is
  // no clear-pending flag anywhere -- unlike PCSX2's GSTexture12::CommitClear.

private:
  // Points m_input_srv_descriptor at a non-array view of `in_tex`'s resource, creating it if the
  // source identity changed. False means no descriptor exists and the frame must be skipped.
  bool EnsureInputDescriptor(const DXTexture* in_tex);

  // Returns the cached descriptor to the persistent heap (fence-deferred) and drops the source
  // reference.
  void ReleaseInputDescriptor();

  // Opaque here so the header does not need a LIBRA_RUNTIME_* macro.
  void* m_chain = nullptr;

  // One-slot cache of the non-array input view handed to the chain, and the resource it was created
  // from. librashader's generated HLSL declares its input as a non-array Texture2D, while every
  // Dolphin chain input is a Texture_2DArray whose own SRV descriptor is TEXTURE2DARRAY, so the
  // descriptor cannot be shared. The source is held by ComPtr to make the identity check sound: the
  // adapter does not own the source texture, and a bare pointer would report a false hit for a
  // freed resource whose address was reused.
  ComPtr<ID3D12Resource> m_input_srv_source;
  DescriptorHandle m_input_srv_descriptor = {};
};
}  // namespace DX12
