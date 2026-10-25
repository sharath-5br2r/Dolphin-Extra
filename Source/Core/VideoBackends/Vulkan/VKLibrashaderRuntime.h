// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// VulkanLoader.h MUST come before LibrashaderRuntime.h, which pulls in <librashader.h>. When the
// including translation unit has defined LIBRA_RUNTIME_VULKAN (VKLibrashaderRuntime.cpp does, on
// its first line), librashader.h re-includes <vulkan/vulkan.h>, and only the VK_NO_PROTOTYPES that
// VulkanLoader.h sets suppresses the prototype declarations that would conflict with Dolphin's own
// function-pointer globals. Doing it here rather than in the .cpp keeps the header correct for
// every includer instead of only the one that remembers the ordering.
#include "VideoBackends/Vulkan/VulkanLoader.h"

#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"

namespace Vulkan
{
// Vulkan binding for librashader's native filter chain. Owns the chain handle and the libra_vk_*
// entry points; VideoCommon::LibrashaderPostProcessing owns everything else.
class VKLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime
{
public:
  VKLibrashaderRuntime();
  ~VKLibrashaderRuntime() override;

  // Reports whether the librashader shared library was found, its ABI is compatible, and every
  // libra_vk_* entry point resolved. AbstractGfx::CreatePostProcessor() uses it to decide whether
  // to fall back to the built-in engine. Safe to call before any chain exists.
  bool IsSupported() const override;

  bool CreateChain(libra_shader_preset_t preset) override;
  void DestroyChain() override;
  bool HasChain() const override { return m_chain != nullptr; }

  bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                u64 frame_count) override;
  void SetParameter(const char* name, float value) override;
  void DiscardPendingTargetClear() override;

private:
  // librashader Vulkan filter chain; null means "no chain", which LibrashaderPostProcessing renders
  // as a passthrough copy. Spelled as the opaque struct rather than libra_vk_filter_chain_t because
  // that typedef only exists behind LIBRA_RUNTIME_VULKAN, which includers of this header
  // (VKGfx.cpp) do not define.
  _filter_chain_vk* m_chain = nullptr;
};
}  // namespace Vulkan
