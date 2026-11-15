// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>

#include <imgui_internal.h>

#include "VideoCommon/BigPicture/Style.h"

namespace VideoCommon::BigPicture
{
MenuResult
NavigationToolkit::DrawDialog(const MenuPage& page, ::BigPicture::NavigationOptions options,
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
                                display.x * (1 - 2 * options.safe_area) / 680,
                                display.y * (1 - 2 * options.safe_area) / 400}));
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  const float header = Style::BAR_HEIGHT * scale;
  const float footer = Style::FOOTER_HEIGHT * scale;
  const float padding = 16 * scale;
  const float width = 600 * scale;
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  const auto explanation =
      ImGui::CalcTextSize(page.description.c_str(), nullptr, false, width - 2 * padding);
  ImGui::PopFont();
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);
  std::vector<float> heights;
  float content_height = padding * 2;
  if (!page.description.empty())
    content_height += explanation.y + padding;
  for (const auto& item : page.items)
  {
    const float wrap = width - 2 * padding - 48 * scale;
    const auto label = ImGui::CalcTextSize(item.label.c_str(), nullptr, false, wrap);
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    const auto detail = ImGui::CalcTextSize(item.detail.c_str(), nullptr, false, wrap);
    ImGui::PopFont();
    const float height = std::max(
        52 * scale, label.y + (item.detail.empty() ? 0 : detail.y + 6 * scale) + 20 * scale);
    heights.push_back(height);
    content_height += height;
  }
  const float available_height =
      std::max(scale, display.y - footer - 2 * std::max(24 * scale, display.y * options.safe_area));
  const ImVec2 size(width, std::min(header + content_height, available_height));
  const ImVec2 position((display.x - size.x) * 0.5f, (display.y - footer - size.y) * 0.5f);
  auto& memory = m_memory[page.id];
  const auto valid = [&](const std::string& id) { return HasEnabledItem(page.items, id); };
  const bool revision_changed = memory.revision != page.focus_revision;
  const bool entering =
      TrackLayout(page.id, size, scale).Any() || revision_changed || GImGui->NavWindow == nullptr;
  if (revision_changed || !valid(memory.item))
  {
    const auto first = std::ranges::find_if(page.items, &MenuItem::enabled);
    memory.item = valid(page.default_focus) ? page.default_focus :
                  first == page.items.end() ? std::string{} :
                                              first->id;
    memory.scroll = 0;
  }
  const auto focus_id = memory.item;
  memory.revision = page.focus_revision;

  if (!page.background)
  {
    auto backdrop = palette.background;
    backdrop.w = page.overlay ? 0.8f : 1;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), display,
                                                  ImGui::GetColorU32(backdrop));
  }
  // Rows draw their own focus frame; the dialog uses no ImGui buttons.
  const Style::ScopedMenuStyle style(palette, {.background = palette.background,
                                               .scrollbar_size = 12 * scale,
                                               .window_rounding = Style::DIALOG_ROUNDING * scale});
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(size);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureDialog", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + header),
                      ImGui::GetColorU32(palette.header), Style::DIALOG_ROUNDING * scale,
                      ImDrawFlags_RoundCornersTop);
  draw->PushClipRect(position, ImVec2(position.x + width - header, position.y + header), true);
  draw->AddText(
      ImVec2(position.x + padding, position.y + (header - ImGui::GetTextLineHeight()) * 0.5f),
      ImGui::GetColorU32(palette.header_text), page.title.c_str());
  draw->PopClipRect();
  ImGui::SetCursorPos(ImVec2(width - header, 0));
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  if (ImGui::InvisibleButton("##close", ImVec2(header, header)))
    result.back = true;
  ImGui::PopItemFlag();
  const ImVec2 center(position.x + width - header * 0.5f, position.y + header * 0.5f);
  const float half = 7 * scale;
  const auto close_color = ImGui::GetColorU32(palette.header_text);
  draw->AddLine(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half),
                close_color, scale);
  draw->AddLine(ImVec2(center.x + half, center.y - half), ImVec2(center.x - half, center.y + half),
                close_color, scale);

  ImGui::SetCursorPos(ImVec2(0, header));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
  ImGui::BeginChild("##dialog-content", ImVec2(width, size.y - header),
                    ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  if (entering)
    ImGui::SetScrollY(memory.scroll * scale);
  if (!page.description.empty())
  {
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    ImGui::PushTextWrapPos(width - padding);
    ImGui::TextUnformatted(page.description.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, padding));
  }
  for (std::size_t i = 0; i < page.items.size(); ++i)
  {
    const auto& item = page.items[i];
    ImGui::SetCursorPosX(padding);
    ImGui::PushID(item.id.c_str());
    if (entering && focus_id == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled || page.busy);
    const bool pressed =
        ImGui::Selectable("##dialog-action", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                          ImVec2(width - 2 * padding - 12 * scale, heights[i]));
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    if (pressed)
      result.activated = item.id;
    if (pressed || (ImGui::IsItemFocused() && (!entering || focus_id == item.id)))
    {
      memory.item = item.id;
      if (entering || GImGui->NavJustMovedToId == ImGui::GetItemID())
      {
        ImRect reveal(min, max);
        reveal.Expand(ImVec2(0, padding));
        ImGui::ScrollToRectEx(ImGui::GetCurrentWindow(), reveal,
                              ImGuiScrollFlags_KeepVisibleEdgeY | ImGuiScrollFlags_NoScrollParent);
      }
    }
    if (ImGui::IsItemVisible())
    {
      auto* rows = ImGui::GetWindowDrawList();
      if (ImGui::IsItemHovered())
      {
        auto hover = palette.hover;
        hover.w *= 0.7f;
        rows->AddRectFilled(min, max, ImGui::GetColorU32(hover), 6 * scale);
        rows->AddRect(min, max, ImGui::GetColorU32(palette.secondary), 6 * scale, 0, scale);
      }
      DrawMenuIcon(rows, item.icon, ImVec2(min.x + 12 * scale, min.y + 14 * scale), 22 * scale,
                   ImGui::GetColorU32(palette.text));
      const float wrap = width - 2 * padding - 48 * scale;
      rows->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(min.x + 44 * scale, min.y + 10 * scale),
                    ImGui::GetColorU32(palette.text), item.label.c_str(), nullptr, wrap);
      if (!item.detail.empty())
      {
        const auto label = ImGui::CalcTextSize(item.label.c_str(), nullptr, false, wrap);
        ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
        rows->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      ImVec2(min.x + 44 * scale, min.y + label.y + 16 * scale),
                      ImGui::GetColorU32(palette.secondary), item.detail.c_str(), nullptr, wrap);
        ImGui::PopFont();
      }
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  memory.scroll = ImGui::GetScrollY() / scale;
  ImGui::EndChild();
  result.back |= ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                 ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::End();

  auto* foreground = ImGui::GetForegroundDrawList();
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
