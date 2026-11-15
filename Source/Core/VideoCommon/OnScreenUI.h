// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/BigPicture/InGameMenu.h"
#include "VideoCommon/BigPicture/Navigation.h"
#include "VideoCommon/OnScreenUIKeyMap.h"

class NativeVertexFormat;
class AbstractTexture;
class AbstractPipeline;
struct ImTextureData;
struct ImFont;

namespace VideoCommon
{
// OnScreenUI handles all the ImGui rendering.
class OnScreenUI
{
public:
  OnScreenUI() = default;
  ~OnScreenUI();

  // ImGui initialization depends on being able to create textures and pipelines, so do it last.
  bool Initialize(u32 width, u32 height, float scale);

  // Returns a lock for the ImGui mutex, enabling data structures to be modified from outside.
  // Use with care, only non-drawing functions should be called from outside the video thread,
  // as the drawing is tied to a "frame".
  std::unique_lock<std::mutex> GetImGuiLock();

  bool IsReady() { return m_ready; }

  // Sets up ImGui state for the next frame.
  // This function itself acquires the ImGui lock, so it should not be held.
  void BeginImGuiFrame(u32 width, u32 height);

  // Same as above but without locking the ImGui lock.
  void BeginImGuiFrameUnlocked(u32 width, u32 height);

  // Renders ImGui windows to the currently-bound framebuffer.
  // Should be called with the ImGui lock held.
  void DrawImGui();

  // Recompiles ImGui pipeline - call when stereo mode changes.
  bool RecompileImGuiPipeline();

  void SetScale(float backbuffer_scale);

  // Complete the frame and update dynamic font textures. UI-only sessions must not draw overlays
  // that read emulation state. The caller must own the graphics context, as for DrawImGui().
  void Finalize(bool draw_emulation_overlays = true);

  // Receive keyboard and mouse from QT
  void SetKeyMap(const DolphinKeyMap& key_map);
  void SetKey(u32 key, bool is_down, const char* chars);
  void SetMousePos(float x, float y);
  void SetMousePress(u32 button_mask);

private:
  void DrawDebugText();
  void DrawChallengesAndLeaderboards();
  void UpdateImguiTexture(ImTextureData* tex);
  void ProcessInput();
  void DrawNavigation();
  std::string GetNavigationPrompts(const BigPicture::MenuPage& page,
                                   const ::BigPicture::NavigationText& text, bool text_input) const;
  ImTextureID GetNavigationTexture(const ::BigPicture::Snapshot& snapshot, const std::string& path);
  ImTextureID GetLibraryCover(const ::BigPicture::LibrarySnapshot& library,
                              const std::string& path);
  struct CoverTexture
  {
    std::unique_ptr<AbstractTexture> texture;
    u64 last_frame = 0;
    u64 last_request_us = 0;
  };
  std::map<std::string, CoverTexture> m_cover_textures;
  struct ImageTexture
  {
    std::shared_ptr<const ::BigPicture::LibraryImage> image;
    std::unique_ptr<AbstractTexture> texture;
    u64 last_frame = 0;
  };
  std::array<ImageTexture, ::BigPicture::STATE_SLOT_COUNT> m_state_textures;
  std::shared_ptr<const ::BigPicture::AchievementsSnapshot> m_achievement_snapshot;
  std::map<std::string, std::shared_ptr<const ::BigPicture::LibraryImage>> m_achievement_images;
  std::map<std::string, ImageTexture> m_achievement_textures;
  u64 m_cover_revision = 0;
  u64 m_cover_frame = 0;
  unsigned m_cover_uploads = 0;
  u64 m_input_epoch = 0;
  u64 m_navigation_generation = 0;
  ::BigPicture::NavigationStatus m_navigation;
  BigPicture::NavigationToolkit m_navigation_toolkit;
  // Built menus are pure functions of these immutable inputs; rebuild only when one changes.
  std::shared_ptr<const ::BigPicture::Snapshot> m_menu_snapshot;
  std::shared_ptr<const ::BigPicture::NavigationText> m_menu_text;
  ::BigPicture::NavigationOptions m_menu_options;
  BigPicture::InGameMenu m_menu;
  BigPicture::PendingMenuHold m_pending_menu;
  std::map<std::string, std::unique_ptr<AbstractTexture>> m_home_textures;
  std::shared_ptr<const ::BigPicture::LibraryImage> m_home_avatar;
  BigPicture::NavigationInput m_navigation_input;
  ImFont* m_menu_font = nullptr;

  // ImGui resources.
  std::unique_ptr<NativeVertexFormat> m_imgui_vertex_format;
  std::vector<std::unique_ptr<AbstractTexture>> m_imgui_textures;
  std::unique_ptr<AbstractPipeline> m_imgui_pipeline;
  std::map<u32, int> m_dolphin_to_imgui_map;
  std::mutex m_imgui_mutex;
  u64 m_imgui_last_frame_time = 0;

  u32 m_backbuffer_width = 1;
  u32 m_backbuffer_height = 1;
  float m_backbuffer_scale = 1.0;

#ifdef USE_RETRO_ACHIEVEMENTS
  std::map<int, std::unique_ptr<AbstractTexture>, std::less<>> m_challenge_texture_map;
#endif  // USE_RETRO_ACHIEVEMENTS

  bool m_ready = false;
};
}  // namespace VideoCommon
