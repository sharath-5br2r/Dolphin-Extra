// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui_internal.h>

#include "VideoCommon/BigPicture/Style.h"

namespace VideoCommon::BigPicture
{
// ImGui wraps only at spaces and splits a long word anywhere, but values are often paths.
// Break after path and word separators instead and keep at most two lines; a value that still
// does not fit loses its start, where long paths differ least.
std::vector<std::string> WrapSettingValue(std::string_view value, float width)
{
  struct Glyph
  {
    std::size_t begin;
    std::size_t end;
    float width;
  };
  std::vector<Glyph> glyphs;
  for (std::size_t i = 0; i < value.size();)
  {
    std::size_t end = i + 1;
    while (end < value.size() && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80)
      ++end;
    glyphs.push_back({i, end, ImGui::CalcTextSize(value.data() + i, value.data() + end).x});
    i = end;
  }
  const auto separator = [&](std::size_t glyph) {
    return std::string_view(" /\\-_.,").find(value[glyphs[glyph].begin]) != std::string_view::npos;
  };
  // Glyph ranges of each line, starting at `first`, with the first line indented by `indent`.
  const auto wrap = [&](std::size_t first, float indent) {
    std::vector<std::pair<std::size_t, std::size_t>> lines;
    std::size_t start = first;
    std::size_t after_separator = 0;
    float line = indent;
    for (std::size_t i = first; i < glyphs.size(); ++i)
    {
      if (i > start && line + glyphs[i].width > width)
      {
        const std::size_t end = after_separator > start ? after_separator : i;
        lines.emplace_back(start, end);
        start = end;
        line = 0;
        after_separator = 0;
        for (std::size_t j = start; j < i; ++j)
        {
          line += glyphs[j].width;
          if (separator(j))
            after_separator = j + 1;
        }
      }
      line += glyphs[i].width;
      if (separator(i))
        after_separator = i + 1;
    }
    if (start < glyphs.size())
      lines.emplace_back(start, glyphs.size());
    return lines;
  };
  std::size_t first = 0;
  auto lines = wrap(0, 0);
  const float ellipsis = ImGui::CalcTextSize("...").x;
  while (lines.size() > 2 && first + 1 < glyphs.size())
    lines = wrap(++first, ellipsis);
  std::vector<std::string> text;
  for (const auto& [begin, end] : lines)
  {
    text.emplace_back(text.empty() && first != 0 ? "..." : "");
    text.back() += value.substr(glyphs[begin].begin, glyphs[end - 1].end - glyphs[begin].begin);
  }
  return text;
}

MenuResult NavigationToolkit::DrawSettings(const MenuPage& page,
                                           ::BigPicture::NavigationOptions options,
                                           const std::string& prompts, ImFont* font)
{
  MenuResult result;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  const auto display = ImGui::GetIO().DisplaySize;
  const float scale = std::max(0.01f, std::min(display.x / 1280, display.y / 720) * options.scale);
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  const float header = Style::BAR_HEIGHT * scale;
  const float footer = Style::FOOTER_HEIGHT * scale;
  const float margin = std::max(12 * scale, display.x * options.safe_area);
  const float top = std::max(header, display.y * options.safe_area);
  const float bottom =
      std::min(display.y - footer, display.y * (1 - options.safe_area)) - 16 * scale;
  const float content_width = std::max(scale, display.x - margin * 2);
  auto& memory = m_memory[page.id];
  const auto row = [&](const std::string& id) {
    return std::ranges::find_if(
        page.items, [&](const auto& item) { return item.id == id && (item.enabled || page.busy); });
  };
  const auto header_item = [&](const std::string& id) {
    return std::ranges::any_of(page.toolbar, [&](const auto& item) {
      return item.id == id && (item.enabled || page.busy);
    });
  };
  // Clicking another category focuses its tab before this page closes. Coming back to the page
  // (e.g. with the keyboard), focus returns to its rows, not to that tab.
  if (m_current_page != page.id && std::ranges::any_of(page.toolbar, [&](const auto& item) {
        return item.id == memory.item && item.id != "back" && !item.active;
      }))
  {
    memory.item = row(memory.selected) != page.items.end() ? memory.selected : std::string{};
  }
  const auto previous = memory.item;
  if (row(memory.item) == page.items.end() && !header_item(memory.item))
  {
    auto first = row(page.default_focus);
    if (first == page.items.end())
      first = std::ranges::find_if(page.items, &MenuItem::enabled);
    memory.item = first == page.items.end() ? "back" : first->id;
  }
  if (row(memory.selected) == page.items.end())
    memory.selected = memory.item;
  const auto focus_id = memory.item;
  const bool entering = TrackLayout(page.id, display, scale).Any() || previous != memory.item ||
                        GImGui->NavWindow == nullptr || (m_settings_busy && !page.busy);
  m_settings_busy = page.busy;

  const Style::ScopedMenuStyle style(
      palette, {.background = palette.background, .scrollbar_size = 12 * scale});
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureSettings", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, header), ImGui::GetColorU32(palette.header));
  draw->AddRectFilled(ImVec2(0, display.y - footer), display, ImGui::GetColorU32(palette.header));
  // The header bar marks one control with an underline: the one under the pointer or keyboard
  // focus, otherwise the open category. Buttons draw no hover square of their own.
  std::vector<std::pair<float, float>> underlined;
  std::optional<std::pair<float, float>> hovered_button;
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.header);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.header);
  const auto button = [&](const MenuItem& item, float x, float width) {
    ImGui::SetCursorPos(ImVec2(x, 0));
    ImGui::PushID(item.id.c_str());
    if (entering && focus_id == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    const bool pressed = ImGui::Button("##category", ImVec2(width, header));
    // Refocusing the window on a new page briefly restores ImGui's last focused header button,
    // such as a category clicked earlier, until this page's own focus applies. Keyboard focus
    // marks only the control that the page is restoring.
    const bool hovered = ImGui::IsItemHovered() && (m_pointer || !entering || focus_id == item.id);
    const float icon_size = std::min(24 * scale, width - 8 * scale);
    auto color = palette.header_text;
    if (!item.active && !hovered && item.id != "back")
      color.w *= 0.55f;
    DrawMenuIcon(draw, item.icon,
                 ImVec2(x + (width - icon_size) * 0.5f, (header - icon_size) * 0.5f), icon_size,
                 ImGui::GetColorU32(color));
    if (hovered)
      hovered_button = std::pair{x, width};
    if (item.active)
      underlined.emplace_back(x, width);
    if (ImGui::IsItemFocused() && !entering)
      memory.item = item.id;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s%s%s", item.label.c_str(), item.detail.empty() ? "" : "\n",
                        item.detail.c_str());
    ImGui::EndDisabled();
    ImGui::PopID();
    return pressed;
  };
  std::vector<const MenuItem*> categories;
  std::vector<const MenuItem*> context;
  for (const auto& item : page.toolbar)
  {
    if (item.id == "back")
    {
      if (button(item, 5 * scale, 48 * scale))
        result.activated = item.id;
    }
    else if (item.id == "global" || item.id == "game")
      context.push_back(&item);
    else
      categories.push_back(&item);
  }
  const int count = static_cast<int>(categories.size() + context.size());
  const float tab_width = std::min(56 * scale, display.x * 0.68f / std::max(1, count));
  const float tabs_x = display.x - 8 * scale - count * tab_width;
  draw->PushClipRect(ImVec2(63 * scale, 0),
                     ImVec2(std::max(63 * scale, tabs_x - 10 * scale), header), true);
  draw->AddText(ImVec2(63 * scale, (header - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(palette.header_text), page.title.c_str());
  draw->PopClipRect();
  float x = tabs_x;
  for (const auto* item : categories)
  {
    if (button(*item, x, tab_width))
      result.activated = item->id;
    x += tab_width;
  }
  for (const auto* item : context)
  {
    if (button(*item, x, tab_width))
      result.activated = item->id;
    x += tab_width;
  }
  ImGui::PopStyleColor(2);
  if (hovered_button)
    underlined = {*hovered_button};
  for (const auto& [left, width] : underlined)
  {
    draw->AddRectFilled(ImVec2(left + 10 * scale, header - 2 * scale),
                        ImVec2(left + width - 10 * scale, header),
                        ImGui::GetColorU32(palette.header_text));
  }
  ImGui::SetCursorPos(ImVec2(0, top));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 16 * scale));
  ImGui::BeginChild("##settings-rows", ImVec2(display.x, std::max(scale, bottom - top)),
                    ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  if (entering)
    ImGui::SetScrollY(memory.scroll * scale);
  ImGui::SetCursorPosX(margin + 16 * scale);
  // Revealing the first row also reveals the explanation above it.
  const float content_top = ImGui::GetCursorScreenPos().y;
  const auto context_end = page.description.find('\n');
  const auto explanation = !page.category_page ? page.description :
                           context_end == std::string::npos ?
                                                 std::string{} :
                                                 page.description.substr(context_end + 1);
  if (!explanation.empty())
  {
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    ImGui::PushTextWrapPos(display.x - margin - 16 * scale);
    ImGui::TextUnformatted(explanation.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 12 * scale));
  }
  std::string section;
  for (const auto& item : page.items)
  {
    // Revealing a row that starts a section also reveals the section's heading.
    const bool first = &item == page.items.data();
    const bool heading = item.section != section && !item.section.empty();
    const float heading_top = first ? content_top : ImGui::GetCursorScreenPos().y;
    if (heading)
    {
      section = item.section;
      ImGui::SetCursorPosX(margin + 16 * scale);
      ImGui::Dummy(ImVec2(0, &item == page.items.data() ? 0 : 10 * scale));
      ImGui::SetCursorPosX(margin + 16 * scale);
      const auto min = ImGui::GetCursorScreenPos();
      auto* rows = ImGui::GetWindowDrawList();
      rows->AddText(min, ImGui::GetColorU32(palette.secondary), section.c_str());
      rows->AddLine(ImVec2(min.x, min.y + 28 * scale),
                    ImVec2(display.x - margin - 16 * scale, min.y + 28 * scale),
                    ImGui::GetColorU32(palette.secondary), scale);
      ImGui::Dummy(ImVec2(0, 34 * scale));
    }
    const float width = std::max(scale, content_width - 12 * scale);
    const float value_width = item.toggle        ? 62 * scale :
                              item.value.empty() ? 0 :
                                                   std::min(320 * scale, width * 0.32f);
    const float label_width = std::max(scale, width - 64 * scale - value_width);
    const auto title_size = ImGui::CalcTextSize(item.label.c_str(), nullptr, false, label_width);
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    const float help_width = std::max(scale, width - 32 * scale - value_width);
    // Keep long desktop tooltips from filling the viewport. The options editor retains the
    // complete description, including requirements and the current override state.
    std::string summary = item.detail;
    const char* line = summary.c_str();
    const char* end = line + summary.size();
    for (int i = 0; i < 3 && line < end; ++i)
    {
      const auto* paragraph_end = std::find(line, end, '\n');
      const float wrap_width =
          i == 2 ? std::max(scale, help_width - ImGui::CalcTextSize("...").x) : help_width;
      const auto* next = paragraph_end == line ?
                             line + 1 :
                             ImGui::GetFont()->CalcWordWrapPosition(ImGui::GetFontSize(), line,
                                                                    paragraph_end, wrap_width);
      if (next == line)
        break;
      if (next == paragraph_end && paragraph_end < end)
        ++next;
      line = next;
      while (line < end && (*line == ' ' || *line == '\r' || *line == '\t'))
        ++line;
      if (i == 2 && line < end)
      {
        summary.resize(static_cast<std::size_t>(line - summary.c_str()));
        while (!summary.empty() &&
               (summary.back() == '\n' || summary.back() == '\r' || summary.back() == ' '))
          summary.pop_back();
        summary += "...";
      }
    }
    const auto help_size = ImGui::CalcTextSize(summary.c_str(), nullptr, false, help_width);
    ImGui::PopFont();
    auto visible_value = item.value;
    if (visible_value.size() > 96)
    {
      std::size_t start = visible_value.size() - 96;
      while (start < visible_value.size() &&
             (static_cast<unsigned char>(visible_value[start]) & 0xc0) == 0x80)
        ++start;
      visible_value = "..." + visible_value.substr(start);
    }
    const auto value_lines =
        WrapSettingValue(visible_value, std::max(scale, value_width - 16 * scale));
    ImVec2 value_size(0, ImGui::GetFontSize() * value_lines.size());
    for (const auto& value_line : value_lines)
      value_size.x = std::max(value_size.x, ImGui::CalcTextSize(value_line.c_str()).x);
    // A slider adds its track and range labels below the label and summary.
    const float slider_height = item.slider ? 46 * scale : 0;
    const float height =
        std::max({item.detail.empty() ? 48 * scale : 72 * scale,
                  title_size.y + (item.detail.empty() ? 0 : help_size.y + 6 * scale) + 22 * scale,
                  item.toggle || item.value.empty() ? 0 : value_size.y + 22 * scale}) +
        slider_height;
    ImGui::SetCursorPosX(margin);
    ImGui::PushID(item.id.c_str());
    if (entering && focus_id == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    const bool pressed = ImGui::Selectable(
        "##setting", false, ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(width, height));
    ImGui::PopStyleColor();
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    if (pressed)
      result.activated = item.id;
    // The slider track spans the row below its label: [track_left, track_right] at track_y.
    const float track_left = min.x + 48 * scale;
    const float track_right = std::max(track_left + scale, max.x - 24 * scale);
    const float track_y = max.y - 30 * scale;
    if (item.slider && item.enabled && !page.busy && ImGui::IsItemActive() &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
      // Pressing or dragging on the row sets the value under the pointer, in whole steps.
      const auto& slider = *item.slider;
      const double span = slider.maximum - slider.minimum;
      if (span > 0 && slider.step > 0)
      {
        const double fraction = std::clamp(
            (ImGui::GetIO().MousePos.x - track_left) / (track_right - track_left), 0.0f, 1.0f);
        const double value =
            std::clamp(slider.minimum + std::round(fraction * span / slider.step) * slider.step,
                       slider.minimum, slider.maximum);
        const auto sent = std::tuple{item.id, value, slider.value};
        if (std::abs(value - slider.value) >= slider.step * 0.5 && m_slider_sent != sent)
        {
          result.activated = item.id + ":set";
          result.value = value;
          m_slider_sent = sent;
        }
      }
    }
    const bool options_clicked = item.has_options && ImGui::IsItemClicked(ImGuiMouseButton_Right);
    if (options_clicked)
    {
      result.activated = "edit:" + item.id;
      ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
    }
    if (pressed || options_clicked ||
        (ImGui::IsItemFocused() && (!entering || focus_id == item.id)))
    {
      memory.item = item.id;
      memory.selected = item.id;
      if (entering || GImGui->NavJustMovedToId == ImGui::GetItemID())
      {
        ImRect reveal(ImVec2(min.x, first || heading ? heading_top : min.y), max);
        reveal.Expand(ImVec2(0, 16 * scale));
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
      const float label_x = min.x + 48 * scale;
      DrawMenuIcon(rows, item.icon, ImVec2(min.x + 18 * scale, min.y + 12 * scale), 22 * scale,
                   ImGui::GetColorU32(palette.text));
      rows->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(label_x, min.y + 10 * scale),
                    ImGui::GetColorU32(palette.text), item.label.c_str(), nullptr, label_width);
      ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
      rows->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(min.x + 16 * scale, min.y + 16 * scale + title_size.y),
                    ImGui::GetColorU32(palette.secondary), summary.c_str(), nullptr, help_width);
      ImGui::PopFont();
      auto value_color = palette.text;
      if (!item.value_enabled)
        value_color.w *= 0.5f;
      if (item.toggle)
      {
        const float h = 24 * scale;
        const ImVec2 switch_min(max.x - 66 * scale, min.y + (height - h) * 0.5f);
        const ImVec2 switch_max(switch_min.x + 50 * scale, switch_min.y + h);
        auto track = *item.toggle ? palette.focus : Style::ColorFromHex(0x888888);
        if (!item.value_enabled)
          track.w *= 0.5f;
        rows->AddRectFilled(switch_min, switch_max, ImGui::GetColorU32(track), h * 0.5f);
        rows->AddCircleFilled(
            ImVec2(*item.toggle ? switch_max.x - h * 0.5f : switch_min.x + h * 0.5f,
                   switch_min.y + h * 0.5f),
            h * 0.5f - 2 * scale, ImGui::GetColorU32(value_color));
      }
      else if (!item.value.empty())
      {
        // Each line is right-aligned.
        float y = min.y + (height - slider_height - value_size.y) * 0.5f;
        for (const auto& value_line : value_lines)
        {
          rows->AddText(ImVec2(max.x - 16 * scale - ImGui::CalcTextSize(value_line.c_str()).x, y),
                        ImGui::GetColorU32(value_color), value_line.c_str());
          y += ImGui::GetFontSize();
        }
      }
      else if (item.active)
        DrawMenuIcon(rows, MenuIcon::Check,
                     ImVec2(max.x - 40 * scale, min.y + (height - 22 * scale) * 0.5f), 22 * scale,
                     ImGui::GetColorU32(palette.text));
    }
    if (item.slider && ImGui::IsItemVisible())
    {
      const auto& slider = *item.slider;
      auto* rows = ImGui::GetWindowDrawList();
      const double span = slider.maximum - slider.minimum;
      const float fraction =
          span > 0 ?
              static_cast<float>(std::clamp((slider.value - slider.minimum) / span, 0.0, 1.0)) :
              0.0f;
      const float knob_x = track_left + (track_right - track_left) * fraction;
      const float thickness = 3 * scale;
      auto fill = palette.focus;
      if (!item.enabled)
        fill.w *= 0.5f;
      rows->AddRectFilled(ImVec2(track_left, track_y - thickness),
                          ImVec2(track_right, track_y + thickness),
                          ImGui::GetColorU32(palette.hover), thickness);
      rows->AddRectFilled(ImVec2(track_left, track_y - thickness),
                          ImVec2(knob_x, track_y + thickness), ImGui::GetColorU32(fill), thickness);
      rows->AddCircleFilled(ImVec2(knob_x, track_y), 9 * scale, ImGui::GetColorU32(palette.text));
      ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
      const float label_y = track_y + 10 * scale;
      rows->AddText(ImVec2(track_left, label_y), ImGui::GetColorU32(palette.secondary),
                    slider.minimum_label.c_str());
      rows->AddText(
          ImVec2(track_right - ImGui::CalcTextSize(slider.maximum_label.c_str()).x, label_y),
          ImGui::GetColorU32(palette.secondary), slider.maximum_label.c_str());
      ImGui::PopFont();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    m_slider_sent.reset();
  memory.scroll = ImGui::GetScrollY() / scale;
  ImGui::EndChild();

  // A stepper page uses Left and Right (with key repeat) for its value; the shoulder buttons
  // still change the settings page.
  const bool stepper = !page.increase.empty() && !page.busy;
  const bool step_down = stepper && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ||
                                     ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft));
  const bool step_up = stepper && (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ||
                                   ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight));
  // Holding a direction, or Page Up/Down, moves in large steps.
  const auto held = [](ImGuiKey key, ImGuiKey pad) {
    return std::max(ImGui::GetKeyData(key)->DownDuration, ImGui::GetKeyData(pad)->DownDuration);
  };
  const bool page_down =
      stepper && !page.decrease_large.empty() && ImGui::IsKeyPressed(ImGuiKey_PageDown);
  const bool page_up =
      stepper && !page.increase_large.empty() && ImGui::IsKeyPressed(ImGuiKey_PageUp);
  if (step_down || step_up)
  {
    ImGui::NavMoveRequestCancel();
    const auto& large = step_down ? page.decrease_large : page.increase_large;
    const bool held_long =
        (step_down ? held(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft) :
                     held(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) > 0.6f;
    result.activated =
        held_long && !large.empty() ? large : (step_down ? page.decrease : page.increase);
  }
  else if (page_down || page_up)
  {
    ImGui::NavMoveRequestCancel();
    result.activated = page_down ? page.decrease_large : page.increase_large;
  }
  int direction = 0;
  if ((!stepper && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) ||
      ImGui::IsKeyPressed(ImGuiKey_GamepadL1))
    direction = -1;
  if ((!stepper && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) ||
      ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
    direction = 1;
  if (direction && !categories.empty())
  {
    ImGui::NavMoveRequestCancel();
    const auto active =
        std::ranges::find_if(categories, [](const auto* item) { return item->active; });
    int index = active == categories.end() ? 0 : static_cast<int>(active - categories.begin());
    const int size = static_cast<int>(categories.size());
    for (int attempt = 0; attempt < size; ++attempt)
    {
      index = (index + direction + size) % size;
      if (categories[index]->enabled)
      {
        result.activated = categories[index]->id;
        break;
      }
    }
  }
  if (!page.busy && (ImGui::IsKeyPressed(ImGuiKey_F2, false) ||
                     (row(memory.item) != page.items.end() &&
                      ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, false))))
  {
    const auto selected = row(memory.selected);
    if (selected != page.items.end() && selected->has_options)
    {
      ImGui::NavMoveRequestCancel();
      result.activated = "edit:" + selected->id;
    }
  }
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  const auto prompt =
      Style::DrawFooterText(draw, ImVec2(15 * scale, display.y - footer), display, prompts,
                            ImGui::GetColorU32(palette.header_text), 15 * scale);
  if (page.category_page)
  {
    draw->PushClipRect(ImVec2(15 * scale, display.y - footer),
                       ImVec2(std::max(15 * scale, prompt.x - 20 * scale), display.y), true);
    const auto end = page.description.find('\n');
    draw->AddText(ImVec2(15 * scale, prompt.y), ImGui::GetColorU32(palette.header_text),
                  page.description.c_str(),
                  page.description.c_str() +
                      (end == std::string::npos ? page.description.size() : end));
    draw->PopClipRect();
  }
  ImGui::PopFont();
  ImGui::End();
  ImGui::PopFont();
  return result;
}
}  // namespace VideoCommon::BigPicture
