// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"
#include "VideoCommon/BigPicture/Style.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <utility>

#include <imgui_internal.h>

namespace VideoCommon::BigPicture
{
namespace
{
ImU32 Color(const ImVec4& color)
{
  return ImGui::GetColorU32(color);
}

std::string GetClockText()
{
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char clock[32]{};
  std::strftime(clock, sizeof(clock), "%H:%M:%S", &local);
  return clock;
}

void DrawCover(ImDrawList* draw, ImVec2 min, ImVec2 size, ImTextureID texture, float scale,
               const Style::Palette& palette)
{
  const ImVec2 max(min.x + size.x, min.y + size.y);
  if (texture)
    draw->AddImage(texture, min, max);
  else
  {
    draw->AddRectFilled(min, max, Color(palette.header), Style::ROUNDING * scale);
    // A neutral disc motif also works before asynchronous cover loading completes.
    const ImVec2 center(min.x + size.x * 0.5f, min.y + size.y * 0.5f);
    const float radius = std::min(size.x, size.y) * 0.28f;
    draw->AddCircle(center, radius, Color(palette.header_text), 32, 2 * scale);
    draw->AddCircle(center, radius * 0.18f, Color(palette.header_text), 16, 2 * scale);
  }
}
}  // namespace

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
    size.y = std::min(size.y, 320.0f * scale);
  }
  return {{(viewport.x - size.x) * 0.5f, (viewport.y - size.y) * 0.5f}, size, scale};
}

bool HasEnabledItem(const std::vector<MenuItem>& items, std::string_view id)
{
  return std::ranges::any_of(items,
                             [&](const auto& item) { return item.enabled && item.id == id; });
}

NavigationToolkit::LayoutChange NavigationToolkit::TrackLayout(const std::string& page, ImVec2 size,
                                                               float scale)
{
  const LayoutChange change{m_current_page != page, m_layout_width != size.x ||
                                                        m_layout_height != size.y ||
                                                        m_layout_scale != scale};
  m_current_page = page;
  m_layout_width = size.x;
  m_layout_height = size.y;
  m_layout_scale = scale;
  return change;
}

void NavigationToolkit::Reset()
{
  m_current_page.clear();
  m_memory.clear();
  m_restore_frames = 0;
  m_header_height = 0;
  m_collapsed_achievement_groups.clear();
}

FocusMemory NavigationToolkit::GetMemory(const std::string& page) const
{
  const auto it = m_memory.find(page);
  return it != m_memory.end() ? it->second : FocusMemory{};
}

void NavigationToolkit::RestoreMemory(const std::string& page, FocusMemory memory)
{
  m_memory[page] = std::move(memory);
}

MenuResult NavigationToolkit::DrawLanding(
    const MenuPage& page, ::BigPicture::NavigationOptions options, const std::string& prompts,
    ImFont* font, const std::function<ImTextureID(const std::string&)>& image, bool passive)
{
  MenuResult result;
  const bool launch = page.presentation == MenuPresentation::Launch;
  const int columns = launch ? std::clamp(static_cast<int>(page.items.size()), 1, 4) : 4;
  const int rows = std::max(1, (static_cast<int>(page.items.size()) + columns - 1) / columns);
  const auto display = ImGui::GetIO().DisplaySize;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  // The bars reach the screen edges; overscan applies to the usable menu area.
  const float scale =
      std::max(0.01f, std::min({display.x / 1280 * options.scale, display.y / 720 * options.scale,
                                display.x * (1 - options.safe_area * 2) / (columns * 280 - 30),
                                display.y * (1 - options.safe_area * 2) / (rows * 320 + 82)}));
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  auto& memory = m_memory[page.id];
  const auto previous_focus = memory.item;
  const bool valid_focus =
      HasEnabledItem(page.items, memory.item) || HasEnabledItem(page.toolbar, memory.item);
  if (!valid_focus && !passive)
  {
    auto first = std::ranges::find_if(page.items, [&](const auto& item) {
      return item.enabled && item.id == page.default_focus;
    });
    if (first == page.items.end())
      first = std::ranges::find_if(page.items, &MenuItem::enabled);
    memory.item = first == page.items.end() ? "back" : first->id;
  }
  const auto focus_id = memory.item;
  const bool entering = !passive && (m_current_page != page.id || GImGui->NavWindow == nullptr ||
                                     previous_focus != memory.item);
  if (!passive)
    m_current_page = page.id;
  if (memory.item.empty())
    memory.item = page.default_focus;
  const float header = (launch ? Style::BAR_HEIGHT : Style::HEADER_HEIGHT) * scale;
  const float footer = Style::FOOTER_HEIGHT * scale;
  const Style::ScopedMenuStyle style(
      palette, {.background = palette.background, .item_spacing = ImVec2(30 * scale, 0)});
  ImGui::PushFont(font, Style::TITLE_SIZE * scale / font_scale);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  if (entering)
    ImGui::SetNextWindowFocus();
  ImGui::Begin(("###BigPictureLanding-" + page.id).c_str(), nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   (passive ? ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing : 0));
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, header), Color(palette.header));
  draw->AddRectFilled(ImVec2(0, display.y - footer), display, Color(palette.header));
  const auto texture = [&](const std::string& key) { return image ? image(key) : ImTextureID{}; };
  if (launch)
  {
    ImGui::SetCursorPos(ImVec2(5 * scale, 0));
    if (entering && focus_id == "back")
      ImGui::SetKeyboardFocusHere();
    const auto back =
        std::ranges::find_if(page.toolbar, [](const auto& item) { return item.id == "back"; });
    ImGui::BeginDisabled(back == page.toolbar.end() || !back->enabled);
    if (ImGui::Button("##back", ImVec2(48 * scale, header)))
      result.activated = "back";
    DrawMenuIcon(draw, MenuIcon::Back, ImVec2(17 * scale, 10 * scale), 24 * scale,
                 Color(palette.header_text));
    if (ImGui::IsItemFocused() && !entering && !passive)
      memory.item = "back";
    ImGui::EndDisabled();
  }
  else
  {
    if (const auto logo = texture("home:logo"))
      draw->AddImage(logo, ImVec2(15 * scale, 8 * scale), ImVec2(47 * scale, 40 * scale));
  }
  const float header_title_y = launch ? (header - ImGui::GetTextLineHeight()) * 0.5f : 8 * scale;
  draw->AddText(ImVec2(60 * scale, header_title_y), Color(palette.header_text), page.title.c_str());
  if (!launch)
  {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char clock[32]{};
    std::strftime(clock, sizeof(clock), "%H:%M:%S", &local);
    const float clock_x = display.x - 15 * scale - ImGui::CalcTextSize(clock).x;
    draw->AddText(ImVec2(clock_x, 8 * scale), Color(palette.header_text), clock);
    if (!page.account_name.empty())
    {
      // Clip long display names before they can cover either branding or the clock.
      const float right = clock_x - 15 * scale;
      const float left =
          std::max(190 * scale, right - std::min(ImGui::CalcTextSize(page.account_name.c_str()).x,
                                                 display.x * 0.35f));
      draw->PushClipRect(ImVec2(left, 0), ImVec2(right, header), true);
      draw->AddText(ImVec2(left, 8 * scale), Color(palette.header_text), page.account_name.c_str());
      draw->PopClipRect();
      if (const auto avatar = texture("home:avatar"))
        draw->AddImage(avatar, ImVec2(left - 42 * scale, 8 * scale),
                       ImVec2(left - 15 * scale, 35 * scale));
    }
  }
  const float top = header + (display.y - header - footer - rows * 320 * scale) * 0.5f;
  for (std::size_t i = 0; i < page.items.size(); ++i)
  {
    const auto& item = page.items[i];
    const int row = static_cast<int>(i) / columns;
    const int row_columns = std::min(columns, static_cast<int>(page.items.size()) - row * columns);
    const float menu_width = (row_columns * 280 - 30) * scale;
    ImGui::SetCursorPos(ImVec2((display.x - menu_width) * 0.5f + (i % columns) * 280 * scale,
                               top + row * 320 * scale));
    ImGui::PushID(item.id.c_str());
    if (entering && focus_id == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    const bool clicked =
        ImGui::Selectable("##home-action", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                          ImVec2(250 * scale, 320 * scale));
    ImGui::PopStyleColor();
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const bool highlight = ImGui::IsItemHovered();
    if (!passive && (ImGui::IsItemFocused() || clicked))
      memory.item = item.id;
    if (clicked)
      result.activated = item.id;
    if (highlight)
    {
      auto hover = palette.hover;
      hover.w *= 0.7f;
      draw->AddRectFilled(min, max, Color(hover), 6 * scale);
      draw->AddRect(min, max, Color(palette.secondary), 6 * scale, 0, scale);
    }
    draw->PushClipRect(min, max, true);
    const auto color = Color(palette.text);
    const ImVec2 icon_min(min.x + 50 * scale, min.y + 30 * scale);
    if (const auto icon = texture(item.image_key))
      draw->AddImage(icon, icon_min, ImVec2(icon_min.x + 150 * scale, icon_min.y + 150 * scale),
                     ImVec2(0, 0), ImVec2(1, 1), color);
    else
      DrawMenuIcon(draw, item.icon, icon_min, 150 * scale, color);
    const float wrap = 190 * scale;
    const auto title_size = ImGui::CalcTextSize(item.label.c_str(), nullptr, false, wrap);
    const float title_y = min.y + 190 * scale;
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                  ImVec2(min.x + (250 * scale - title_size.x) * 0.5f, title_y), color,
                  item.label.c_str(), nullptr, wrap);
    ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
    const auto detail_size = ImGui::CalcTextSize(item.detail.c_str(), nullptr, false, wrap);
    draw->AddText(
        ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(min.x + (250 * scale - detail_size.x) * 0.5f, title_y + title_size.y + 10 * scale),
        color, item.detail.c_str(), nullptr, wrap);
    ImGui::PopFont();
    draw->PopClipRect();
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  ImGui::PushFont(font, Style::SUMMARY_SIZE * scale / font_scale);
  Style::DrawFooterText(draw, ImVec2(15 * scale, display.y - footer), display, prompts,
                        Color(palette.header_text), 15 * scale);
  ImGui::PopFont();
  if (passive)
  {
    auto dim = palette.text;
    dim.w = 0.25f;
    draw->AddRectFilled(ImVec2(0, 0), display, Color(dim));
  }
  result.back = !passive && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                             ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
  result.focused = memory.item;
  ImGui::End();
  ImGui::PopFont();
  return result;
}
MenuResult NavigationToolkit::Draw(const MenuPage& page, ::BigPicture::NavigationOptions options,
                                   const std::string& prompts, ImFont* font,
                                   const std::function<ImTextureID(const std::string&)>& image)
{
  // Mouse movement, clicks and the wheel switch to pointer highlighting; any key or gamepad
  // button switches back. ImGui reports the focused control as hovered only while
  // NavHighlightItemUnderNav is set, so this keeps exactly one control highlighted: the focused
  // one after keyboard or gamepad input (including focus restored when a page opens), and the
  // one under the pointer after mouse input. ImGui itself ignores the first position after the
  // pointer enters the window, which would leave the keyboard highlight in place.
  const auto& io = ImGui::GetIO();
  const auto pointer = ImGui::IsMousePosValid() ? std::optional(io.MousePos) : std::nullopt;
  const bool pointer_moved = m_last_pointer && pointer &&
                             (!*m_last_pointer || (*m_last_pointer)->x != pointer->x ||
                              (*m_last_pointer)->y != pointer->y);
  m_last_pointer = pointer;
  if (pointer_moved || io.MouseWheel != 0 || io.MouseWheelH != 0 ||
      std::ranges::any_of(io.MouseClicked, [](bool clicked) { return clicked; }))
  {
    m_pointer = true;
  }
  for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_MouseLeft && m_pointer; ++key)
  {
    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
      m_pointer = false;
  }
  GImGui->NavHighlightItemUnderNav = !m_pointer;
  if (!m_pointer)
    GImGui->NavCursorVisible = true;

  MenuResult result;
  if ((page.presentation == MenuPresentation::Landing && page.id == "home") ||
      page.presentation == MenuPresentation::Launch)
    result = DrawLanding(page, options, prompts, font, image);
  else if (page.presentation == MenuPresentation::FilePicker)
    result = DrawFilePicker(page, options, prompts, font, image);
  else if (page.presentation == MenuPresentation::Settings)
    result = DrawSettings(page, options, prompts, font);
  else if (page.presentation == MenuPresentation::Library)
    result = DrawLibrary(page, options, prompts, font, image);
  else if (page.presentation == MenuPresentation::States)
    result = DrawStates(page, options, prompts, font, image);
  else if (page.presentation == MenuPresentation::Achievements)
    result = DrawAchievements(page, options, prompts, font, image);
  else if (page.dialog)
    result = DrawDialog(page, options, prompts, font, image);
  else
    result = DrawList(page, options, prompts, font, image);
  result.fullscreen = ImGui::IsKeyPressed(ImGuiKey_F11, false);
  return result;
}

MenuResult NavigationToolkit::DrawList(const MenuPage& page,
                                       ::BigPicture::NavigationOptions options,
                                       const std::string& prompts, ImFont* font,
                                       const std::function<ImTextureID(const std::string&)>& image)
{
  MenuResult result;
  const auto layout = ComputeMenuLayout(ImGui::GetIO().DisplaySize, options, page.dialog);
  const float scale = layout.scale;
  const bool pause = page.presentation == MenuPresentation::Pause;
  const bool achievements = !page.rich_presence.empty() || !page.achievement_progress.empty();
  const float header_height = (pause ? (achievements ? 130 : 110) : Style::HEADER_HEIGHT) * scale;
  // DisplaySize is already in native pixels. Diagnostic OSD/DPI font scaling must not
  // multiply the menu's separately scaled geometry a second time.
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  auto& memory = m_memory[page.id];
  const auto previous_focus = memory.item;
  const bool revision_changed = m_focus_revision != page.focus_revision;
  const bool entering = TrackLayout(page.id, layout.size, scale).Any() || revision_changed ||
                        m_header_height != header_height;
  m_focus_revision = page.focus_revision;
  m_header_height = header_height;
  // Destructive dialogs always start at their safe default, including another request using
  // the same page. Reuse one memory entry rather than accumulating entries per command ID.
  if (entering && (page.dialog || (page.reset_on_revision && revision_changed)))
    memory = {};
  auto focus = std::ranges::find_if(page.items, [&](const MenuItem& item) {
    return item.enabled && item.id == (memory.item.empty() ? page.default_focus : memory.item);
  });
  bool toolbar_focus = std::ranges::any_of(
      page.toolbar, [&](const auto& item) { return item.enabled && item.id == memory.item; });
  if (focus == page.items.end() && !toolbar_focus)
  {
    toolbar_focus = std::ranges::any_of(page.toolbar, [&](const auto& item) {
      return item.enabled && item.id == page.default_focus;
    });
    if (toolbar_focus)
      memory.item = page.default_focus;
    focus = std::ranges::find_if(page.items, [&](const auto& item) {
      return item.enabled && item.id == page.default_focus;
    });
    if (focus == page.items.end() && !toolbar_focus)
      focus = std::ranges::find_if(page.items, &MenuItem::enabled);
  }
  if (focus != page.items.end())
    memory.item = focus->id;
  else if (!toolbar_focus)
    memory.item.clear();
  const auto focus_id = memory.item;
  const bool restore_focus = entering || previous_focus != focus_id || GImGui->NavWindow == nullptr;
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

  const auto& palette = Style::GetPalette(page.theme);
  const float padding = Style::PADDING * scale;
  const float gap = (pause ? 7 : Style::GAP) * scale;
  const float title_size = Style::TITLE_SIZE * scale;
  const float summary_size = Style::SUMMARY_SIZE * scale;
  const bool landing = page.presentation == MenuPresentation::Landing;
  auto* background = ImGui::GetBackgroundDrawList();
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  auto backdrop = palette.background;
  if (pause)
    backdrop = palette.text.x > 0.5f ?
                   ImVec4(0, 0, 0, 0.72f) :
                   ImVec4(palette.background.x, palette.background.y, palette.background.z, 0.90f);
  else if (page.overlay || page.dialog)
    backdrop.w = 0.80f;
  background->AddRectFilled(ImVec2(0, 0), display, Color(backdrop));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, palette.background);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, palette.text);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, palette.secondary);
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Button, palette.header);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.hover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.selected);
  ImGui::PushStyleColor(ImGuiCol_Border, palette.hover);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, palette.panel);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, palette.hover);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, palette.secondary);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, palette.focus);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, page.dialog ? 8 * scale : 0);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Style::ROUNDING * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(gap, gap));
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10 * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, page.dialog ? scale : 0);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
  ImGui::PushFont(font, title_size / font_scale);
  ImGui::SetNextWindowPos(layout.position);
  ImGui::SetNextWindowSize(layout.size);
  ImGui::SetNextWindowBgAlpha(pause ? 0.0f : 1.0f);
  if (entering || GImGui->NavWindow == nullptr)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureNavigation", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* window_draw = ImGui::GetWindowDrawList();
  const ImVec2 header_max(layout.position.x + layout.size.x, layout.position.y + header_height);
  if (!pause)
    window_draw->AddRectFilled(layout.position, header_max, Color(palette.header),
                               page.dialog ? 8 * scale : 0);
  ImGui::SetCursorPos(ImVec2(padding, gap));
  if (pause)
  {
    DrawCover(window_draw, ImVec2(layout.position.x + padding, layout.position.y + gap),
              ImVec2(60 * scale, 90 * scale),
              image && !page.context.image_key.empty() ? image(page.context.image_key) :
                                                         ImTextureID{},
              scale, palette);
    const ImVec2 text_min(layout.position.x + padding + 80 * scale, layout.position.y + gap);
    // Reserve a separate right-aligned band so long titles and translated metadata
    // cannot paint over the clock or play times.
    const float right = header_max.x - padding;
    const float stats_width = std::min(280 * scale, layout.size.x * 0.35f);
    const ImVec2 text_max(right - stats_width - gap, header_max.y);
    window_draw->PushClipRect(text_min, text_max, true);
    window_draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_min, Color(palette.text),
                         page.context.label.c_str());
    ImGui::PushFont(font, summary_size / font_scale);
    window_draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                         ImVec2(text_min.x, text_min.y + title_size + 4 * scale),
                         Color(palette.text), page.context.detail.c_str());
    float metadata_y =
        text_min.y + title_size + 4 * scale + ImGui::CalcTextSize(page.context.detail.c_str()).y;
    const float metadata_width = std::max(scale, text_max.x - text_min.x);
    if (!page.rich_presence.empty())
    {
      metadata_y += 4 * scale;
      const float presence_height = std::min(
          summary_size * 2,
          ImGui::CalcTextSize(page.rich_presence.c_str(), nullptr, false, metadata_width).y);
      window_draw->PushClipRect(ImVec2(text_min.x, metadata_y),
                                ImVec2(text_max.x, metadata_y + presence_height), true);
      window_draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(text_min.x, metadata_y),
                           Color(palette.text), page.rich_presence.c_str(), nullptr,
                           metadata_width);
      window_draw->PopClipRect();
      metadata_y += presence_height;
    }
    if (!page.achievement_progress.empty())
    {
      metadata_y += 4 * scale;
      DrawMenuIcon(window_draw, MenuIcon::Trophy, ImVec2(text_min.x, metadata_y), summary_size,
                   Color(palette.text));
      window_draw->AddText(ImVec2(text_min.x + summary_size + 5 * scale, metadata_y),
                           Color(palette.text), page.achievement_progress.c_str());
    }
    ImGui::PopFont();
    window_draw->PopClipRect();
    const auto clock = GetClockText();
    window_draw->PushClipRect(ImVec2(right - stats_width, text_min.y), header_max, true);
    window_draw->AddText(ImVec2(right - ImGui::CalcTextSize(clock.c_str()).x, text_min.y),
                         Color(palette.text), clock.c_str());
    ImGui::PushFont(font, summary_size / font_scale);
    const float line_height = ImGui::GetTextLineHeight();
    std::string_view times = page.play_time;
    float time_y = text_min.y + title_size + 4 * scale;
    while (!times.empty())
    {
      const auto end = times.find('\n');
      const auto line = times.substr(0, end);
      const float width = ImGui::CalcTextSize(line.data(), line.data() + line.size()).x;
      window_draw->AddText(ImVec2(right - width, time_y), Color(palette.text), line.data(),
                           line.data() + line.size());
      time_y += line_height;
      if (end == std::string_view::npos)
        break;
      times.remove_prefix(end + 1);
    }
    ImGui::PopFont();
    window_draw->PopClipRect();
  }
  else
  {
    ImGui::PushStyleColor(ImGuiCol_Text, palette.header_text);
    ImGui::TextUnformatted(page.title.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::SetCursorPos(ImVec2(padding, header_height + gap));
  if (!page.toolbar.empty())
  {
    ImGui::PushFont(font, summary_size / font_scale);
    const float toolbar_width =
        (ImGui::GetContentRegionAvail().x - static_cast<float>(page.toolbar.size() - 1) * gap) /
        page.toolbar.size();
    float toolbar_height = Style::SHORT_ROW_HEIGHT * scale;
    for (const auto& item : page.toolbar)
    {
      const float icon_width = item.icon == MenuIcon::None ? 0 : 24 * scale;
      toolbar_height = std::max(
          toolbar_height, ImGui::CalcTextSize(item.label.c_str(), nullptr, false,
                                              std::max(scale, toolbar_width - gap * 2 - icon_width))
                                  .y +
                              gap * 2);
    }
    for (const auto& item : page.toolbar)
    {
      if (&item != &page.toolbar.front())
        ImGui::SameLine();
      ImGui::PushID(item.id.c_str());
      if (restore_focus && item.id == focus_id)
        ImGui::SetKeyboardFocusHere();
      ImGui::BeginDisabled(!item.enabled);
      if (item.active)
      {
        ImGui::PushStyleColor(ImGuiCol_Button, palette.selected);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.selected);
      }
      const bool clicked = ImGui::Button("##toolbar", ImVec2(toolbar_width, toolbar_height));
      if (item.active)
        ImGui::PopStyleColor(2);
      const auto min = ImGui::GetItemRectMin();
      const auto max = ImGui::GetItemRectMax();
      if (item.active)
        window_draw->AddLine(ImVec2(min.x + gap, max.y - 4 * scale),
                             ImVec2(max.x - gap, max.y - 4 * scale), Color(palette.selected_text),
                             2 * scale);
      const auto text_color = Color(item.active            ? palette.selected_text :
                                    ImGui::IsItemHovered() ? palette.text :
                                                             palette.header_text);
      const float icon_width = item.icon == MenuIcon::None ? 0 : 24 * scale;
      window_draw->PushClipRect(min, max, true);
      DrawMenuIcon(window_draw, item.icon,
                   ImVec2(min.x + gap, min.y + (toolbar_height - 18 * scale) * 0.5f), 18 * scale,
                   text_color);
      window_draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                           ImVec2(min.x + gap + icon_width, min.y + gap), text_color,
                           item.label.c_str(), nullptr,
                           std::max(scale, toolbar_width - gap * 2 - icon_width));
      window_draw->PopClipRect();
      if (item.enabled && (clicked || ImGui::IsItemFocused()))
        memory.item = item.id;
      if (clicked)
        result.activated = item.id;
      ImGui::EndDisabled();
      ImGui::PopID();
    }
    ImGui::PopFont();
  }
  // The prompt strip is outside the scrolling region, including on small/high-scale displays.
  ImGui::PushFont(font, summary_size / font_scale);
  const float footer_height =
      pause ? Style::FOOTER_HEIGHT * scale :
              std::max(Style::FOOTER_HEIGHT * scale,
                       ImGui::CalcTextSize(prompts.c_str(), nullptr, false,
                                           std::max(scale, layout.size.x - padding * 2))
                               .y +
                           gap * 2);
  ImGui::PopFont();
  const float content_top = ImGui::GetCursorPosY();
  const float footer_y =
      page.dialog ? layout.position.y + layout.size.y - footer_height : display.y - footer_height;
  const float content_bottom = std::min(layout.position.y + layout.size.y, footer_y - gap);
  const float content_height = std::max(scale, content_bottom - layout.position.y - content_top);
  const float content_width = std::max(scale, ImGui::GetContentRegionAvail().x);
  if (entering || m_restore_frames != 0)
    ImGui::SetNextWindowScroll(
        ImVec2(0, (m_restore_frames ? m_restore_scroll : memory.scroll) * scale));
  ImGui::BeginChild(
      page.id.c_str(),
      ImVec2(pause ? std::min(content_width, Style::ACTION_COLUMN_WIDTH * scale) : content_width,
             content_height),
      page.toolbar.empty() ? ImGuiChildFlags_None : ImGuiChildFlags_NavFlattened,
      pause ? ImGuiWindowFlags_None : ImGuiWindowFlags_AlwaysVerticalScrollbar);
  if (!page.description.empty())
  {
    ImGui::PushFont(font, summary_size / font_scale);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.secondary);
    ImGui::TextWrapped("%s", page.description.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Spacing();
  }

  const bool previous =
      ImGui::IsKeyPressed(ImGuiKey_GamepadL1) || ImGui::IsKeyPressed(ImGuiKey_PageUp);
  const bool next =
      ImGui::IsKeyPressed(ImGuiKey_GamepadR1) || ImGui::IsKeyPressed(ImGuiKey_PageDown);
  const int columns =
      page.artwork && page.columns > 1 ?
          std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / (210 * scale)), 1, 6) :
          std::clamp(page.columns, 1, 6);
  std::string requested_focus;
  if (focus != page.items.end() && (previous || next))
  {
    const int current = static_cast<int>(focus - page.items.begin());
    const float row_height = (page.compact ? 48 :
                              page.artwork ? (columns == 1 ? 90 : 310) :
                                             Style::ROW_HEIGHT + Style::GAP) *
                             scale;
    const int step = std::max(1, static_cast<int>(ImGui::GetWindowHeight() / row_height)) * columns;
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
  const float width =
      std::max(scale, (ImGui::GetContentRegionAvail().x - (columns - 1) * gap) / columns);
  // Large menu text plus overscan margins can leave less than one normal card of height.
  // Shrink artwork before sacrificing the title/summary or making focus impossible to reveal.
  const float card_height_limit =
      std::max(Style::ROW_HEIGHT * scale, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - gap);
  for (std::size_t i = 0; i < page.items.size(); ++i)
  {
    const auto& item = page.items[i];
    if (i % columns != 0)
      ImGui::SameLine();
    const float requested_artwork_height =
        page.artwork ?
            (columns == 1 ? 70 * scale :
                            std::min({210 * scale, width * 1.05f,
                                      std::max(0.0f, card_height_limit - 102 * scale)})) :
            0;
    const float artwork_height =
        columns > 1 && requested_artwork_height < 32 * scale ? 0 : requested_artwork_height;
    const float artwork_width = artwork_height * 2 / 3;
    const bool large_icon =
        landing && columns > 1 && item.icon != MenuIcon::None && card_height_limit > 130 * scale;
    const float icon_width = !page.artwork && !large_icon && item.icon != MenuIcon::None ?
                                 Style::ICON_SIZE * scale + gap :
                                 0;
    const float text_offset = page.artwork && columns == 1 ? artwork_width + gap : icon_width;
    const float text_top =
        large_icon ? std::min(96 * scale, std::max(0.0f, card_height_limit - 90 * scale)) :
        artwork_height > 0 && columns > 1 ? artwork_height + 12 * scale :
                                            0;
    const float wrap_width =
        std::max(scale, width - padding * 2 - text_offset - (item.active ? title_size + gap : 0));
    const float label_height =
        ImGui::CalcTextSize(item.label.c_str(), nullptr, false, wrap_width).y;
    ImGui::PushFont(font, summary_size / font_scale);
    const float detail_height =
        item.detail.empty() ?
            0 :
            ImGui::CalcTextSize(item.detail.c_str(), nullptr, false, wrap_width).y + 4 * scale;
    ImGui::PopFont();
    // Fixed grid cells keep all subsequent rows aligned even with long translated titles.
    const float height =
        columns > 1 ? (page.compact ? 40 * scale :
                       page.artwork ? text_top + 90 * scale :
                                      std::min((landing ? 220 : 150) * scale, card_height_limit)) :
                      std::max((page.compact        ? 40 :
                                page.artwork        ? 90 :
                                item.detail.empty() ? (pause ? 36 : Style::SHORT_ROW_HEIGHT) :
                                                      Style::ROW_HEIGHT) *
                                   scale,
                               label_height + detail_height + gap * 2);
    ImGui::PushID(item.id.c_str());
    const bool request_focus = (restore_focus && item.id == focus_id) || item.id == requested_focus;
    if (request_focus && item.enabled)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    if (pause)
      ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    const bool clicked =
        ImGui::Selectable("##item", item.enabled && item.id == memory.item,
                          ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(width, height));
    if (pause)
      ImGui::PopStyleColor();
    if (item.enabled && (ImGui::IsItemFocused() || clicked))
      memory.item = item.id;
    if (request_focus && !requested_focus.empty())
      ImGui::SetScrollHereY();
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    if ((m_restore_frames == 1 && item.id == focus_id) ||
        (item.id == memory.item && memory.item != previous_focus))
    {
      // On the first resized frame ImGui clamps to the previous content size. Wait for
      // layout to settle before deciding whether wrapping requires revealing the control.
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
    if (clicked && item.enabled)
      result.activated = item.id;
    // Selectable still participates in navigation/scroll restoration when clipped. Custom
    // rounded backgrounds and text must not allocate geometry for thousands of hidden rows.
    if (!ImGui::IsItemVisible())
    {
      ImGui::EndDisabled();
      ImGui::PopID();
      continue;
    }
    auto* draw = ImGui::GetWindowDrawList();
    const bool highlighted = ImGui::IsItemHovered();
    if (highlighted || columns > 1)
      draw->AddRectFilled(min, max, Color(highlighted ? palette.hover : palette.header),
                          Style::ROUNDING * scale);
    if (highlighted)
      draw->AddRect(min, max, Color(palette.secondary), Style::ROUNDING * scale, 0, scale);
    draw->PushClipRect(min, max, true);
    const auto label_color = Color(highlighted ? palette.text :
                                   columns > 1 ? palette.header_text :
                                                 palette.text);
    if (item.active)
      DrawMenuIcon(draw, MenuIcon::Check, ImVec2(max.x - padding - title_size, min.y + gap),
                   title_size, Color(palette.focus));
    if (!page.artwork)
    {
      const float icon_size = large_icon ?
                                  std::min(64 * scale, std::max(0.0f, text_top - 32 * scale)) :
                                  Style::ICON_SIZE * scale;
      DrawMenuIcon(draw, item.icon,
                   large_icon ? ImVec2(min.x + (width - icon_size) * 0.5f, min.y + 24 * scale) :
                                ImVec2(min.x + padding, min.y + gap),
                   icon_size, label_color);
    }
    if (page.artwork && artwork_height > 0 && max.y >= ImGui::GetWindowPos().y &&
        min.y <= ImGui::GetWindowPos().y + ImGui::GetWindowHeight())
    {
      const ImVec2 image_min(min.x + (columns == 1 ? 12 * scale : (width - artwork_width) * 0.5f),
                             min.y + 8 * scale);
      const auto texture = image && !item.image_key.empty() ? image(item.image_key) : ImTextureID{};
      DrawCover(draw, image_min, ImVec2(artwork_width, artwork_height), texture, scale, palette);
    }
    const float title_height = columns > 1 ? std::min(label_height, title_size * 2) : label_height;
    draw->PushClipRect(min, ImVec2(max.x, min.y + gap + text_top + title_height), true);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                  ImVec2(min.x + padding + text_offset, min.y + gap + text_top), label_color,
                  item.label.c_str(), nullptr, wrap_width);
    draw->PopClipRect();
    if (!item.detail.empty())
    {
      ImGui::PushFont(font, summary_size / font_scale);
      // Grid titles have two lines; clipping prevents unusually long titles covering summaries.
      const float detail_top = min.y + gap + text_top + title_height + 4 * scale;
      draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(min.x + padding + text_offset, detail_top),
                    Color(highlighted ? palette.text :
                          columns > 1 ? palette.card_secondary :
                                        palette.secondary),
                    item.detail.c_str(), nullptr, wrap_width);
      ImGui::PopFont();
    }
    draw->PopClipRect();
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  // List menus loop in both directions, including when the opposite end is clipped.
  // Grids retain directional navigation between cards and their toolbar.
  if (columns == 1 && page.toolbar.empty())
    ImGui::NavMoveRequestTryWrapping(ImGui::GetCurrentWindow(), ImGuiNavMoveFlags_LoopY);
  memory.scroll = m_restore_frames ? m_restore_scroll : ImGui::GetScrollY() / scale;
  if (m_restore_frames)
    --m_restore_frames;
  ImGui::EndChild();
  // Match the home screen's thin edge-to-edge bar. Overscan margins belong to
  // the content layout and must not increase the visible footer's thickness.
  const ImVec2 footer_min(page.dialog ? layout.position.x : 0, footer_y);
  const ImVec2 footer_max(page.dialog ? footer_min.x + layout.size.x : display.x,
                          footer_y + footer_height);
  auto* footer_draw = page.dialog ? window_draw : ImGui::GetForegroundDrawList();
  footer_draw->AddRectFilled(footer_min, footer_max, Color(palette.header));
  ImGui::PushFont(font, summary_size / font_scale);
  const float prompt_width = ImGui::CalcTextSize(prompts.c_str()).x;
  const float prompt_x = !page.dialog ?
                             std::max(layout.position.x + padding,
                                      layout.position.x + layout.size.x - padding - prompt_width) :
                             footer_min.x + padding;
  footer_draw->PushClipRect(footer_min, footer_max, true);
  footer_draw->AddText(
      ImGui::GetFont(), ImGui::GetFontSize(),
      ImVec2(prompt_x, footer_min.y + (footer_height - ImGui::GetTextLineHeight()) * 0.5f),
      Color(palette.header_text), prompts.c_str(), nullptr,
      std::max(scale, layout.size.x - padding * 2));
  footer_draw->PopClipRect();
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, 0));
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::End();
  ImGui::PopFont();
  ImGui::PopStyleVar(10);
  ImGui::PopStyleColor(16);
  return result;
}
}  // namespace VideoCommon::BigPicture
