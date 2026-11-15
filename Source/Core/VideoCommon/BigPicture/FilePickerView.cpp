// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>

#include <imgui_internal.h>

#include "VideoCommon/BigPicture/Style.h"

namespace VideoCommon::BigPicture
{
MenuResult
NavigationToolkit::DrawFilePicker(const MenuPage& page, ::BigPicture::NavigationOptions options,
                                  const std::string& prompts, ImFont* font,
                                  const std::function<ImTextureID(const std::string&)>& image)
{
  if (page.background)
    DrawLanding(*page.background, options, {}, font, image, true);

  MenuResult result;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  const auto display = ImGui::GetIO().DisplaySize;
  const float scale =
      std::max(0.01f, std::min({display.x / 1280 * options.scale, display.y / 720 * options.scale,
                                display.x * (1 - options.safe_area * 2) / 1000,
                                display.y * (1 - options.safe_area * 2) / 620}));
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  const float header = Style::BAR_HEIGHT * scale;
  const float footer = Style::FOOTER_HEIGHT * scale;
  const float margin = std::max(28 * scale, display.y * options.safe_area);
  const ImVec2 size(std::min(1000 * scale, display.x - margin * 2),
                    std::max(100 * scale, display.y - footer - margin * 2));
  const ImVec2 position((display.x - size.x) * 0.5f, (display.y - footer - size.y) * 0.5f);
  auto& memory = m_memory[page.id];
  const bool revision_changed = memory.revision != page.focus_revision;
  const auto valid = [&](const std::string& id) {
    return (id == "back" && page.context.enabled) || HasEnabledItem(page.items, id) ||
           HasEnabledItem(page.toolbar, id);
  };
  const bool entering =
      TrackLayout(page.id, size, scale).Any() || revision_changed || GImGui->NavWindow == nullptr;
  if (revision_changed || !valid(memory.item))
  {
    memory.item = valid(page.default_focus) ? page.default_focus : "back";
    memory.scroll = 0;
  }
  const auto focus_id = memory.item;
  m_focus_revision = page.focus_revision;
  memory.revision = page.focus_revision;

  auto panel = palette.background;
  panel.w = page.background ? 0.96f : 1.0f;
  // Picker rows use ImGui's Selectable fill; the picker uses no ImGui buttons.
  const Style::ScopedMenuStyle style(palette, {.background = panel,
                                               .scrollbar_size = 12 * scale,
                                               .header = palette.hover,
                                               .window_rounding = Style::DIALOG_ROUNDING * scale});
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);

  // Draw a separate non-interactive backdrop so modal navigation cannot reach the tiles.
  auto* background = ImGui::GetBackgroundDrawList();
  if (!page.background)
    background->AddRectFilled(ImVec2(0, 0), display, ImGui::GetColorU32(palette.background));
  auto* foreground = ImGui::GetForegroundDrawList();
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(size);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureFilePicker", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + header),
                      ImGui::GetColorU32(palette.header), Style::DIALOG_ROUNDING * scale,
                      ImDrawFlags_RoundCornersTop);
  DrawMenuIcon(draw, MenuIcon::Folder, ImVec2(position.x + 18 * scale, position.y + 12 * scale),
               22 * scale, ImGui::GetColorU32(palette.header_text));
  draw->PushClipRect(position, ImVec2(position.x + size.x - 48 * scale, position.y + header), true);
  draw->AddText(ImVec2(position.x + 48 * scale, position.y + 8 * scale),
                ImGui::GetColorU32(palette.header_text), page.title.c_str());
  draw->PopClipRect();
  ImGui::SetCursorPos(ImVec2(size.x - 44 * scale, 0));
  if (entering && focus_id == "back")
    ImGui::SetKeyboardFocusHere();
  ImGui::BeginDisabled(!page.context.enabled);
  if (ImGui::Selectable("##close", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                        ImVec2(44 * scale, header)))
    result.back = true;
  if (ImGui::IsItemFocused() && !entering)
    memory.item = "back";
  const ImVec2 cross(position.x + size.x - 22 * scale, position.y + header * 0.5f);
  const auto color = ImGui::GetColorU32(palette.header_text);
  draw->AddLine(ImVec2(cross.x - 7 * scale, cross.y - 7 * scale),
                ImVec2(cross.x + 7 * scale, cross.y + 7 * scale), color, scale);
  draw->AddLine(ImVec2(cross.x - 7 * scale, cross.y + 7 * scale),
                ImVec2(cross.x + 7 * scale, cross.y - 7 * scale), color, scale);
  ImGui::EndDisabled();

  ImGui::SetCursorPos(ImVec2(0, header));
  ImGui::BeginChild("##entries", ImVec2(size.x, size.y - header), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoSavedSettings);
  if (entering)
    ImGui::SetScrollY(memory.scroll * scale);
  const auto row = [&](const MenuItem& item, bool path = false) {
    ImGui::PushID(item.id.c_str());
    if (entering && focus_id == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    if (ImGui::Selectable("##entry", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                          ImVec2(0, 47 * scale)))
      result.activated = item.id;
    if (ImGui::IsItemFocused() && (!entering || focus_id == item.id))
    {
      memory.item = item.id;
      if (entering)
        ImGui::SetScrollHereY();
    }
    if (ImGui::IsItemVisible())
    {
      const auto min = ImGui::GetItemRectMin();
      const auto max = ImGui::GetItemRectMax();
      auto* rows_draw = ImGui::GetWindowDrawList();
      const auto text_color = ImGui::GetColorU32(path ? palette.secondary : palette.text);
      DrawMenuIcon(rows_draw, item.icon, ImVec2(min.x + 24 * scale, min.y + 14 * scale), 21 * scale,
                   text_color);
      const float x = min.x + 57 * scale;
      const float right = max.x - 12 * scale;
      const auto detail_size = ImGui::CalcTextSize(item.detail.c_str());
      const float label_right = item.detail.empty() ? right : right - detail_size.x - 20 * scale;
      rows_draw->PushClipRect(ImVec2(x, min.y), ImVec2(std::max(x, label_right), max.y), true);
      rows_draw->AddText(ImVec2(x, min.y + 10 * scale), text_color, item.label.c_str());
      rows_draw->PopClipRect();
      if (!item.detail.empty())
        rows_draw->AddText(ImVec2(right - detail_size.x, min.y + 10 * scale),
                           ImGui::GetColorU32(palette.secondary), item.detail.c_str());
      if (ImGui::IsItemHovered() &&
          ImGui::CalcTextSize(item.label.c_str()).x > max.x - x - 12 * scale)
        ImGui::SetTooltip("%s", item.label.c_str());
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  };
  const auto path =
      std::ranges::find_if(page.toolbar, [](const auto& item) { return item.id == "path"; });
  if (path != page.toolbar.end())
  {
    auto current = *path;
    current.label = page.context.label.empty() ? path->label : page.context.label;
    current.icon = MenuIcon::Folder;
    row(current, true);
  }
  if (!page.description.empty())
  {
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    ImGui::TextWrapped("%s", page.description.c_str());
    ImGui::PopFont();
  }
  for (const auto& item : page.toolbar)
    if (item.id == "choose-folder")
      row(item);
  for (const auto& item : page.items)
    row(item);
  for (const auto& item : page.toolbar)
    if (item.id != "path" && item.id != "choose-folder")
      row(item);

  // SetKeyboardFocusHere/SetScrollHereY reveal the retained row after a resize, even
  // when the new viewport cannot show as many rows as the old one.
  memory.scroll = ImGui::GetScrollY() / scale;
  ImGui::EndChild();
  result.back |= ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                 ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
      ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false))
  {
    const auto parent = std::ranges::find_if(
        page.items, [](const auto& item) { return item.id == "parent" && item.enabled; });
    if (parent != page.items.end())
      result.activated = parent->id;
    else
    {
      const auto roots = std::ranges::find_if(
          page.toolbar, [](const auto& item) { return item.id == "roots" && item.enabled; });
      if (roots != page.toolbar.end())
        result.activated = roots->id;
    }
  }
  result.focused = memory.item;
  ImGui::End();

  foreground->AddRectFilled(ImVec2(0, display.y - footer), display,
                            ImGui::GetColorU32(palette.header));
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  Style::DrawFooterText(foreground, ImVec2(15 * scale, display.y - footer), display, prompts,
                        ImGui::GetColorU32(palette.header_text), 15 * scale);
  ImGui::PopFont();
  ImGui::PopFont();
  return result;
}
}  // namespace VideoCommon::BigPicture
