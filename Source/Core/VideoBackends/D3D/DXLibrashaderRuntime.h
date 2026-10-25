// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoBackends/D3D/D3DBase.h"

#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"

namespace DX11
{
class DXTexture;

// Drives librashader's Direct3D 11 runtime. librashader_ld.h is unused; the libra_d3d11_*
// entry points are resolved through VideoCommon::Librashader::GetSymbol in the .cpp, which is
// the only translation unit that defines LIBRA_RUNTIME_D3D11.
class DXLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime
{
public:
  DXLibrashaderRuntime();
  ~DXLibrashaderRuntime() override;

  bool IsSupported() const override;
  bool CreateChain(libra_shader_preset_t preset) override;
  void DestroyChain() override;
  bool HasChain() const override;
  bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                u64 frame_count) override;
  void SetParameter(const char* name, float value) override;

  // DiscardPendingTargetClear() is deliberately not overridden: Dolphin's D3D11 backend has no
  // deferred clear to drop. SetAndClearFramebuffer() applies the state and issues
  // DXFramebuffer::Clear() immediately (D3DGfx.cpp:228-236), so there is no clear-pending flag
  // anywhere -- unlike PCSX2's GSTexture11 commit-clear.

private:
  // Points m_input_srv at a non-array view of `in_tex`'s resource, creating it if the source
  // identity changed. False means no view exists and the frame must be skipped.
  bool EnsureInputSRV(const DXTexture* in_tex);

  // Opaque here so the header does not need a LIBRA_RUNTIME_* macro.
  void* m_chain = nullptr;

  // One-slot cache of the non-array input view handed to the chain, and the resource it was created
  // from. librashader's generated HLSL declares its input as a non-array Texture2D, while every
  // Dolphin chain input is a Texture_2DArray whose own SRV is TEXTURE2DARRAY, so the view cannot be
  // shared. The source is held by ComPtr to make the identity check sound: the adapter does not own
  // the source texture, and a bare pointer would report a false hit for a freed texture whose
  // address was reused.
  ComPtr<ID3D11Texture2D> m_input_srv_source;
  ComPtr<ID3D11ShaderResourceView> m_input_srv;
};
}  // namespace DX11
