// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This must stay above every #include in this file, including the ones that look unrelated.
// LibrashaderLoader.h includes <librashader.h> deliberately without any LIBRA_RUNTIME_* macro, so
// it stays cheap for the rest of VideoCommon. Whichever translation unit includes it first thereby
// satisfies librashader.h's include guard, so a later `#include <librashader.h>` here would be a
// silent no-op leaving every libra_vk_* declaration out -- with no error, because the runtime
// sections are #ifdef'd, not #error'd. Defining the macro first costs nothing (it pulls in no
// header of its own) and is the only ordering that survives an include reshuffle.
#define LIBRA_RUNTIME_VULKAN

#include "VideoBackends/Vulkan/VKLibrashaderRuntime.h"

#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"

#include "Core/Config/GraphicsSettings.h"

// VulkanContext.h transitively includes VulkanLoader.h, which includes <vulkan/vulkan.h> with
// VK_NO_PROTOTYPES and declares Dolphin's function-pointer globals (including
// ::vkGetInstanceProcAddr). It MUST come before LibrashaderLoader.h: with LIBRA_RUNTIME_VULKAN
// defined above, that header's <librashader.h> re-includes <vulkan/vulkan.h>, and only the guard
// set here suppresses the conflicting prototype declarations. VKLibrashaderRuntime.h above includes
// VulkanLoader.h for exactly this reason before it reaches LibrashaderRuntime.h; the Vulkan include
// block sorting ahead of the VideoCommon one keeps that true for anything added here later.
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/PostProcessing/ChainOutputPolicy.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace Vulkan
{
namespace
{
// Vulkan chain entry points, resolved once. Absent symbols leave the pointers null, which
// IsSupported() reports rather than calling through -- unlike librashader_ld.h, which substituted
// no-op stubs and so turned a truncated library into post-processing that silently did nothing.
struct VulkanFunctions
{
  PFN_libra_vk_filter_chain_create create = nullptr;
  PFN_libra_vk_filter_chain_frame frame = nullptr;
  PFN_libra_vk_filter_chain_set_param set_param = nullptr;
  PFN_libra_vk_filter_chain_free free = nullptr;

  VulkanFunctions()
  {
    using VideoCommon::Librashader::GetSymbol;
    create = reinterpret_cast<PFN_libra_vk_filter_chain_create>(
        GetSymbol("libra_vk_filter_chain_create"));
    frame =
        reinterpret_cast<PFN_libra_vk_filter_chain_frame>(GetSymbol("libra_vk_filter_chain_frame"));
    set_param = reinterpret_cast<PFN_libra_vk_filter_chain_set_param>(
        GetSymbol("libra_vk_filter_chain_set_param"));
    free =
        reinterpret_cast<PFN_libra_vk_filter_chain_free>(GetSymbol("libra_vk_filter_chain_free"));
  }

  bool Complete() const { return create && frame && set_param && free; }
};

const VulkanFunctions& Functions()
{
  static const VulkanFunctions s_functions;
  return s_functions;
}

// Logs a librashader error (if any) and frees it. Returns true if an error was present. The message
// now comes from librashader itself rather than a bare errno, which is strictly more informative.
bool CheckError(libra_error_t error, const char* context)
{
  // Tested against the handle rather than against the description: an error whose message happened
  // to come back empty would otherwise be reported as success, after having already been freed.
  if (error == nullptr)
    return false;

  ERROR_LOG_FMT(VIDEO, "Librashader: {} failed: {}", context,
                VideoCommon::Librashader::DescribeAndFreeError(error));
  return true;
}
}  // namespace

VKLibrashaderRuntime::VKLibrashaderRuntime() = default;

VKLibrashaderRuntime::~VKLibrashaderRuntime()
{
  DestroyChain();
}

bool VKLibrashaderRuntime::IsSupported() const
{
  return VideoCommon::Librashader::GetAvailability().available && Functions().Complete();
}

bool VKLibrashaderRuntime::CreateChain(libra_shader_preset_t preset)
{
  // libra_device_vk_t = { physical_device, instance, device, queue, entry }. `entry` is the
  // vkGetInstanceProcAddr loader; Dolphin's global (declared in VulkanLoader.h) already holds the
  // resolved pointer, so it is passed by value.
  libra_device_vk_t device = {};
  device.physical_device = g_vulkan_context->GetPhysicalDevice();
  device.instance = g_vulkan_context->GetVulkanInstance();
  device.device = g_vulkan_context->GetDevice();
  device.queue = g_vulkan_context->GetGraphicsQueue();
  device.entry = ::vkGetInstanceProcAddr;

  // Chain options. frames_in_flight 0 selects librashader's default of three, which is >= Dolphin's
  // NUM_FRAMES_IN_FLIGHT (2): a pass's per-frame objects are never recycled while a submit that
  // still references them can be in flight. Dynamic rendering (when the device has it and
  // librashader can resolve vkCmdBeginRendering) avoids one VkFramebuffer per pass per frame; in
  // render-pass mode librashader creates those objects every frame.
  filter_chain_vk_opt_t options = {};
  options.version = LIBRASHADER_CURRENT_VERSION;
  options.frames_in_flight = 0;
  options.force_no_mipmaps = false;
  options.use_dynamic_rendering = VideoCommon::ChooseDynamicRendering(
      g_vulkan_context->SupportsDynamicRendering() &&
          Config::Get(Config::GFX_LIBRASHADER_DYNAMIC_RENDERING),
      vkGetDeviceProcAddr(g_vulkan_context->GetDevice(), "vkCmdBeginRendering") != nullptr);
  options.disable_cache = false;

  // vk_filter_chain_create invalidates the preset handle regardless of success or failure (see the
  // header: "the shader preset is immediately invalidated"), so it must not be freed on either path
  // -- doing so would be a double-free. On error, leave m_chain null (passthrough).
  if (CheckError(Functions().create(&preset, device, &options, &m_chain), "vk_filter_chain_create"))
  {
    m_chain = nullptr;
    return false;
  }

  INFO_LOG_FMT(VIDEO, "Librashader: Vulkan filter chain created (dynamic rendering {})",
               options.use_dynamic_rendering ? "on" : "off");
  return true;
}

void VKLibrashaderRuntime::DestroyChain()
{
  if (m_chain == nullptr)
    return;

  Functions().free(&m_chain);
  m_chain = nullptr;
}

bool VKLibrashaderRuntime::RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                                    u64 frame_count)
{
  if (m_chain == nullptr)
    return false;

  const auto* in_tex = static_cast<const VKTexture*>(source);
  auto* out_tex = static_cast<VKTexture*>(target->GetColorAttachment());
  if (out_tex == nullptr)
    return false;

  libra_image_vk_t in{in_tex->GetImage(), in_tex->GetVkFormat(), in_tex->GetWidth(),
                      in_tex->GetHeight()};
  libra_image_vk_t out{out_tex->GetImage(), out_tex->GetVkFormat(), out_tex->GetWidth(),
                       out_tex->GetHeight()};
  libra_viewport_t vp{0.0f, 0.0f, out_tex->GetWidth(), out_tex->GetHeight()};

  // libra_vk_filter_chain_frame() must NOT be recorded inside a render pass, and the header
  // requires the input in SHADER_READ_ONLY_OPTIMAL and the output in COLOR_ATTACHMENT_OPTIMAL.
  // EndRenderPass() covers a pass the emulated frame left open, which is the case whenever no
  // native-source downscale ran: on the downscale path the caller's framebuffer restore has
  // already ended the pass (VKGfx::BindFramebuffer ends it) and this is a no-op. (`source` is
  // const, which is fine because VKTexture::TransitionToLayout is a const member function. Both
  // transitions are no-ops when the image already has the requested layout.) librashader creates no
  // barrier after its final pass, so on success the target is left in COLOR_ATTACHMENT_OPTIMAL and
  // Dolphin's own tracking is reconciled to match.
  //
  // Image layouts and render passes are not the whole of what the chain disturbs: it also binds
  // its own pipeline, descriptor sets and vertex buffer onto our command buffer and restores none
  // of them, so the tracker's cached bindings become a lie too. That is reconciled below, after
  // the recording.
  StateTracker::GetInstance()->EndRenderPass();
  const VkCommandBuffer cmd = g_command_buffer_mgr->GetCurrentCommandBuffer();
  in_tex->TransitionToLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  out_tex->TransitionToLayout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  const libra_error_t err =
      Functions().frame(&m_chain, cmd, frame_count, in, out, &vp, nullptr, nullptr);
  out_tex->OverrideImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // The layout override above reconciles the image; this reconciles the bindings. Every
  // StateTracker setter early-outs when what it is asked for equals what it recorded, so without
  // this the next draw in this command buffer emits neither vkCmdBindPipeline nor
  // vkCmdBindDescriptorSets and runs against librashader's final-pass state.
  // InvalidateCachedState() clears the descriptor-set handles and raises exactly the flags the
  // chain invalidates (pipeline, descriptor sets, viewport, scissor, vertex and index buffer).
  // Mono presents hide the need because VKGfx invalidates on every submit; a Side-by-Side or
  // Top-and-Bottom present blits twice into one command buffer.
  StateTracker::GetInstance()->InvalidateCachedState();

  return !CheckError(err, "vk_filter_chain_frame");
}

void VKLibrashaderRuntime::SetParameter(const char* name, float value)
{
  if (m_chain == nullptr)
    return;

  CheckError(Functions().set_param(&m_chain, name, value), "vk_filter_chain_set_param");
}

void VKLibrashaderRuntime::DiscardPendingTargetClear()
{
  StateTracker::GetInstance()->DiscardPendingClear();
}
}  // namespace Vulkan
