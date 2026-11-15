// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <imgui.h>

#include "Common/BigPicture/Navigation.h"
#include "VideoCommon/BigPicture/Icons.h"

namespace VideoCommon::BigPicture
{
enum class MenuPresentation
{
  List,
  Landing,
  Pause,
  Library,
  Launch,
  FilePicker,
  Settings,
  States,
  Achievements,
};

struct MenuSlider
{
  double minimum = 0;
  double maximum = 0;
  double step = 1;
  double value = 0;
  std::string minimum_label;
  std::string maximum_label;
};

struct MenuItem
{
  std::string id;
  std::string label;
  std::string detail;
  bool enabled = true;
  std::string image_key;
  bool active = false;
  MenuIcon icon = MenuIcon::None;
  std::string preview_detail;
  float image_aspect = 2.0f / 3.0f;
  std::string value;
  std::string section;
  std::optional<bool> toggle;
  bool value_enabled = true;
  bool has_options = false;
  // Drawn as a horizontal slider; "<id>:set" sets an arbitrary value (MenuResult::value).
  std::optional<MenuSlider> slider;
};

struct MenuPage
{
  std::string id;
  std::string title;
  std::string description;
  std::vector<MenuItem> items;
  std::string default_focus;
  int columns = 1;
  bool dialog = false;
  std::uint64_t focus_revision = 0;
  std::vector<MenuItem> toolbar;
  bool artwork = false;
  bool compact = false;
  bool reset_on_revision = false;
  MenuPresentation presentation = MenuPresentation::List;
  // Header metadata is separate from the scrollable explanation/status text.
  MenuItem context;
  bool overlay = false;
  std::string theme = "Dark";
  std::string account_name;
  std::string play_time;
  std::string rich_presence;
  std::string achievement_progress;
  std::shared_ptr<const MenuPage> background;
  bool category_page = false;
  bool busy = false;
  bool settings_text = false;
  std::shared_ptr<const ::BigPicture::AchievementsSnapshot> achievements;
  std::string empty_text;
  // Items that Left and Right activate instead of changing the settings page, and the larger
  // steps for Page Up/Down or a held key.
  std::string decrease;
  std::string increase;
  std::string decrease_large;
  std::string increase_large;
};

struct MenuResult
{
  std::optional<std::string> activated;
  bool back = false;
  std::string focused;
  bool fullscreen = false;
  // The value chosen for an activated "<slider>:set" request.
  std::optional<double> value;
};

struct FocusMemory
{
  std::string item;
  // Logical reference pixels, so a resize/DPI change does not change the retained position.
  float scroll = 0;
  std::uint64_t revision = 0;
  std::string selected;
};

struct MenuLayout
{
  ImVec2 position;
  ImVec2 size;
  float scale;
};
MenuLayout ComputeMenuLayout(ImVec2 viewport, ::BigPicture::NavigationOptions options,
                             bool dialog = false);

// Whether a focusable entry with this ID is currently enabled.
bool HasEnabledItem(const std::vector<MenuItem>& items, std::string_view id);

// Graphics-owner only. Stable page/item IDs retain focus and scroll across nested screens.
// This layer returns item IDs; it never calls a frontend or emulator operation.
// Lines of a setting value at most `width` wide in the current font: broken after path and word
// separators, at most two, with the start of a value too long for them replaced by "...".
std::vector<std::string> WrapSettingValue(std::string_view value, float width);

class NavigationToolkit
{
public:
  MenuResult Draw(const MenuPage& page, ::BigPicture::NavigationOptions options,
                  const std::string& prompts, ImFont* font = nullptr,
                  const std::function<ImTextureID(const std::string&)>& image = {});
  void InvalidateFocus() { m_current_page.clear(); }
  void Reset();
  FocusMemory GetMemory(const std::string& page) const;
  void RestoreMemory(const std::string& page, FocusMemory memory);

private:
  struct LayoutChange
  {
    bool page = false;
    bool shape = false;
    bool Any() const { return page || shape; }
  };
  // Records the drawn page and its size and scale, reporting what changed since the previous
  // frame. Views decide what else (focus revisions, a lost ImGui nav window) re-applies focus.
  LayoutChange TrackLayout(const std::string& page, ImVec2 size, float scale);
  MenuResult DrawAchievements(const MenuPage& page, ::BigPicture::NavigationOptions options,
                              const std::string& prompts, ImFont* font,
                              const std::function<ImTextureID(const std::string&)>& image);
  std::set<std::string> m_collapsed_achievement_groups;
  MenuResult DrawList(const MenuPage& page, ::BigPicture::NavigationOptions options,
                      const std::string& prompts, ImFont* font,
                      const std::function<ImTextureID(const std::string&)>& image);
  MenuResult DrawDialog(const MenuPage& page, ::BigPicture::NavigationOptions options,
                        const std::string& prompts, ImFont* font,
                        const std::function<ImTextureID(const std::string&)>& image);
  MenuResult DrawLibrary(const MenuPage& page, ::BigPicture::NavigationOptions options,
                         const std::string& prompts, ImFont* font,
                         const std::function<ImTextureID(const std::string&)>& image);
  MenuResult DrawLanding(const MenuPage& page, ::BigPicture::NavigationOptions options,
                         const std::string& prompts, ImFont* font,
                         const std::function<ImTextureID(const std::string&)>& image,
                         bool passive = false);
  MenuResult DrawFilePicker(const MenuPage& page, ::BigPicture::NavigationOptions options,
                            const std::string& prompts, ImFont* font,
                            const std::function<ImTextureID(const std::string&)>& image);
  MenuResult DrawSettings(const MenuPage& page, ::BigPicture::NavigationOptions options,
                          const std::string& prompts, ImFont* font);
  MenuResult DrawStates(const MenuPage& page, ::BigPicture::NavigationOptions options,
                        const std::string& prompts, ImFont* font,
                        const std::function<ImTextureID(const std::string&)>& image);
  std::string m_current_page;
  std::map<std::string, FocusMemory> m_memory;
  unsigned m_restore_frames = 0;
  // The last input came from the mouse rather than a keyboard or gamepad.
  bool m_pointer = false;
  // The pointer position on the previous frame; unset before the first frame.
  std::optional<std::optional<ImVec2>> m_last_pointer;
  float m_restore_scroll = 0;
  float m_layout_width = 0;
  float m_layout_height = 0;
  float m_layout_scale = 0;
  float m_header_height = 0;
  std::uint64_t m_focus_revision = 0;
  int m_library_columns = 0;
  bool m_library_grid = false;
  bool m_settings_busy = false;
  // The last value a pointer drag requested and the slider value it was requested from, so a
  // held pointer does not resend it every frame but retries once the published value changes.
  std::optional<std::tuple<std::string, double, double>> m_slider_sent;
};

// Native bindings become named gamepad keys on the render owner. Confirm/back swapping is
// resolved here, so ImGui always receives logical Select/Back controls.
ImGuiKey GetNavigationKey(::BigPicture::NavButton button, bool swap_confirm_back);

// The most recently pressed input family owns navigation. Release the previous family's
// held keys when switching, so a missed release or held stick cannot mask the new input.
class NavigationInput
{
public:
  void SetSource(bool keyboard);
  void KeyEvent(ImGuiKey key, bool down, bool keyboard);
  void Reset() { m_held.clear(); }
  bool IsKeyboard() const { return m_keyboard; }

private:
  bool m_keyboard = true;
  std::vector<ImGuiKey> m_held;
};

std::string GetBindingGlyph(std::string_view binding, std::string_view device,
                            std::string_view source = {});
std::string GetControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                 const ::BigPicture::NavigationText& text, bool home,
                                 bool pause = false);
std::string GetStateControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                      const ::BigPicture::NavigationText& text);
std::string GetAchievementsControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                             const ::BigPicture::NavigationText& text);
std::string GetLibraryControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                        const ::BigPicture::NavigationText& text);
std::string GetLaunchControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                       const ::BigPicture::NavigationText& text, bool picker);
std::string GetSettingsControllerPrompts(const ::BigPicture::NavigationStatus& status,
                                         const ::BigPicture::NavigationText& text,
                                         bool row_options = true, bool adjust = false);
}  // namespace VideoCommon::BigPicture
