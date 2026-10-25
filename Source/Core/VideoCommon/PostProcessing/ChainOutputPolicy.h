// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

namespace VideoCommon
{
// True when the post-processing chain's draw rect is exactly the whole target framebuffer AND that
// framebuffer owns an image. Only then may the chain render straight into it and skip the
// intermediate draw-rect-sized target plus the 1:1 blit.
//
// The rect test: librashader derives OutputSize / FinalViewportSize from the output IMAGE, so for
// any sub-rect (pillarbox, letterbox, stereo half) rendering into the backbuffer would size the
// chain for the wrong extent (the vertical-moire bug the intermediate target exists to fix).
//
// The image test: every runtime is handed the target's own image, and OpenGL's window framebuffer
// has none -- OGLGfx's m_system_framebuffer wraps FBO 0 with a null color attachment, unlike the
// D3D and Vulkan swapchain framebuffers, which wrap a real texture. librashader's GL runtime builds
// an FBO of its own around the output texture it is given and has no way to be pointed at FBO 0, so
// on OpenGL the chain always goes through the intermediate target and is then blitted to the
// screen.
constexpr bool ShouldRenderChainDirectly(const MathUtil::Rectangle<int>& dst, u32 fb_width,
                                         u32 fb_height, bool target_owns_image)
{
  return target_owns_image && dst.left == 0 && dst.top == 0 &&
         dst.right == static_cast<int>(fb_width) && dst.bottom == static_cast<int>(fb_height);
}

// librashader takes its dynamic-rendering path only when the device feature is enabled AND it can
// resolve the core vkCmdBeginRendering entry point; if either is missing it silently falls back to
// render-pass objects (one VkFramebuffer per pass per frame). Request it only when both hold so the
// log line reflects the path actually taken.
constexpr bool ChooseDynamicRendering(bool feature_enabled, bool has_begin_rendering_entry_point)
{
  return feature_enabled && has_begin_rendering_entry_point;
}

// The full truth table, so the rule is enforced at compile time wherever this header is included.
static_assert(ChooseDynamicRendering(true, true));
static_assert(!ChooseDynamicRendering(true, false));
static_assert(!ChooseDynamicRendering(false, true));
static_assert(!ChooseDynamicRendering(false, false));
}  // namespace VideoCommon
