// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Common/BigPicture/Theme.h"

namespace VideoCommon::BigPicture::Style
{
// Independently authored fullscreen controls, measured in 1280 x 720 reference pixels.
// Keep these independent of the diagnostic OSD's font and color preferences.
constexpr float TITLE_SIZE = 22;
constexpr float SUMMARY_SIZE = 14;
constexpr float HEADER_HEIGHT = 48;
// The Back/title strip of full-screen pages, dialogs and pickers.
constexpr float BAR_HEIGHT = 44;
constexpr float FOOTER_HEIGHT = 34;
constexpr float DIALOG_ROUNDING = 8;
constexpr float ROW_HEIGHT = 70;
constexpr float SHORT_ROW_HEIGHT = 46;
constexpr float PADDING = 15;
constexpr float GAP = 10;
constexpr float ROUNDING = 6;
constexpr float ICON_SIZE = 22;
constexpr float ACTION_COLUMN_WIDTH = 500;

struct Palette
{
  ImVec4 background{0.129f, 0.129f, 0.129f, 1};
  ImVec4 header{0.180f, 0.180f, 0.180f, 1};
  ImVec4 panel{0.075f, 0.075f, 0.075f, 1};
  ImVec4 text{1, 1, 1, 1};
  ImVec4 secondary{0.72f, 0.72f, 0.72f, 1};
  ImVec4 hover{0.294f, 0.294f, 0.294f, 1};
  ImVec4 selected{0.051f, 0.278f, 0.631f, 1};
  ImVec4 focus{0.388f, 0.643f, 1, 1};
  ImVec4 header_text{1, 1, 1, 1};
  ImVec4 selected_text{1, 1, 1, 1};
  ImVec4 card_secondary{0.72f, 0.72f, 0.72f, 1};
};
constexpr Palette DARK;

constexpr ImVec4 ColorFromHex(unsigned color)
{
  return {((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f, 1};
}

constexpr Palette MakePalette(unsigned background, unsigned header, unsigned panel, unsigned hover,
                              unsigned selected, unsigned focus, bool light = false,
                              bool light_header = false)
{
  Palette palette;
  palette.background = ColorFromHex(background);
  palette.header = ColorFromHex(header);
  palette.panel = ColorFromHex(panel);
  palette.hover = ColorFromHex(hover);
  palette.selected = ColorFromHex(selected);
  palette.focus = ColorFromHex(focus);
  palette.text = light ? ColorFromHex(0x161616) : ColorFromHex(0xffffff);
  palette.secondary = light ? ColorFromHex(0x303030) : ColorFromHex(0xc4c4c4);
  palette.header_text = light_header ? ColorFromHex(0x161616) : ColorFromHex(0xffffff);
  palette.card_secondary = light && !light_header ? ColorFromHex(0xd6def1) : palette.secondary;
  return palette;
}

// Reference-inspired color families, with separate text colors for dark selection/header
// surfaces on light themes. These are renderer-owned design values, not global ImGui styles.
inline constexpr std::array<Palette, ::BigPicture::THEME_CHOICES.size()> PALETTES = {
    DARK,
    MakePalette(0xc8c8c8, 0x2a3e78, 0xe1e2e1, 0xb3b8c2, 0x2a3e78, 0x235cd9, true),
    MakePalette(0x0a0a0a, 0x0a0a0a, 0x000000, 0x252525, 0x333333, 0xdcdcdc),
    MakePalette(0x2b3760, 0x202e5a, 0x222c4d, 0x3b54ac, 0x244d9e, 0x8cb6ff),
    MakePalette(0x353944, 0x292d35, 0x212121, 0x484d57, 0x202c40, 0xb7c9e5),
    MakePalette(0x9db1bb, 0xa2c2bc, 0xc0d2d0, 0x89aaa9, 0x28666b, 0x185459, true, true),
    MakePalette(0xf1d9ee, 0xffaec9, 0xf8e8f5, 0xe7b3cc, 0xa82d58, 0x96214b, true, true),
    MakePalette(0xd9c9ba, 0xe9bb93, 0xf0dfcd, 0xd1a580, 0x8f3b1f, 0x782b12, true, true),
    MakePalette(0x782c44, 0x4f2c44, 0x632438, 0xa73e5f, 0x8c203c, 0xffb1c8),
    MakePalette(0x6e1e7d, 0x321846, 0x502657, 0x862c9c, 0x561d79, 0xff94f1),
    MakePalette(0x819af0, 0x92b6fe, 0xb4cffe, 0xa1bbfb, 0x224b9b, 0x103a8e, true, true),
};

constexpr const Palette& GetPalette(std::string_view id)
{
  for (std::size_t i = 0; i < ::BigPicture::THEME_CHOICES.size(); ++i)
    if (id == ::BigPicture::THEME_CHOICES[i].id)
      return PALETTES[i];
  return DARK;
}

struct MenuStyleOptions
{
  ImVec4 background{0, 0, 0, 0};
  ImVec2 item_spacing{0, 0};
  float scrollbar_size = 12;
  // Selectable fill; transparent where the view draws its own hover and focus frames.
  ImVec4 header{0, 0, 0, 0};
  // Left at the global style when unset.
  std::optional<float> window_rounding;
  std::optional<float> frame_rounding;
};

// Keep paired ImGui stack operations together when views share the same controls.
class ScopedMenuStyle
{
public:
  ScopedMenuStyle(const Palette& palette, const MenuStyleOptions& options)
  {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, options.background);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, palette.text);
    ImGui::PushStyleColor(ImGuiCol_Header, options.header);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, options.header);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, options.header);
    // Like PCSX2's fullscreen UI, controls never draw ImGui's separate focus outline: their own
    // highlight follows ImGui's hovered state, which is the focused control after keyboard or
    // gamepad input and the control under the pointer after mouse input.
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, palette.header);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.hover);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, palette.background);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, palette.header);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, palette.hover);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, palette.secondary);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, options.item_spacing);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, options.scrollbar_size);
    if (options.window_rounding)
    {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, *options.window_rounding);
      ++m_vars;
    }
    if (options.frame_rounding)
    {
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, *options.frame_rounding);
      ++m_vars;
    }
  }
  ~ScopedMenuStyle()
  {
    ImGui::PopStyleVar(m_vars);
    ImGui::PopStyleColor(14);
  }
  ScopedMenuStyle(const ScopedMenuStyle&) = delete;
  ScopedMenuStyle& operator=(const ScopedMenuStyle&) = delete;

private:
  int m_vars = 5;
};

// Uses the caller's current font and clip rectangle; each view retains its own footer geometry.
// Returns the text position so settings pages can place their context label alongside it.
inline ImVec2 DrawFooterText(ImDrawList* draw, ImVec2 clip_min, ImVec2 clip_max,
                             const std::string& text, ImU32 color, float padding,
                             float wrap_width = 0)
{
  const auto size = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap_width);
  const ImVec2 position(std::max(padding, clip_max.x - padding - size.x),
                        clip_min.y + (clip_max.y - clip_min.y - size.y) * 0.5f);
  draw->PushClipRect(clip_min, clip_max, true);
  draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), position, color, text.c_str(), nullptr,
                wrap_width);
  draw->PopClipRect();
  return position;
}
}  // namespace VideoCommon::BigPicture::Style
