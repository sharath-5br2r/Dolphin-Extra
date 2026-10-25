// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "Common/BigPicture/Navigation.h"

namespace VideoCommon::BigPicture
{
struct MenuItem
{
  std::string id;
  std::string label;
  std::string detail;
  bool enabled = true;
};

struct MenuPage
{
  std::string id;
  std::string title;
  std::string description;
  std::vector<MenuItem> items;
  std::string default_focus;
  int columns = 1;
  bool dialog = false;
  std::uint64_t focus_revision = 0;
};

struct MenuResult
{
  std::optional<std::string> activated;
  bool back = false;
};

struct FocusMemory
{
  std::string item;
  // Logical reference pixels, so a resize/DPI change does not change the retained position.
  float scroll = 0;
};

struct MenuLayout
{
  ImVec2 position;
  ImVec2 size;
  float scale;
};
MenuLayout ComputeMenuLayout(ImVec2 viewport, ::BigPicture::NavigationOptions options,
                             bool dialog = false);

// Graphics-owner only. Stable page/item IDs retain focus and scroll across nested screens.
// This layer returns item IDs; it never calls a frontend or emulator operation.
class NavigationToolkit
{
public:
  MenuResult Draw(const MenuPage& page, ::BigPicture::NavigationOptions options,
                  const std::string& prompts, ImFont* font = nullptr);
  void InvalidateFocus() { m_current_page.clear(); }
  void Reset();
  FocusMemory GetMemory(const std::string& page) const;

private:
  std::string m_current_page;
  std::map<std::string, FocusMemory> m_memory;
  unsigned m_restore_frames = 0;
  float m_restore_scroll = 0;
  float m_layout_width = 0;
  float m_layout_height = 0;
  float m_layout_scale = 0;
  std::uint64_t m_focus_revision = 0;
};

// Native bindings become named gamepad keys on the render owner. Confirm/back swapping is
// resolved here, so ImGui always receives logical Select/Back controls.
ImGuiKey GetNavigationKey(::BigPicture::NavButton button, bool swap_confirm_back);
}  // namespace VideoCommon::BigPicture
