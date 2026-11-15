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
NavigationToolkit::DrawAchievements(const MenuPage& page, ::BigPicture::NavigationOptions options,
                                    const std::string& prompts, ImFont* font,
                                    const std::function<ImTextureID(const std::string&)>& image)
{
  MenuResult result;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  const auto display = ImGui::GetIO().DisplaySize;
  const float scale = std::max(0.01f, std::min(display.x / 1280, display.y / 720) * options.scale);
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  const float margin = std::max(12 * scale, display.x * options.safe_area);
  const float footer = Style::FOOTER_HEIGHT * scale;
  const float sidebar = std::min(200 * scale, display.x * 0.28f);
  auto& memory = m_memory[page.id];
  const auto previous_subset = memory.selected;
  const auto subset_key = [](const auto& subset) { return "subset:" + std::to_string(subset.id); };
  const auto* subsets = page.achievements ? &page.achievements->subsets : nullptr;
  const ::BigPicture::AchievementSubset* selected = nullptr;
  if (subsets && !subsets->empty())
  {
    auto it = std::ranges::find_if(
        *subsets, [&](const auto& subset) { return subset_key(subset) == memory.selected; });
    if (it == subsets->end())
      it = subsets->begin();
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false))
      it = std::next(it) == subsets->end() ? subsets->begin() : std::next(it);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false))
      it = it == subsets->begin() ? std::prev(subsets->end()) : std::prev(it);
    selected = &*it;
    memory.selected = subset_key(*selected);
  }
  const bool subset_changed = previous_subset != memory.selected;
  if (selected && subset_changed)
    memory.item = subset_key(*selected);
  bool entering =
      TrackLayout(page.id, display, scale).Any() || subset_changed || GImGui->NavWindow == nullptr;
  bool valid_focus = memory.item == "back";
  if (subsets)
    for (const auto& subset : *subsets)
      valid_focus |= memory.item == subset_key(subset);
  if (selected)
    for (const auto& group : selected->groups)
    {
      const auto group_id =
          "group:" + std::to_string(selected->id) + ":" + std::to_string(group.id);
      valid_focus |= memory.item == group_id;
      if (!m_collapsed_achievement_groups.contains(group_id))
        for (const auto& achievement : group.achievements)
          valid_focus |= memory.item == "achievement:" + std::to_string(achievement.id);
    }
  const bool arrived = selected && memory.revision == 0;
  memory.revision = selected ? 1 : 0;
  if (!valid_focus || arrived)
  {
    memory.item = selected ? subset_key(*selected) : "back";
    entering = true;
  }

  auto backdrop = palette.background;
  backdrop.w = page.overlay ? 0.88f : 1;
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), display,
                                                ImGui::GetColorU32(backdrop));
  const Style::ScopedMenuStyle style(
      palette, {.item_spacing = ImVec2(0, 6 * scale), .scrollbar_size = 10 * scale});
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);
  const auto color = [&](const auto& value) { return ImGui::GetColorU32(value); };
  const auto badge = [&](ImDrawList* draw, const std::string& key, ImVec2 pos, float size) {
    const auto texture = image ? image(key) : ImTextureID{};
    if (texture)
      draw->AddImage(texture, pos, ImVec2(pos.x + size, pos.y + size));
    else
      DrawMenuIcon(draw, MenuIcon::Achievements, pos, size, color(palette.secondary));
  };
  const auto highlight = [&](ImDrawList* draw, ImVec2 min, ImVec2 max) {
    if (ImGui::IsItemHovered())
    {
      draw->AddRectFilled(min, max, color(palette.hover), 6 * scale);
      draw->AddRect(min, max, color(palette.secondary), 6 * scale, 0, scale);
    }
  };
  const auto progress = [&](ImDrawList* draw, ImVec2 min, float width, float value,
                            const std::string& label) {
    const float height = 20 * scale;
    value = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0;
    draw->AddRectFilled(min, ImVec2(min.x + width, min.y + height), IM_COL32(0, 0, 0, 255));
    draw->AddRectFilled(min, ImVec2(min.x + width * value, min.y + height),
                        color(palette.selected));
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    const auto text = ImGui::CalcTextSize(label.c_str());
    draw->PushClipRect(min, ImVec2(min.x + width, min.y + height), true);
    draw->AddText(ImVec2(min.x + (width - text.x) * 0.5f, min.y + (height - text.y) * 0.5f),
                  IM_COL32_WHITE, label.c_str());
    draw->PopClipRect();
    ImGui::PopFont();
  };
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  const float summary_width = std::max(scale, display.x - margin * 2 - 102 * scale);
  const float summary_height =
      selected ?
          std::min(
              28 * scale,
              ImGui::CalcTextSize(selected->summary.c_str(), nullptr, false, summary_width).y) :
          14 * scale;
  ImGui::PopFont();
  const float header = std::max(104 * scale, summary_height + 86 * scale);
  const float top = std::max(header, display.y * options.safe_area);
  const float bottom = std::min(display.y - footer, display.y * (1 - options.safe_area));
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureAchievements", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, header), color(palette.header));
  if (selected)
    badge(draw, "achievement:subset:" + std::to_string(selected->id), ImVec2(margin, 8 * scale),
          86 * scale);
  const float header_x = margin + (selected ? 98 * scale : 0);
  draw->PushClipRect(ImVec2(header_x, 0), ImVec2(display.x - margin - 30 * scale, 40 * scale),
                     true);
  draw->AddText(ImVec2(header_x, 8 * scale), color(palette.header_text),
                selected ? selected->title.c_str() : page.title.c_str());
  draw->PopClipRect();
  if (selected)
  {
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    draw->PushClipRect(ImVec2(header_x, 42 * scale),
                       ImVec2(display.x - margin, header - 32 * scale), true);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(header_x, 42 * scale),
                  color(palette.header_text), selected->summary.c_str(), nullptr, summary_width);
    draw->PopClipRect();
    ImGui::PopFont();
    progress(draw, ImVec2(header_x, header - 30 * scale), display.x - margin - header_x,
             selected->progress, selected->progress_label);
  }
  ImGui::SetCursorPos(ImVec2(display.x - margin - 24 * scale, 6 * scale));
  if (entering && memory.item == "back")
    ImGui::SetKeyboardFocusHere();
  ImGui::PushStyleColor(ImGuiCol_Text, palette.header_text);
  if (ImGui::Button("X##close", ImVec2(24 * scale, 24 * scale)))
    result.activated = "back";
  ImGui::PopStyleColor();
  if (ImGui::IsItemFocused())
    memory.item = "back";

  ImGui::SetCursorPos(ImVec2(margin, top));
  ImGui::BeginChild("##achievement-subsets", ImVec2(sidebar, std::max(scale, bottom - top)),
                    ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoSavedSettings);
  bool focus_content = false;
  if (subsets)
    for (const auto& subset : *subsets)
    {
      const auto id = subset_key(subset);
      ImGui::PushID(id.c_str());
      if (entering && memory.item == id)
        ImGui::SetKeyboardFocusHere();
      const bool pressed =
          ImGui::Selectable("##subset", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                            ImVec2(sidebar - 8 * scale, 46 * scale));
      if (pressed)
      {
        entering = true;
        selected = &subset;
        memory.selected = id;
        focus_content = !ImGui::IsMouseClicked(ImGuiMouseButton_Left);
      }
      if (ImGui::IsItemFocused())
        memory.item = id;
      auto* side = ImGui::GetWindowDrawList();
      const auto min = ImGui::GetItemRectMin();
      const auto max = ImGui::GetItemRectMax();
      highlight(side, min, max);
      if (selected && selected->id == subset.id)
        side->AddRectFilled(ImVec2(min.x + 4 * scale, min.y + 10 * scale),
                            ImVec2(min.x + 8 * scale, max.y - 10 * scale), color(palette.focus));
      side->PushClipRect(min, max, true);
      badge(side, "achievement:subset:" + std::to_string(subset.id),
            ImVec2(min.x + 14 * scale, min.y + 9 * scale), 28 * scale);
      ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
      side->AddText(ImVec2(min.x + 50 * scale, min.y + 15 * scale), color(palette.text),
                    subset.title.c_str());
      ImGui::PopFont();
      side->PopClipRect();
      ImGui::PopID();
    }
  ImGui::EndChild();

  const float content_x = margin + sidebar + 16 * scale;
  const float content_width = std::max(scale, display.x - margin - content_x);
  const std::string content_id =
      selected ? "achievement-content:" + std::to_string(selected->id) : "achievement-content";
  auto& content_memory = m_memory[content_id];
  const bool switching = content_memory.selected != memory.selected;
  content_memory.selected = memory.selected;
  ImGui::SetCursorPos(ImVec2(content_x, top));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * scale, 10 * scale));
  ImGui::BeginChild("##achievement-list", ImVec2(content_width, std::max(scale, bottom - top)),
                    ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  if (entering || switching)
    ImGui::SetScrollY(content_memory.scroll * scale);
  if (!selected || selected->groups.empty())
  {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(page.empty_text.c_str());
    ImGui::PopTextWrapPos();
  }
  if (selected)
    for (const auto& group : selected->groups)
    {
      const std::string group_id =
          "group:" + std::to_string(selected->id) + ":" + std::to_string(group.id);
      ImGui::PushID(group_id.c_str());
      if (focus_content || (entering && memory.item == group_id))
      {
        ImGui::SetKeyboardFocusHere();
        focus_content = false;
      }
      const float row_width = std::max(scale, ImGui::GetContentRegionAvail().x);
      if (ImGui::Selectable("##group", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                            ImVec2(row_width, 34 * scale)))
      {
        if (!m_collapsed_achievement_groups.erase(group_id))
          m_collapsed_achievement_groups.insert(group_id);
      }
      if (ImGui::IsItemFocused())
        memory.item = group_id;
      auto* rows = ImGui::GetWindowDrawList();
      auto min = ImGui::GetItemRectMin();
      auto max = ImGui::GetItemRectMax();
      highlight(rows, min, max);
      rows->AddText(ImVec2(min.x + 4 * scale, min.y + 2 * scale), color(palette.text),
                    group.title.c_str());
      rows->AddLine(ImVec2(min.x, max.y - 3 * scale), ImVec2(max.x, max.y - 3 * scale),
                    color(palette.secondary), scale);
      const bool collapsed = m_collapsed_achievement_groups.contains(group_id);
      const ImVec2 chevron(max.x - 18 * scale, min.y + 15 * scale);
      rows->AddLine(ImVec2(chevron.x - 6 * scale, chevron.y + (collapsed ? -3 : 3) * scale),
                    ImVec2(chevron.x, chevron.y + (collapsed ? 3 : -3) * scale),
                    color(palette.text), 2 * scale);
      rows->AddLine(ImVec2(chevron.x, chevron.y + (collapsed ? 3 : -3) * scale),
                    ImVec2(chevron.x + 6 * scale, chevron.y + (collapsed ? -3 : 3) * scale),
                    color(palette.text), 2 * scale);
      ImGui::PopID();
      if (collapsed)
        continue;
      for (const auto& achievement : group.achievements)
      {
        const std::string id = "achievement:" + std::to_string(achievement.id);
        ImGui::PushID(id.c_str());
        const float text_width = std::max(scale, row_width - 160 * scale);
        ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
        const float tag_width =
            achievement.type.empty() ?
                0 :
                std::min(text_width * 0.5f,
                         ImGui::CalcTextSize(achievement.type.c_str()).x + 32 * scale);
        ImGui::PopFont();
        const float title_width = std::max(scale, text_width - tag_width - 8 * scale);
        const auto title_size =
            ImGui::CalcTextSize(achievement.title.c_str(), nullptr, false, title_width);
        const float title_height = title_size.y;
        ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
        const float description_height =
            ImGui::CalcTextSize(achievement.description.c_str(), nullptr, false, text_width).y;
        ImGui::PopFont();
        // rcheevos leaves the text empty when a measurement is unknown or unavailable.
        // A cached percentage alone must not resurrect a progress bar.
        const bool measured = !achievement.unlocked && !achievement.measured.empty();
        const float detail_height = measured                        ? 24 * scale :
                                    achievement.unlocked_at.empty() ? 0 :
                                                                      20 * scale;
        const float height =
            std::max(78 * scale, title_height + description_height + detail_height + 20 * scale);
        if (entering && memory.item == id)
          ImGui::SetKeyboardFocusHere();
        ImGui::Selectable("##achievement", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                          ImVec2(row_width, height));
        if (ImGui::IsItemFocused())
          memory.item = id;
        min = ImGui::GetItemRectMin();
        max = ImGui::GetItemRectMax();
        if (ImGui::IsItemVisible())
        {
          highlight(rows, min, max);
          rows->PushClipRect(min, max, true);
          badge(rows, "achievement:badge:" + std::to_string(achievement.id),
                ImVec2(min.x + 4 * scale, min.y + 6 * scale), 50 * scale);
          const float text_x = min.x + 66 * scale;
          rows->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(text_x, min.y + 4 * scale),
                        color(palette.text), achievement.title.c_str(), nullptr, title_width);
          if (!achievement.type.empty())
          {
            ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
            const auto tag_size = ImGui::CalcTextSize(achievement.type.c_str());
            const ImVec2 tag(text_x + std::min(title_size.x, title_width) + 8 * scale,
                             min.y + 6 * scale);
            rows->AddRectFilled(tag, ImVec2(tag.x + tag_width - 8 * scale, tag.y + 19 * scale),
                                IM_COL32(156, 112, 195, 255), 3 * scale);
            rows->PushClipRect(tag, ImVec2(tag.x + tag_width - 8 * scale, tag.y + 19 * scale),
                               true);
            DrawMenuIcon(rows, MenuIcon::Reset, ImVec2(tag.x + 3 * scale, tag.y + 3 * scale),
                         13 * scale, IM_COL32_WHITE);
            rows->AddText(ImVec2(tag.x + 20 * scale, tag.y + (19 * scale - tag_size.y) * 0.5f),
                          IM_COL32_WHITE, achievement.type.c_str());
            rows->PopClipRect();
            ImGui::PopFont();
          }
          ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
          const float description_y = min.y + title_height + 9 * scale;
          rows->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(text_x, description_y),
                        color(palette.text), achievement.description.c_str(), nullptr, text_width);
          const float detail_y = min.y + title_height + description_height + 14 * scale;
          if (measured)
            progress(rows, ImVec2(text_x, detail_y), std::max(scale, row_width - 74 * scale),
                     achievement.progress, achievement.measured);
          else if (!achievement.unlocked_at.empty())
            rows->AddText(ImVec2(text_x, detail_y), color(palette.text),
                          achievement.unlocked_at.c_str());
          const float points_x =
              max.x - 8 * scale - ImGui::CalcTextSize(achievement.points.c_str()).x;
          rows->AddText(ImVec2(points_x, description_y), color(palette.text),
                        achievement.points.c_str());
          ImGui::PopFont();
          DrawMenuIcon(rows, achievement.unlocked ? MenuIcon::Unlocked : MenuIcon::Locked,
                       ImVec2(max.x - 34 * scale, min.y + 6 * scale), 22 * scale,
                       color(palette.text));
          rows->PopClipRect();
        }
        ImGui::PopID();
      }
    }
  content_memory.scroll = ImGui::GetScrollY() / scale;
  ImGui::EndChild();
  draw->AddRectFilled(ImVec2(0, display.y - footer), display, color(palette.header));
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  Style::DrawFooterText(draw, ImVec2(0, display.y - footer), display, prompts,
                        color(palette.header_text), margin);
  ImGui::PopFont();
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::End();
  ImGui::PopFont();
  return result;
}
}  // namespace VideoCommon::BigPicture
