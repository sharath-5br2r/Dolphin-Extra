// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/Constants.h"

namespace Vulkan
{
class VKFramebuffer;
class VKShader;
class VKPipeline;
class VKTexture;
class StreamBuffer;
class VertexFormat;

class StateTracker
{
public:
  StateTracker();
  ~StateTracker();

  static StateTracker* GetInstance();
  static bool CreateInstance();
  static void DestroyInstance();

  VKFramebuffer* GetFramebuffer() const { return m_framebuffer; }
  const VKPipeline* GetPipeline() const { return m_pipeline; }
  void SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset, u32 size);
  void SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type);
  void SetFramebuffer(VKFramebuffer* framebuffer);
  void SetPipeline(const VKPipeline* pipeline);
  void SetComputeShader(const VKShader* shader);
  void SetGXUniformBuffer(u32 index, VkBuffer buffer, u32 offset, u32 size);
  void SetUtilityUniformBuffer(VkBuffer buffer, u32 offset, u32 size);
  void SetTexture(u32 index, VkImageView view);
  void SetSampler(u32 index, VkSampler sampler);
  void SetSSBO(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
  void SetTexelBuffer(u32 index, VkBufferView view);
  void SetImageTexture(u32 index, VkImageView view);

  void UnbindTexture(VkImageView view);

  // Set dirty flags on everything to force re-bind at next draw time.
  void InvalidateCachedState();

  // Ends a render pass if we're currently in one.
  // When Bind() is next called, the pass will be restarted.
  // Calling this function is allowed even if a pass has not begun.
  bool InRenderPass() const { return m_current_render_pass != VK_NULL_HANDLE; }
  void BeginRenderPass();
  void BeginDiscardRenderPass();
  void EndRenderPass();

  // Ends the current render pass if it was a clear render pass.
  void BeginClearRenderPass(const VkRect2D& area, const VkClearValue* clear_values,
                            u32 num_clear_values);
  void EndClearRenderPass();

  // Defers a full-framebuffer clear until the next render pass begins on `framebuffer`, so the
  // clear becomes that pass's loadOp instead of costing a clear-store pass of its own followed by a
  // load pass when the first draw arrives (two full backbuffer round-trips on a tiler). Rendering
  // to other framebuffers in between leaves it pending. Discard it when the first draw is known to
  // overwrite every pixel; a caller that ends the frame with nothing drawn must flush it by
  // beginning (and ending) a render pass on the framebuffer.
  void SetPendingClear(VKFramebuffer* framebuffer, const VkClearValue& color_value,
                       const VkClearValue& depth_value);
  void DiscardPendingClear() { m_pending_clear_framebuffer = nullptr; }
  bool HasPendingClear(const VKFramebuffer* framebuffer) const
  {
    return m_pending_clear_framebuffer != nullptr && m_pending_clear_framebuffer == framebuffer;
  }

  void SetViewport(const VkViewport& viewport);
  void SetScissor(const VkRect2D& scissor);

  // Binds all dirty state to the command buffer.
  // If this returns false, you should not issue the draw.
  bool Bind();

  // Binds all dirty compute state to the command buffer.
  // If this returns false, you should not dispatch the shader.
  bool BindCompute();

  // Returns true if the specified rectangle is inside the current render area (used for clears).
  bool IsWithinRenderArea(s32 x, s32 y, u32 width, u32 height) const;

private:
  // Number of descriptor sets for game draws.
  enum
  {
    NUM_GX_DESCRIPTOR_SETS = 3,
    NUM_UTILITY_DESCRIPTOR_SETS = 2,
    NUM_COMPUTE_DESCRIPTOR_SETS = 1
  };

  enum DIRTY_FLAG : u32
  {
    DIRTY_FLAG_GX_UBOS = (1 << 0),
    DIRTY_FLAG_GX_UBO_OFFSETS = (1 << 1),
    DIRTY_FLAG_GX_SAMPLERS = (1 << 4),
    DIRTY_FLAG_GX_SSBO = (1 << 5),
    DIRTY_FLAG_UTILITY_UBO = (1 << 2),
    DIRTY_FLAG_UTILITY_UBO_OFFSET = (1 << 3),
    DIRTY_FLAG_UTILITY_BINDINGS = (1 << 6),
    DIRTY_FLAG_COMPUTE_BINDINGS = (1 << 7),
    DIRTY_FLAG_VERTEX_BUFFER = (1 << 8),
    DIRTY_FLAG_INDEX_BUFFER = (1 << 9),
    DIRTY_FLAG_VIEWPORT = (1 << 10),
    DIRTY_FLAG_SCISSOR = (1 << 11),
    DIRTY_FLAG_PIPELINE = (1 << 12),
    DIRTY_FLAG_COMPUTE_SHADER = (1 << 13),
    DIRTY_FLAG_DESCRIPTOR_SETS = (1 << 14),
    DIRTY_FLAG_COMPUTE_DESCRIPTOR_SET = (1 << 15),

    DIRTY_FLAG_ALL_DESCRIPTORS = DIRTY_FLAG_GX_UBOS | DIRTY_FLAG_UTILITY_UBO |
                                 DIRTY_FLAG_GX_SAMPLERS | DIRTY_FLAG_GX_SSBO |
                                 DIRTY_FLAG_UTILITY_BINDINGS | DIRTY_FLAG_COMPUTE_BINDINGS
  };

  bool Initialize();

  // Check that the specified viewport is within the render area.
  // If not, ends the render pass if it is a clear render pass.
  bool IsViewportWithinRenderArea() const;

  // Begins the deferred clear pass if one is pending for the current framebuffer. Returns true if
  // a render pass was begun.
  bool BeginPendingClearRenderPass();

  void UpdateDescriptorSet();
  void UpdateGXDescriptorSet();
  void UpdateUtilityDescriptorSet();
  void UpdateComputeDescriptorSet();

  // Which bindings/state has to be updated before the next draw.
  u32 m_dirty_flags = 0;

  // input assembly
  VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
  VkDeviceSize m_vertex_buffer_offset = 0;
  VkBuffer m_index_buffer = VK_NULL_HANDLE;
  VkDeviceSize m_index_buffer_offset = 0;
  VkIndexType m_index_type = VK_INDEX_TYPE_UINT16;

  // pipeline state
  const VKPipeline* m_pipeline = nullptr;
  const VKShader* m_compute_shader = nullptr;

  // shader bindings
  struct
  {
    std::array<VkDescriptorBufferInfo, NUM_UBO_DESCRIPTOR_SET_BINDINGS> gx_ubo_bindings;
    std::array<u32, NUM_UBO_DESCRIPTOR_SET_BINDINGS> gx_ubo_offsets;
    VkDescriptorBufferInfo utility_ubo_binding;
    u32 utility_ubo_offset;
    std::array<VkDescriptorImageInfo, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS> samplers;
    std::array<VkBufferView, NUM_COMPUTE_TEXEL_BUFFERS> texel_buffers;
    VkDescriptorBufferInfo ssbo;
    VkDescriptorBufferInfo gx_uber_vertex_ssbo;
    std::array<VkDescriptorImageInfo, VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS> image_textures;
  } m_bindings = {};
  std::array<VkDescriptorSet, NUM_GX_DESCRIPTOR_SETS> m_gx_descriptor_sets = {};
  std::array<VkDescriptorSet, NUM_UTILITY_DESCRIPTOR_SETS> m_utility_descriptor_sets = {};
  VkDescriptorSet m_compute_descriptor_set = VK_NULL_HANDLE;

  // rasterization
  VkViewport m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  VkRect2D m_scissor = {{0, 0}, {1, 1}};

  // uniform buffers
  std::unique_ptr<VKTexture> m_dummy_texture;
  std::unique_ptr<VKTexture> m_dummy_compute_texture;

  VKFramebuffer* m_framebuffer = nullptr;
  VkRenderPass m_current_render_pass = VK_NULL_HANDLE;
  VkRect2D m_framebuffer_render_area = {};

  // See SetPendingClear(). Compared by identity only; never dereferenced while pending.
  VKFramebuffer* m_pending_clear_framebuffer = nullptr;
  VkClearValue m_pending_clear_color = {};
  VkClearValue m_pending_clear_depth = {};
};
}  // namespace Vulkan
