// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

#include "VideoCommon/BigPicture/Style.h"

namespace VideoCommon::BigPicture
{
MenuResult
NavigationToolkit::DrawStates(const MenuPage& page, ::BigPicture::NavigationOptions options,
                              const std::string& prompts, ImFont* font,
                              const std::function<ImTextureID(const std::string&)>& image)
{
  MenuResult result;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  const auto display = ImGui::GetIO().DisplaySize;
  const float scale = std::max(0.01f, std::min(display.x / 1280, display.y / 720) * options.scale);
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  const float header = Style::BAR_HEIGHT * scale;
  const float footer = Style::FOOTER_HEIGHT * scale;
  const float margin = std::max(15 * scale, display.x * options.safe_area);
  const float top = std::max(header, display.y * options.safe_area);
  const float bottom = std::min(display.y - footer, display.y * (1 - options.safe_area));
  const float gap = 48 * scale;
  const float available = std::max(scale, display.x - margin * 2);
  const int columns = std::clamp(static_cast<int>((available + gap) / (268 * scale + gap)), 1, 3);
  const float width = std::min(268 * scale, (available - gap * (columns - 1)) / columns);
  const float padding = 12 * scale;
  const float image_width = std::max(scale, width - padding * 2);
  const float image_height = image_width * 0.75f;
  const float height = image_height + 86 * scale + padding * 2;
  const float grid_width = columns * width + (columns - 1) * gap;
  const float left = (display.x - grid_width) * 0.5f;
  auto& memory = m_memory[page.id];
  const auto previous = memory.item;
  if (!HasEnabledItem(page.items, memory.item) && !HasEnabledItem(page.toolbar, memory.item) &&
      !page.busy)
    memory.item = page.default_focus;
  const bool entering = TrackLayout(page.id, display, scale).Any() || previous != memory.item ||
                        GImGui->NavWindow == nullptr || (m_settings_busy && !page.busy);
  m_settings_busy = page.busy;

  auto background = palette.background;
  background.w = page.overlay ? 0.80f : 1.0f;
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), display,
                                                ImGui::GetColorU32(background));
  const Style::ScopedMenuStyle style(
      palette, {.item_spacing = ImVec2(gap, 10 * scale), .scrollbar_size = 12 * scale});
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureStates", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, header), ImGui::GetColorU32(palette.header));
  draw->AddRectFilled(ImVec2(0, display.y - footer), display, ImGui::GetColorU32(palette.header));
  for (const auto& item : page.toolbar)
  {
    const bool back = item.id == "back";
    const float x = back ? 5 * scale : display.x - 53 * scale;
    ImGui::SetCursorPos(ImVec2(x, 0));
    ImGui::PushID(item.id.c_str());
    if (entering && memory.item == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    if (ImGui::Button("##action", ImVec2(48 * scale, header)))
      result.activated = item.id;
    DrawMenuIcon(draw, item.icon, ImVec2(x + 12 * scale, 10 * scale), 24 * scale,
                 ImGui::GetColorU32(palette.header_text));
    if (ImGui::IsItemFocused())
      memory.item = item.id;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", item.label.c_str());
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  draw->AddText(ImVec2(63 * scale, (header - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(palette.header_text), page.title.c_str());
  ImGui::SetCursorPos(ImVec2(0, top));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 10 * scale));
  ImGui::BeginChild("##state-slots", ImVec2(display.x, std::max(scale, bottom - top)),
                    ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  if (entering)
    ImGui::SetScrollY(memory.scroll * scale);
  for (std::size_t i = 0; i < page.items.size(); ++i)
  {
    const auto& item = page.items[i];
    if (i % columns)
      ImGui::SameLine();
    else
      ImGui::SetCursorPosX(left);
    ImGui::PushID(item.id.c_str());
    if (entering && memory.item == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    const bool pressed = ImGui::Selectable(
        "##slot", false, ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(width, height));
    ImGui::PopStyleColor();
    if (pressed || ImGui::IsItemFocused())
      memory.item = item.id;
    if (pressed)
      result.activated = item.id;
    if (entering && memory.item == item.id)
      ImGui::SetScrollHereY();
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    if (ImGui::IsItemVisible())
    {
      auto* cards = ImGui::GetWindowDrawList();
      if (ImGui::IsItemHovered())
      {
        cards->AddRectFilled(min, max, ImGui::GetColorU32(palette.hover), 6 * scale);
        cards->AddRect(min, max, ImGui::GetColorU32(palette.secondary), 6 * scale, 0, scale);
      }
      const ImVec2 content_min(min.x + padding, min.y + padding);
      const ImVec2 content_max(max.x - padding, max.y - padding);
      cards->PushClipRect(content_min, content_max, true);
      const auto texture = image ? image(item.image_key) : ImTextureID{};
      if (texture)
      {
        const float aspect = std::isfinite(item.image_aspect) && item.image_aspect > 0 ?
                                 item.image_aspect :
                                 4.0f / 3;
        const float w = std::min(image_width, image_height * aspect);
        const float h = w / aspect;
        const ImVec2 position(content_min.x + (image_width - w) * 0.5f,
                              content_min.y + (image_height - h) * 0.5f);
        cards->AddImage(texture, position, ImVec2(position.x + w, position.y + h));
      }
      else
        DrawMenuIcon(cards, MenuIcon::Image,
                     ImVec2(content_min.x, content_min.y + (image_height - image_width) * 0.5f),
                     image_width, ImGui::GetColorU32(palette.secondary));
      cards->AddText(ImVec2(content_min.x, content_min.y + image_height + 10 * scale),
                     ImGui::GetColorU32(palette.text), item.label.c_str());
      ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
      cards->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                     ImVec2(content_min.x, content_min.y + image_height + 36 * scale),
                     ImGui::GetColorU32(palette.text), item.detail.c_str(), nullptr, image_width);
      ImGui::PopFont();
      cards->PopClipRect();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  memory.scroll = ImGui::GetScrollY() / scale;
  ImGui::EndChild();
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  Style::DrawFooterText(draw, ImVec2(0, display.y - footer), display, prompts,
                        ImGui::GetColorU32(palette.header_text), margin);
  ImGui::PopFont();
  if (!page.busy &&
      (ImGui::IsKeyPressed(ImGuiKey_F1, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)))
    result.activated = "refresh";
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::End();
  ImGui::PopFont();
  return result;
}
}  // namespace VideoCommon::BigPicture
