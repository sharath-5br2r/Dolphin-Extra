// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"
#include "VideoCommon/BigPicture/Style.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <ctime>

#include <imgui_internal.h>

namespace VideoCommon::BigPicture
{
namespace
{
ImU32 Color(const ImVec4& color)
{
  return ImGui::GetColorU32(color);
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

void NavigationInput::SetSource(bool keyboard)
{
  if (m_keyboard == keyboard)
    return;
  for (const auto key : m_held)
    ImGui::GetIO().AddKeyEvent(key, false);
  m_held.clear();
  m_keyboard = keyboard;
}

void NavigationInput::KeyEvent(ImGuiKey key, bool down, bool keyboard)
{
  if (down)
    SetSource(keyboard);
  if (down && std::ranges::find(m_held, key) == m_held.end())
    m_held.push_back(key);
  else if (!down)
    std::erase(m_held, key);
  ImGui::GetIO().AddKeyEvent(key, down);
}

std::string GetBindingGlyph(std::string_view binding, std::string_view device,
                            std::string_view source)
{
  const auto original = binding;
  if (binding.size() >= 2 && binding.front() == '`' && binding.back() == '`')
    binding = binding.substr(1, binding.size() - 2);
  // A compound expression is not a single physical button. Preserve its text rather
  // than interpreting one of its operands as the complete binding.
  if (binding.find_first_of("`|&()!@") != std::string_view::npos)
    return "\xe2\x90\xbc " + std::string(original);
  if (const auto colon = binding.rfind(':'); colon != std::string_view::npos)
  {
    const auto qualifier = binding.substr(0, colon);
    const auto first = qualifier.find('/');
    const auto second =
        first == std::string_view::npos ? std::string_view::npos : qualifier.find('/', first + 1);
    if (second == std::string_view::npos)
      return "\xe2\x90\xbc " + std::string(original);
    source = qualifier.substr(0, first);
    device = qualifier.substr(second + 1);
    binding.remove_prefix(colon + 1);
  }
  const bool sony = device.find("DualShock") != std::string_view::npos ||
                    device.find("DualSense") != std::string_view::npos ||
                    device.find("PS4") != std::string_view::npos ||
                    device.find("PS5") != std::string_view::npos ||
                    device.find("PlayStation") != std::string_view::npos;
  const bool nintendo = device.find("Nintendo") != std::string_view::npos ||
                        device.find("Switch") != std::string_view::npos;
  // Sony's DS4/DualSense DirectInput HID layout is not SDL's numbered-button layout.
  // SDL's Windows PS4/PS5 mappings independently document x:b0,a:b1,b:b2,y:b3,
  // leftshoulder:b4,rightshoulder:b5 (SDL_gamepad_db.h). Do not apply it to
  // arbitrary DirectInput devices or to SDL's legacy numbered aliases.
  const bool sony_hid = device.find("DualSense") != std::string_view::npos ||
                        device.find("DualShock 4") != std::string_view::npos ||
                        device.find("DUALSHOCK 4") != std::string_view::npos ||
                        device.find("PS4") != std::string_view::npos ||
                        device.find("PS5") != std::string_view::npos;
  if (source == "DInput" && sony_hid && binding.starts_with("Button "))
  {
    const auto number = binding.substr(7);
    unsigned index = 0;
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), index);
    static constexpr std::string_view glyphs[] = {
        "\xe2\x87\xa0", "\xe2\x87\xa3",
        "\xe2\x87\xa2", "\xe2\x87\xa1",  // square/cross/circle/triangle
        "\xe2\x86\xb0", "\xe2\x86\xb1",
        "\xe2\x86\xb2", "\xe2\x86\xb3",  // L1/R1/L2/R2
        "\xe2\x87\xa6", "\xe2\x87\xa8",
        "\xe2\x86\xba", "\xe2\x86\xbb",  // share/options/L3/R3
    };
    if (error == std::errc{} && end == number.data() + number.size() && index < std::size(glyphs))
      return std::string(glyphs[index]);
  }
  // SDL uses compass positions; its legacy A/B/X/Y names use Xbox positions too.
  if (binding == "Button A" || binding == "Button S")
    return sony ? "\xe2\x87\xa3" : nintendo ? "\xe2\x87\x92" : "\xe2\x87\x93";
  if (binding == "Button B" || binding == "Button E")
    return sony ? "\xe2\x87\xa2" : nintendo ? "\xe2\x87\x93" : "\xe2\x87\x92";
  if (binding == "Button X" || binding == "Button W")
    return sony ? "\xe2\x87\xa0" : nintendo ? "\xe2\x87\x91" : "\xe2\x87\x90";
  if (binding == "Button Y" || binding == "Button N")
    return sony ? "\xe2\x87\xa1" : nintendo ? "\xe2\x87\x90" : "\xe2\x87\x91";
  if (binding == "Shoulder L")
    return sony ? "\xe2\x86\xb0" : nintendo ? "\xe2\x86\x9c" : "\xe2\x86\x98";
  if (binding == "Shoulder R")
    return sony ? "\xe2\x86\xb1" : nintendo ? "\xe2\x86\x9d" : "\xe2\x86\x99";
  // Keep a readable binding alongside a neutral controller glyph for unrecognized
  // expressions rather than guessing a button or silently dropping the hint.
  return binding.empty() ? "\xe2\x90\xbc" : "\xe2\x90\xbc " + std::string(binding);
}

std::string GetControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                 const ::BigPicture::NavigationText& text, bool home)
{
  using ::BigPicture::NavButton;
  const auto hint = [&](NavButton button, const std::string& label) {
    return GetBindingGlyph(status.bindings[static_cast<std::size_t>(button)], status.device,
                           status.device_source) +
           " " + label;
  };
  const auto confirm = status.options.swap_confirm_back ? NavButton::Back : NavButton::Confirm;
  const auto back = status.options.swap_confirm_back ? NavButton::Confirm : NavButton::Back;
  std::string prompts;
  if (home)
    prompts = hint(NavButton::Previous, text.home_labels[0]) + "   " +
              hint(NavButton::Next,
                   text.actions[static_cast<std::size_t>(::BigPicture::Action::Fullscreen)]) +
              "   ";
  else
    prompts =
        hint(NavButton::Previous, text.previous) + "   " + hint(NavButton::Next, text.next) + "   ";
  return prompts + "\xe2\x87\x8e " + text.navigate + "   " + hint(confirm, text.select) + "   " +
         hint(back, home ? text.home_labels[3] : text.back);
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

MenuResult
NavigationToolkit::DrawLanding(const MenuPage& page, ::BigPicture::NavigationOptions options,
                               const std::string& prompts, ImFont* font,
                               const std::function<ImTextureID(const std::string&)>& image)
{
  MenuResult result;
  const auto display = ImGui::GetIO().DisplaySize;
  options = ::BigPicture::SanitizeNavigationOptions(options);
  // The bars reach the screen edges; overscan applies to the usable menu area.
  const float scale =
      std::max(0.01f, std::min({display.x / 1280 * options.scale, display.y / 720 * options.scale,
                                display.x * (1 - options.safe_area * 2) / 1090,
                                display.y * (1 - options.safe_area * 2) / 402}));
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  const auto& palette = Style::GetPalette(page.theme);
  auto& memory = m_memory[page.id];
  const bool entering = m_current_page != page.id || GImGui->NavWindow == nullptr;
  m_current_page = page.id;
  if (memory.item.empty())
    memory.item = page.default_focus;
  const float header = 48 * scale;
  const float footer = 34 * scale;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, palette.background);
  ImGui::PushStyleColor(ImGuiCol_Text, palette.text);
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_NavCursor, palette.focus);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(30 * scale, 0));
  ImGui::PushFont(font, 22 * scale / font_scale);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  if (entering || GImGui->NavWindow == nullptr)
    ImGui::SetNextWindowFocus();
  ImGui::Begin("###BigPictureHome", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, header), Color(palette.header));
  draw->AddRectFilled(ImVec2(0, display.y - footer), display, Color(palette.header));
  const auto texture = [&](const std::string& key) { return image ? image(key) : ImTextureID{}; };
  if (const auto logo = texture("home:logo"))
    draw->AddImage(logo, ImVec2(15 * scale, 8 * scale), ImVec2(47 * scale, 40 * scale));
  draw->AddText(ImVec2(60 * scale, 8 * scale), Color(palette.header_text), page.title.c_str());
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
  const float menu_width = page.items.size() * 280 * scale - 30 * scale;
  const float top = header + (display.y - header - footer - 320 * scale) * 0.5f;
  ImGui::SetCursorPos(ImVec2((display.x - menu_width) * 0.5f, top));
  for (const auto& item : page.items)
  {
    if (&item != &page.items.front())
      ImGui::SameLine();
    ImGui::PushID(item.id.c_str());
    if (entering && memory.item == item.id)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    const bool clicked =
        ImGui::Selectable("##home-action", false, ImGuiSelectableFlags_NoPadWithHalfSpacing,
                          ImVec2(250 * scale, 320 * scale));
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const bool highlight =
        ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible);
    if (ImGui::IsItemFocused() || clicked)
      memory.item = item.id;
    if (clicked)
      result.activated = item.id;
    if (highlight)
    {
      draw->AddRectFilled(min, max, Color(palette.hover), 6 * scale);
      draw->AddRect(min, max, Color(palette.focus), 6 * scale, 0, scale);
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
    ImGui::PushFont(font, 14 * scale / font_scale);
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
  ImGui::PushFont(font, 14 * scale / font_scale);
  const auto prompt_size = ImGui::CalcTextSize(prompts.c_str());
  draw->PushClipRect(ImVec2(15 * scale, display.y - footer), display, true);
  draw->AddText(ImVec2(std::max(15 * scale, display.x - prompt_size.x - 15 * scale),
                       display.y - footer + (footer - prompt_size.y) * 0.5f),
                Color(palette.header_text), prompts.c_str());
  draw->PopClipRect();
  ImGui::PopFont();
  result.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  result.focused = memory.item;
  ImGui::End();
  ImGui::PopFont();
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(6);
  return result;
}
MenuResult NavigationToolkit::Draw(const MenuPage& page, ::BigPicture::NavigationOptions options,
                                   const std::string& prompts, ImFont* font,
                                   const std::function<ImTextureID(const std::string&)>& image)
{
  if (page.presentation == MenuPresentation::Landing && page.id == "home")
    return DrawLanding(page, options, prompts, font, image);
  MenuResult result;
  const auto layout = ComputeMenuLayout(ImGui::GetIO().DisplaySize, options, page.dialog);
  const float scale = layout.scale;
  // DisplaySize is already in native pixels. Diagnostic OSD/DPI font scaling must not
  // multiply the menu's separately scaled geometry a second time.
  const float font_scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;
  auto& memory = m_memory[page.id];
  const auto previous_focus = memory.item;
  const bool revision_changed = m_focus_revision != page.focus_revision;
  const bool entering = m_current_page != page.id || m_focus_revision != page.focus_revision ||
                        m_layout_width != layout.size.x || m_layout_height != layout.size.y ||
                        m_layout_scale != scale;
  m_current_page = page.id;
  m_focus_revision = page.focus_revision;
  // Destructive dialogs always start at their safe default, including another request using
  // the same page. Reuse one memory entry rather than accumulating entries per command ID.
  if (entering && (page.dialog || (page.reset_on_revision && revision_changed)))
    memory = {};
  m_layout_width = layout.size.x;
  m_layout_height = layout.size.y;
  m_layout_scale = scale;
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
  const float gap = Style::GAP * scale;
  const float title_size = Style::TITLE_SIZE * scale;
  const float summary_size = Style::SUMMARY_SIZE * scale;
  const bool pause = page.presentation == MenuPresentation::Pause;
  const bool landing = page.presentation == MenuPresentation::Landing;
  const bool library_list = page.presentation == MenuPresentation::Library && page.columns == 1 &&
                            !page.items.empty() && layout.size.x >= 850 * scale;
  auto* background = ImGui::GetBackgroundDrawList();
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  auto backdrop = palette.background;
  if (pause || page.overlay || page.dialog)
    backdrop.w = 0.80f;
  background->AddRectFilled(ImVec2(0, 0), display, Color(backdrop));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, palette.background);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, palette.text);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, palette.secondary);
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_NavCursor, palette.focus);
  ImGui::PushStyleColor(ImGuiCol_Button, palette.header);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.selected);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.selected);
  ImGui::PushStyleColor(ImGuiCol_Border, palette.hover);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, palette.panel);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, palette.hover);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, palette.secondary);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, palette.focus);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, pause ? 0 : gap));
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
  const float header_height = (pause ? 110 : Style::HEADER_HEIGHT) * scale;
  const ImVec2 header_max(layout.position.x + layout.size.x, layout.position.y + header_height);
  if (!pause)
    window_draw->AddRectFilled(layout.position, header_max, Color(palette.header),
                               page.dialog ? 8 * scale : 0);
  ImGui::SetCursorPos(ImVec2(padding, gap));
  if (pause && !page.context.label.empty())
  {
    DrawCover(window_draw, ImVec2(layout.position.x + padding, layout.position.y + gap),
              ImVec2(60 * scale, 90 * scale),
              image && !page.context.image_key.empty() ? image(page.context.image_key) :
                                                         ImTextureID{},
              scale, palette);
    const ImVec2 text_min(layout.position.x + padding + 80 * scale, layout.position.y + gap);
    window_draw->PushClipRect(text_min, header_max, true);
    window_draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_min, Color(palette.text),
                         page.context.label.c_str(), nullptr,
                         std::max(scale, layout.size.x - padding * 2 - 80 * scale));
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
        ImGui::PushStyleColor(ImGuiCol_Button, palette.selected);
      const bool clicked = ImGui::Button("##toolbar", ImVec2(toolbar_width, toolbar_height));
      if (item.active)
        ImGui::PopStyleColor();
      const auto min = ImGui::GetItemRectMin();
      const auto max = ImGui::GetItemRectMax();
      if (item.active)
        window_draw->AddLine(ImVec2(min.x + gap, max.y - 4 * scale),
                             ImVec2(max.x - gap, max.y - 4 * scale), Color(palette.selected_text),
                             2 * scale);
      const auto text_color = Color(item.active || ImGui::IsItemHovered() || ImGui::IsItemActive() ?
                                        palette.selected_text :
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
      std::max(Style::FOOTER_HEIGHT * scale,
               ImGui::CalcTextSize(prompts.c_str(), nullptr, false,
                                   std::max(scale, layout.size.x - padding * 2))
                       .y +
                   gap * 2);
  ImGui::PopFont();
  const float content_top = ImGui::GetCursorPosY();
  const float content_height = std::max(scale, layout.size.y - content_top - footer_height - gap);
  const float content_width =
      std::max(scale, ImGui::GetContentRegionAvail().x - (library_list ? 440 * scale : 0));
  if (entering || m_restore_frames != 0)
    ImGui::SetNextWindowScroll(
        ImVec2(0, (m_restore_frames ? m_restore_scroll : memory.scroll) * scale));
  ImGui::BeginChild(
      page.id.c_str(),
      ImVec2(pause ? std::min(content_width, 500 * scale) : content_width, content_height),
      page.toolbar.empty() ? ImGuiChildFlags_None : ImGuiChildFlags_NavFlattened,
      ImGuiWindowFlags_AlwaysVerticalScrollbar);
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
    const float icon_width =
        !page.artwork && !large_icon && item.icon != MenuIcon::None ? title_size + gap : 0;
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
                                item.detail.empty() ? Style::SHORT_ROW_HEIGHT :
                                                      Style::ROW_HEIGHT) *
                                   scale,
                               label_height + detail_height + gap * 2);
    ImGui::PushID(item.id.c_str());
    const bool request_focus = (restore_focus && item.id == focus_id) || item.id == requested_focus;
    if (request_focus && item.enabled)
      ImGui::SetKeyboardFocusHere();
    ImGui::BeginDisabled(!item.enabled);
    const bool clicked =
        ImGui::Selectable("##item", item.enabled && item.id == memory.item,
                          ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(width, height));
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
    const bool selected = item.enabled && item.id == memory.item;
    const bool hovered = ImGui::IsItemHovered();
    if (selected || hovered || columns > 1)
      draw->AddRectFilled(min, max,
                          Color(selected ? palette.selected :
                                hovered  ? palette.hover :
                                           palette.header),
                          Style::ROUNDING * scale);
    if (selected)
      draw->AddRect(min, max, Color(palette.focus), Style::ROUNDING * scale, 0, 2 * scale);
    draw->PushClipRect(min, max, true);
    const auto label_color = Color(selected    ? palette.selected_text :
                                   hovered     ? palette.text :
                                   columns > 1 ? palette.header_text :
                                                 palette.text);
    if (item.active)
      DrawMenuIcon(draw, MenuIcon::Check, ImVec2(max.x - padding - title_size, min.y + gap),
                   title_size, Color(selected ? palette.selected_text : palette.focus));
    if (!page.artwork)
    {
      const float icon_size =
          large_icon ? std::min(64 * scale, std::max(0.0f, text_top - 32 * scale)) : title_size;
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
                    Color(selected    ? palette.selected_text :
                          hovered     ? palette.text :
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
  if (library_list)
  {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.panel);
    ImGui::BeginChild("##game-details", ImVec2(0, content_height), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    const auto selected = std::ranges::find(page.items, memory.item, &MenuItem::id);
    if (selected != page.items.end())
    {
      const float available = ImGui::GetContentRegionAvail().x;
      const float cover_height = std::min(240 * scale, content_height * 0.5f);
      const ImVec2 cover_size(cover_height * 2 / 3, cover_height);
      const auto pos = ImGui::GetCursorScreenPos();
      DrawCover(ImGui::GetWindowDrawList(),
                ImVec2(pos.x + (available - cover_size.x) / 2, pos.y + gap), cover_size,
                image && !selected->image_key.empty() ? image(selected->image_key) : ImTextureID{},
                scale, palette);
      ImGui::Dummy(ImVec2(0, cover_height + gap * 2));
      ImGui::TextWrapped("%s", selected->label.c_str());
      ImGui::PushFont(font, summary_size / font_scale);
      ImGui::TextWrapped(
          "%s",
          (selected->preview_detail.empty() ? selected->detail : selected->preview_detail).c_str());
      ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
  }
  const ImVec2 footer_min(layout.position.x, layout.position.y + layout.size.y - footer_height);
  window_draw->AddRectFilled(footer_min,
                             ImVec2(footer_min.x + layout.size.x, footer_min.y + footer_height),
                             Color(palette.panel));
  ImGui::SetCursorScreenPos(ImVec2(footer_min.x + padding, footer_min.y + gap));
  ImGui::PushFont(font, summary_size / font_scale);
  window_draw->PushClipRect(
      footer_min, ImVec2(footer_min.x + layout.size.x, footer_min.y + footer_height), true);
  window_draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImGui::GetCursorScreenPos(),
                       Color(palette.text), prompts.c_str(), nullptr,
                       std::max(scale, layout.size.x - padding * 2));
  window_draw->PopClipRect();
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
