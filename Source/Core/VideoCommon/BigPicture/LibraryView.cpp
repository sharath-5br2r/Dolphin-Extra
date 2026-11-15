// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

#include "VideoCommon/BigPicture/Style.h"

namespace VideoCommon::BigPicture
{
namespace
{
void Cover(ImDrawList* draw, const MenuItem& item, ImVec2 position, ImVec2 box, float scale,
           const Style::Palette& palette,
           const std::function<ImTextureID(const std::string&)>& image)
{
  const float aspect =
      std::isfinite(item.image_aspect) && item.image_aspect > 0 ? item.image_aspect : 2.0f / 3.0f;
  const ImVec2 size(std::min(box.x, box.y * aspect), std::min(box.y, box.x / aspect));
  const ImVec2 min(position.x + (box.x - size.x) * 0.5f, position.y + (box.y - size.y) * 0.5f);
  const ImVec2 max(min.x + size.x, min.y + size.y);
  if (const auto texture = image && !item.image_key.empty() ? image(item.image_key) : ImTextureID{})
    draw->AddImage(texture, min, max);
  else
  {
    draw->AddRectFilled(min, max, ImGui::GetColorU32(palette.panel), 3 * scale);
    const float icon = std::min(size.x, size.y) * 0.45f;
    DrawMenuIcon(draw, MenuIcon::Disc,
                 ImVec2(min.x + (size.x - icon) * 0.5f, min.y + (size.y - icon) * 0.5f), icon,
                 ImGui::GetColorU32(palette.secondary));
  }
}

std::string Ellipsize(const std::string& title, float width)
{
  if (ImGui::CalcTextSize(title.c_str()).x <= width)
    return title;
  const char* end = title.c_str();
  const float available = std::max(0.0f, width - ImGui::CalcTextSize("…").x);
  ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), available, 0, title.c_str(),
                                  title.c_str() + title.size(), &end);
  return std::string(title.c_str(), end) + "…";
}
}  // namespace

MenuResult
NavigationToolkit::DrawLibrary(const MenuPage& page, ::BigPicture::NavigationOptions options,
                               const std::string& prompts, ImFont* font,
                               const std::function<ImTextureID(const std::string&)>& image)
{
  MenuResult result;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  const auto display = ImGui::GetIO().DisplaySize;
  const float scale = std::max(0.01f, std::min(display.x / 1280, display.y / 720) * options.scale);
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  const float padding = 15 * scale;
  const float header = Style::BAR_HEIGHT * scale;
  const bool grid = page.columns > 1;
  const int columns =
      grid ? std::clamp(static_cast<int>(display.x * (1 - options.safe_area * 2) / (256 * scale)),
                        1, 12) :
             1;
  auto& memory = m_memory[page.id];
  const auto layout = TrackLayout(page.id, display, scale);
  const bool shape_changed = layout.shape || m_library_columns != columns || m_library_grid != grid;
  const bool entering = layout.page || shape_changed || m_focus_revision != page.focus_revision ||
                        GImGui->NavWindow == nullptr;
  const auto previous_focus = memory.item;
  const auto game = [&](const std::string& id) {
    return std::ranges::find_if(page.items,
                                [&](const auto& item) { return item.id == id && item.enabled; });
  };
  auto selected = game(memory.item);
  if (memory.item.empty() && page.default_focus == "back")
  {
    selected = std::ranges::find_if(page.items, [](const auto& item) { return item.enabled; });
    if (selected != page.items.end())
      memory.item = selected->id;
  }
  if (selected == page.items.end() && memory.item != "back" &&
      (page.context.id.empty() || memory.item != page.context.id))
  {
    selected = game(page.default_focus);
    memory.item = selected == page.items.end() ? "back" : selected->id;
  }
  const bool restore_focus = entering || previous_focus != memory.item;
  const auto focus_id = memory.item;
  m_library_columns = columns;
  m_library_grid = grid;
  m_focus_revision = page.focus_revision;

  const Style::ScopedMenuStyle style(palette, {.background = palette.background,
                                               .item_spacing = ImVec2(20 * scale, 30 * scale),
                                               .scrollbar_size = 12 * scale,
                                               .frame_rounding = Style::ROUNDING * scale});
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureLibrary", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, header), ImGui::GetColorU32(palette.header));
  const auto button = [&](const MenuItem& item, float x) {
    ImGui::SetCursorPos(ImVec2(x, 0));
    ImGui::PushID(item.id.c_str());
    if (restore_focus && focus_id == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    if (ImGui::Button("##nav", ImVec2(48 * scale, header)))
      result.activated = item.id;
    const auto min = ImGui::GetItemRectMin();
    auto icon_color = palette.header_text;
    if (!item.active && item.id != "back")
      icon_color.w *= 0.55f;
    DrawMenuIcon(draw, item.icon, ImVec2(min.x + 12 * scale, min.y + 10 * scale), 24 * scale,
                 ImGui::GetColorU32(icon_color));
    if (item.id == "back" && ImGui::IsItemFocused() && !restore_focus)
      memory.item = item.id;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", item.label.c_str());
    ImGui::EndDisabled();
    ImGui::PopID();
  };
  for (std::size_t i = 0; i < page.toolbar.size(); ++i)
  {
    const auto& item = page.toolbar[i];
    button(item,
           item.id == "back" ? 5 * scale : display.x - (page.toolbar.size() - i) * 62 * scale);
  }
  draw->AddText(ImVec2(63 * scale, (header - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(palette.header_text), page.title.c_str());
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  const float footer =
      std::max(34 * scale, ImGui::CalcTextSize(prompts.c_str(), nullptr, false,
                                               std::max(scale, display.x - padding * 2))
                                   .y +
                               14 * scale);
  ImGui::PopFont();
  const float top = std::max(header, display.y * options.safe_area);
  const float bottom = std::min(display.y - footer, display.y * (1 - options.safe_area));
  const float height = std::max(scale, bottom - top);
  const float details_width = !grid && display.x >= 900 * scale ? 530 * scale : 0;
  const float content_width = display.x - details_width;
  const float grid_image_height = std::min(272 * scale, std::max(40 * scale, height - 44 * scale));
  const float row_height = grid ? grid_image_height + 44 * scale : 80 * scale;
  const float pitch = row_height + (grid ? 30 * scale : 0);
  if (entering || previous_focus != memory.item)
  {
    m_restore_frames = 3;
    m_restore_scroll = memory.scroll;
    if (selected != page.items.end())
    {
      const float y = static_cast<float>((selected - page.items.begin()) / columns) * pitch;
      const float saved = memory.scroll * scale;
      if (shape_changed || y < saved || y + row_height > saved + height)
        m_restore_scroll = std::max(0.0f, y - (height - row_height) * 0.5f) / scale;
    }
  }
  for (const auto key :
       {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow,
        ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadDpadLeft,
        ImGuiKey_GamepadDpadRight, ImGuiKey_PageUp, ImGuiKey_PageDown})
    if (ImGui::IsKeyPressed(key))
      m_restore_frames = 0;
  if (m_restore_frames)
    ImGui::SetNextWindowScroll(ImVec2(0, m_restore_scroll * scale));
  ImGui::SetCursorScreenPos(ImVec2(0, top));
  ImGui::BeginChild(page.id.c_str(), ImVec2(content_width, height), ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  float status_height = 0;
  if (!page.description.empty())
  {
    ImGui::SetCursorPosX(padding);
    ImGui::TextWrapped("%s", page.description.c_str());
    status_height = ImGui::GetCursorPosY();
  }
  if (page.items.empty() && !page.context.id.empty())
  {
    ImGui::SetCursorPos(ImVec2(std::max(padding, (content_width - 280 * scale) * 0.5f),
                               std::max(status_height, height * 0.4f)));
    ImGui::BeginDisabled(!page.context.enabled);
    if (restore_focus && memory.item == page.context.id)
      ImGui::SetKeyboardFocusHere();
    if (ImGui::Button(page.context.label.c_str(), ImVec2(280 * scale, 48 * scale)))
      result.activated = page.context.id;
    if (ImGui::IsItemFocused())
      memory.item = page.context.id;
    ImGui::EndDisabled();
  }
  const bool previous = ImGui::IsKeyPressed(ImGuiKey_PageUp);
  const bool next = ImGui::IsKeyPressed(ImGuiKey_PageDown);
  std::string requested;
  if (selected != page.items.end() && (previous || next))
  {
    const int step = std::max(1, static_cast<int>(height / pitch)) * columns;
    int target = std::clamp(static_cast<int>(selected - page.items.begin()) + (next ? step : -step),
                            0, static_cast<int>(page.items.size()) - 1);
    while (target >= 0 && target < static_cast<int>(page.items.size()))
    {
      if (page.items[target].enabled)
      {
        requested = page.items[target].id;
        break;
      }
      target += next ? 1 : -1;
    }
  }
  const float grid_start = (content_width - columns * 256 * scale) * 0.5f;
  for (std::size_t i = 0; i < page.items.size(); ++i)
  {
    const auto& item = page.items[i];
    const float width =
        grid ? 236 * scale : std::max(scale, content_width - padding * 2 - 12 * scale);
    ImGui::SetCursorPos(ImVec2(grid ? grid_start + (i % columns) * 256 * scale : padding,
                               status_height + (i / columns) * pitch));
    ImGui::PushID(item.id.c_str());
    const bool focus = (restore_focus && focus_id == item.id) || requested == item.id;
    if (focus && item.enabled)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    const bool clicked = ImGui::Selectable(
        "##game", false, ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(width, row_height));
    ImGui::PopStyleColor();
    if (clicked ||
        (item.enabled && ImGui::IsItemFocused() && (!restore_focus || focus_id == item.id)))
      memory.item = item.id;
    if (clicked)
      result.activated = item.id;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && item.enabled)
    {
      memory.item = item.id;
      result.activated = "options:" + item.id;
    }
    if (!requested.empty() && focus)
      ImGui::SetScrollHereY(0.5f);
    if (ImGui::IsItemVisible())
    {
      const auto min = ImGui::GetItemRectMin();
      const auto max = ImGui::GetItemRectMax();
      auto* cards = ImGui::GetWindowDrawList();
      const bool highlight = ImGui::IsItemHovered();
      if (highlight)
      {
        auto hover = palette.hover;
        hover.w *= 0.7f;
        cards->AddRectFilled(min, max, ImGui::GetColorU32(hover), 6 * scale);
        cards->AddRect(min, max, ImGui::GetColorU32(palette.secondary), 6 * scale, 0, scale);
      }
      if (grid)
      {
        Cover(cards, item, ImVec2(min.x + padding, min.y + 10 * scale),
              ImVec2(width - padding * 2, grid_image_height), scale, palette, image);
        const auto title = Ellipsize(item.label, width - 20 * scale);
        cards->AddText(ImVec2(min.x + (width - ImGui::CalcTextSize(title.c_str()).x) * 0.5f,
                              min.y + grid_image_height + 20 * scale),
                       ImGui::GetColorU32(palette.text), title.c_str());
      }
      else
      {
        Cover(cards, item, ImVec2(min.x + 10 * scale, min.y + 5 * scale),
              ImVec2(50 * scale, 70 * scale), scale, palette, image);
        const auto title = Ellipsize(item.label, width - 86 * scale);
        cards->AddText(ImVec2(min.x + 74 * scale, min.y + 14 * scale),
                       ImGui::GetColorU32(palette.text), title.c_str());
        const auto summary = Ellipsize(item.detail, width - 86 * scale);
        cards->AddText(ImVec2(min.x + 74 * scale, min.y + 42 * scale),
                       ImGui::GetColorU32(palette.secondary), summary.c_str());
      }
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  ImGui::PopFont();
  memory.scroll = m_restore_frames ? m_restore_scroll : ImGui::GetScrollY() / scale;
  if (m_restore_frames)
    --m_restore_frames;
  ImGui::EndChild();
  if (details_width)
  {
    selected = game(memory.item);
    ImGui::SetCursorScreenPos(ImVec2(content_width + padding, top));
    ImGui::BeginChild("##library-details", ImVec2(details_width - padding * 2, height),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    if (selected != page.items.end())
    {
      const float cover_height = std::min(350 * scale, height * 0.55f);
      Cover(ImGui::GetWindowDrawList(), *selected, ImGui::GetCursorScreenPos(),
            ImVec2(details_width - padding * 2, cover_height), scale, palette, image);
      ImGui::Dummy(ImVec2(0, cover_height + 16 * scale));
      ImGui::TextWrapped("%s", selected->label.c_str());
      ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
      ImGui::TextWrapped(
          "%s",
          (selected->preview_detail.empty() ? selected->detail : selected->preview_detail).c_str());
      ImGui::PopFont();
    }
    ImGui::EndChild();
  }
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  draw->AddRectFilled(ImVec2(0, display.y - footer), display, ImGui::GetColorU32(palette.header));
  Style::DrawFooterText(draw, ImVec2(0, display.y - footer), display, prompts,
                        ImGui::GetColorU32(palette.header_text), padding,
                        std::max(scale, display.x - padding * 2));
  ImGui::PopFont();
  if (ImGui::IsKeyPressed(ImGuiKey_F1, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false))
    result.activated = "view-toggle";
  else if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
    result.activated = "settings";
  else if (ImGui::IsKeyPressed(ImGuiKey_F3, false) ||
           ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false))
    result.activated = "options";
  else if (ImGui::IsKeyPressed(ImGuiKey_F4, false))
    result.activated = "search";
  else if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
    result.activated = "refresh";
  else if ((ImGui::IsKeyPressed(ImGuiKey_F6, false) ||
            ImGui::IsKeyPressed(GetNavigationKey(::BigPicture::NavButton::Options, false),
                                false)) &&
           memory.item.starts_with("game:"))
    result.activated = "options:" + memory.item;
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::End();
  ImGui::PopFont();
  return result;
}
}  // namespace VideoCommon::BigPicture
