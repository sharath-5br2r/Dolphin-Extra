// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Navigation.h"

#include <algorithm>
#include <charconv>

namespace VideoCommon::BigPicture
{
namespace
{
class ControllerHints
{
public:
  explicit ControllerHints(const ::BigPicture::NavigationStatus& status) : m_status(status) {}

  std::string Glyph(::BigPicture::NavButton button) const
  {
    using ::BigPicture::NavButton;
    if (m_status.options.swap_confirm_back)
    {
      if (button == NavButton::Confirm)
        button = NavButton::Back;
      else if (button == NavButton::Back)
        button = NavButton::Confirm;
    }
    return GetBindingGlyph(m_status.bindings[static_cast<std::size_t>(button)], m_status.device,
                           m_status.device_source);
  }

  std::string Button(::BigPicture::NavButton button, const std::string& label) const
  {
    return Glyph(button) + " " + label;
  }

private:
  const ::BigPicture::NavigationStatus& m_status;
};
}  // namespace

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
  case NavButton::Options:
    // ImGui's own navigation ignores Start, unlike the west/north face buttons, which switch
    // windows or activate the focused item. The physical button is chosen by the binding.
    return ImGuiKey_GamepadStart;
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
                                 const ::BigPicture::NavigationText& text, bool home, bool pause)
{
  using ::BigPicture::NavButton;
  const ControllerHints hints(status);
  std::string prompts;
  if (home)
    prompts =
        hints.Button(NavButton::Previous, text.home_labels[0]) + "   " +
        hints.Button(NavButton::Next,
                     text.actions[static_cast<std::size_t>(::BigPicture::Action::Fullscreen)]) +
        "   ";
  else if (!pause)
    prompts = hints.Button(NavButton::Previous, text.previous) + "   " +
              hints.Button(NavButton::Next, text.next) + "   ";
  return prompts + "\xe2\x87\x8e " + text.navigate + "   " +
         hints.Button(NavButton::Confirm, text.select) + "   " +
         hints.Button(NavButton::Back, home  ? text.home_labels[3] :
                                       pause ? text.return_to_game :
                                               text.back);
}
std::string GetStateControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                      const ::BigPicture::NavigationText& text)
{
  using ::BigPicture::NavButton;
  const ControllerHints hints(status);
  return "\xe2\x87\x8e " + text.navigate + "   " +
         hints.Button(NavButton::Previous,
                      text.actions[static_cast<std::size_t>(::BigPicture::Action::RefreshStates)]) +
         "   " + hints.Button(NavButton::Confirm, text.select) + "   " +
         hints.Button(NavButton::Back, text.cancel);
}

std::string GetAchievementsControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                             const ::BigPicture::NavigationText& text)
{
  const ControllerHints hints(status);
  return hints.Glyph(::BigPicture::NavButton::Previous) +
         hints.Glyph(::BigPicture::NavButton::Next) + " " + text.achievements_subsets + "   " +
         GetLaunchControllerPrompts(status, text, false);
}

std::string GetLibraryControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                        const ::BigPicture::NavigationText& text)
{
  using ::BigPicture::NavButton;
  const ControllerHints hints(status);
  // Configurations from before the Game Options binding existed leave it unbound.
  const bool options = !status.bindings[static_cast<std::size_t>(NavButton::Options)].empty();
  return "\xe2\x87\x8e " + text.library_select_game + "   " +
         hints.Button(NavButton::Previous, text.library_change_view) + "   " +
         (options ? hints.Button(NavButton::Options, text.game_options) + "   " : std::string{}) +
         hints.Button(NavButton::Next, text.library_actions[static_cast<std::size_t>(
                                           ::BigPicture::LibraryOperation::Options)]) +
         "   " + hints.Button(NavButton::Confirm, text.home_labels[1]) + "   " +
         hints.Button(NavButton::Back, text.back);
}
std::string GetLaunchControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                       const ::BigPicture::NavigationText& text, bool picker)
{
  using ::BigPicture::NavButton;
  const ControllerHints hints(status);
  return "\xe2\x87\x8e " + text.navigate + "   " +
         (picker ? hints.Button(NavButton::Previous, text.library_parent) + "   " : "") +
         hints.Button(NavButton::Confirm, text.select) + "   " +
         hints.Button(NavButton::Back, picker ? text.library_cancel : text.back);
}
std::string GetSettingsControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                         const ::BigPicture::NavigationText& text, bool row_options,
                                         bool adjust)
{
  using ::BigPicture::NavButton;
  const ControllerHints hints(status);
  return hints.Glyph(NavButton::Previous) + hints.Glyph(NavButton::Next) + " " +
         text.settings_change_page +
         (adjust ? "   \xe2\x86\xa2 " + text.settings_adjust : std::string{}) + "   \xe2\x86\xa3 " +
         text.navigate + (row_options ? "   \xe2\x86\xa0 " + text.settings_options : "") + "   " +
         hints.Glyph(NavButton::Confirm) + " " + text.select + "   " +
         hints.Glyph(NavButton::Back) + " " + text.back;
}

}  // namespace VideoCommon::BigPicture
