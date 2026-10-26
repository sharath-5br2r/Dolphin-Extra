// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "Common/BigPicture/Navigation.h"
#include "VideoCommon/BigPicture/Icons.h"

namespace VideoCommon::BigPicture
{
enum class MenuPresentation
{
  List,
  Landing,
  Pause,
  Library,
};

struct MenuItem
{
  std::string id;
  std::string label;
  std::string detail;
  bool enabled = true;
  std::string image_key;
  bool active = false;
  MenuIcon icon = MenuIcon::None;
  std::string preview_detail;
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
  std::vector<MenuItem> toolbar;
  bool artwork = false;
  bool compact = false;
  bool reset_on_revision = false;
  MenuPresentation presentation = MenuPresentation::List;
  // Header metadata is separate from the scrollable explanation/status text.
  MenuItem context;
  bool overlay = false;
  std::string theme = "Dark";
  std::string account_name;
};

struct MenuResult
{
  std::optional<std::string> activated;
  bool back = false;
  std::string focused;
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
                  const std::string& prompts, ImFont* font = nullptr,
                  const std::function<ImTextureID(const std::string&)>& image = {});
  void InvalidateFocus() { m_current_page.clear(); }
  void Reset();
  FocusMemory GetMemory(const std::string& page) const;

private:
  MenuResult DrawLanding(const MenuPage& page, ::BigPicture::NavigationOptions options,
                         const std::string& prompts, ImFont* font,
                         const std::function<ImTextureID(const std::string&)>& image);
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

// The most recently pressed input family owns navigation. Release the previous family's
// held keys when switching, so a missed release or held stick cannot mask the new input.
class NavigationInput
{
public:
  void SetSource(bool keyboard);
  void KeyEvent(ImGuiKey key, bool down, bool keyboard);
  void Reset() { m_held.clear(); }
  bool IsKeyboard() const { return m_keyboard; }

private:
  bool m_keyboard = true;
  std::vector<ImGuiKey> m_held;
};

std::string GetBindingGlyph(std::string_view binding, std::string_view device,
                            std::string_view source = {});
std::string GetControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                 const ::BigPicture::NavigationText& text, bool home);
}  // namespace VideoCommon::BigPicture
