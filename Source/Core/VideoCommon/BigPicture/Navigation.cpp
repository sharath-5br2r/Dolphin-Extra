// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>
#include <cmath>

namespace VideoCommon::BigPicture
{
MenuLayout ComputeMenuLayout(ImVec2 viewport, ::BigPicture::NavigationOptions options, bool dialog)
{
  options = ::BigPicture::SanitizeNavigationOptions(options);
  viewport.x = std::isfinite(viewport.x) ? std::max(1.0f, viewport.x) : 1.0f;
  viewport.y = std::isfinite(viewport.y) ? std::max(1.0f, viewport.y) : 1.0f;
  const float scale = std::min(viewport.x / 1280.0f, viewport.y / 720.0f) * options.scale;
  ImVec2 size(viewport.x * (1.0f - options.safe_area * 2.0f),
              viewport.y * (1.0f - options.safe_area * 2.0f));
  if (dialog)
  {
    size.x = std::min(size.x, 800.0f * scale);
    size.y = std::min(size.y, 500.0f * scale);
  }
  return {{(viewport.x - size.x) * 0.5f, (viewport.y - size.y) * 0.5f}, size, scale};
}

ImGuiKey GetNavigationKey(::BigPicture::NavButton button, bool swap_confirm_back)
{
  using ::BigPicture::NavButton;
  switch (button)
  {
  case NavButton::Up:
    return ImGuiKey_GamepadDpadUp;
  case NavButton::Down:
    return ImGuiKey_GamepadDpadDown;
  case NavButton::Left:
    return ImGuiKey_GamepadDpadLeft;
  case NavButton::Right:
    return ImGuiKey_GamepadDpadRight;
  case NavButton::Confirm:
    return swap_confirm_back ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
  case NavButton::Back:
    return swap_confirm_back ? ImGuiKey_GamepadFaceDown : ImGuiKey_GamepadFaceRight;
  case NavButton::Previous:
    return ImGuiKey_GamepadL1;
  case NavButton::Next:
    return ImGuiKey_GamepadR1;
  default:
    return ImGuiKey_None;
  }
}

void NavigationToolkit::Reset()
{
  m_current_page.clear();
  m_memory.clear();
  m_restore_frames = 0;
}

FocusMemory NavigationToolkit::GetMemory(const std::string& page) const
{
  const auto it = m_memory.find(page);
  return it != m_memory.end() ? it->second : FocusMemory{};
}

MenuResult NavigationToolkit::Draw(const MenuPage& page, ::BigPicture::NavigationOptions options,
                                   const std::string& prompts, ImFont* font)
{
  MenuResult result;
  const auto layout = ComputeMenuLayout(ImGui::GetIO().DisplaySize, options, page.dialog);
  const float scale = layout.scale;
  // DisplaySize is already in native pixels. Diagnostic OSD/DPI font scaling must not
  // multiply the menu's separately scaled geometry a second time.
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  auto& memory = m_memory[page.id];
  const auto previous_focus = memory.item;
  const bool entering = m_current_page != page.id || m_focus_revision != page.focus_revision ||
                        m_layout_width != layout.size.x || m_layout_height != layout.size.y ||
                        m_layout_scale != scale;
  m_current_page = page.id;
  m_focus_revision = page.focus_revision;
  // Destructive dialogs always start at their safe default, including another request using
  // the same page. Reuse one memory entry rather than accumulating entries per command ID.
  if (entering && page.dialog)
    memory = {};
  m_layout_width = layout.size.x;
  m_layout_height = layout.size.y;
  m_layout_scale = scale;
  auto focus = std::ranges::find_if(page.items, [&](const MenuItem& item) {
    return item.enabled && item.id == (memory.item.empty() ? page.default_focus : memory.item);
  });
  if (focus == page.items.end())
    focus = std::ranges::find_if(page.items, &MenuItem::enabled);
  if (focus != page.items.end())
    memory.item = focus->id;
  else
    memory.item.clear();
  const auto focus_id = memory.item;
  const bool restore_focus = entering || previous_focus != focus_id;
  if (entering)
  {
    m_restore_frames = !previous_focus.empty() && previous_focus == focus_id ? 3 : 0;
    m_restore_scroll = memory.scroll;
  }
  for (const auto key : {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow,
                         ImGuiKey_RightArrow, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown,
                         ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadL1,
                         ImGuiKey_GamepadR1, ImGuiKey_PageUp, ImGuiKey_PageDown})
  {
    if (ImGui::IsKeyPressed(key))
      m_restore_frames = 0;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24 * scale, 20 * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12 * scale, 12 * scale));
  ImGui::PushFont(font, 24 * scale / font_scale);
  ImGui::SetNextWindowPos(layout.position);
  ImGui::SetNextWindowSize(layout.size);
  ImGui::SetNextWindowBgAlpha(0.97f);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureNavigation", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  ImGui::PushFont(font, 32 * scale / font_scale);
  ImGui::TextWrapped("%s", page.title.c_str());
  ImGui::PopFont();
  if (!page.description.empty())
    ImGui::TextWrapped("%s", page.description.c_str());
  ImGui::Separator();
  const float footer_height = 64 * scale;
  if (entering || m_restore_frames != 0)
    ImGui::SetNextWindowScroll(
        ImVec2(0, (m_restore_frames ? m_restore_scroll : memory.scroll) * scale));
  ImGui::BeginChild(page.id.c_str(), ImVec2(0, -footer_height), ImGuiChildFlags_None,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  const bool previous =
      ImGui::IsKeyPressed(ImGuiKey_GamepadL1) || ImGui::IsKeyPressed(ImGuiKey_PageUp);
  const bool next =
      ImGui::IsKeyPressed(ImGuiKey_GamepadR1) || ImGui::IsKeyPressed(ImGuiKey_PageDown);
  std::string requested_focus;
  if (focus != page.items.end() && (previous || next))
  {
    const int current = static_cast<int>(focus - page.items.begin());
    const int step = std::max(1, static_cast<int>(ImGui::GetWindowHeight() / (76 * scale)));
    int target =
        std::clamp(current + (next ? step : -step), 0, static_cast<int>(page.items.size()) - 1);
    while (target >= 0 && target < static_cast<int>(page.items.size()))
    {
      if (page.items[target].enabled)
      {
        requested_focus = page.items[target].id;
        break;
      }
      target += next ? 1 : -1;
    }
  }
  const int columns = std::clamp(page.columns, 1, 6);
  const float width = (ImGui::GetContentRegionAvail().x - (columns - 1) * 12 * scale) / columns;
  for (std::size_t i = 0; i < page.items.size(); ++i)
  {
    const auto& item = page.items[i];
    if (i % columns != 0)
      ImGui::SameLine();
    const float wrap_width = std::max(scale, width - 32 * scale);
    const float label_height =
        ImGui::CalcTextSize(item.label.c_str(), nullptr, false, wrap_width).y;
    const float detail_height =
        item.detail.empty() ?
            0 :
            ImGui::CalcTextSize(item.detail.c_str(), nullptr, false, wrap_width).y + 8 * scale;
    const float height =
        std::max((columns == 1 ? 64 : 150) * scale, label_height + detail_height + 24 * scale);
    ImGui::PushID(item.id.c_str());
    const bool request_focus = (restore_focus && item.id == focus_id) || item.id == requested_focus;
    if (request_focus && item.enabled)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    const bool clicked = ImGui::Selectable("##item", item.enabled && item.id == memory.item,
                                           ImGuiSelectableFlags_None, ImVec2(width, height));
    if (item.enabled && (ImGui::IsItemFocused() || clicked))
      memory.item = item.id;
    if (request_focus && !requested_focus.empty())
      ImGui::SetScrollHereY();
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    if ((m_restore_frames && item.id == focus_id) ||
        (item.id == memory.item && memory.item != previous_focus))
    {
      // Wrapping can change row heights when aspect ratio or safe area changes. Retain
      // the saved offset when it still shows the selected control; otherwise reveal it.
      const float top = ImGui::GetWindowPos().y + ImGui::GetStyle().WindowPadding.y;
      const float bottom =
          ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y;
      if (min.y < top || max.y > bottom)
      {
        const float delta = min.y < top ? min.y - top : max.y - bottom;
        m_restore_scroll = std::max(0.0f, (ImGui::GetScrollY() + delta) / scale);
        ImGui::SetScrollY(m_restore_scroll * scale);
      }
    }
    auto* draw = ImGui::GetWindowDrawList();
    if (item.id != memory.item && !ImGui::IsItemHovered() && !ImGui::IsItemFocused())
      draw->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), 4 * scale);
    draw->PushClipRect(min, max, true);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                  ImVec2(min.x + 16 * scale, min.y + 12 * scale), ImGui::GetColorU32(ImGuiCol_Text),
                  item.label.c_str(), nullptr, wrap_width);
    if (!item.detail.empty())
      draw->AddText(
          ImGui::GetFont(), ImGui::GetFontSize(),
          ImVec2(min.x + 16 * scale, min.y + 20 * scale + label_height),
          ImGui::GetColorU32(item.id == memory.item ? ImGuiCol_Text : ImGuiCol_TextDisabled),
          item.detail.c_str(), nullptr, wrap_width);
    draw->PopClipRect();
    if (clicked && item.enabled)
      result.activated = item.id;
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  memory.scroll = m_restore_frames ? m_restore_scroll : ImGui::GetScrollY() / scale;
  if (m_restore_frames)
    --m_restore_frames;
  ImGui::EndChild();
  ImGui::PushFont(font, 18 * scale / font_scale);
  ImGui::TextWrapped("%s", prompts.c_str());
  ImGui::PopFont();
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  ImGui::End();
  ImGui::PopFont();
  ImGui::PopStyleVar(2);
  return result;
}
}  // namespace VideoCommon::BigPicture
