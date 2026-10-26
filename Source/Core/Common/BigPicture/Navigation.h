// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Common/BigPicture/Contracts.h"

namespace BigPicture
{
enum class NavButton
{
  Menu,
  Up,
  Down,
  Left,
  Right,
  Confirm,
  Back,
  Previous,
  Next,
  Count
};
constexpr std::size_t NAV_BUTTON_COUNT = static_cast<std::size_t>(NavButton::Count);
using NavValues = std::array<double, NAV_BUTTON_COUNT>;

struct NavTransition
{
  NavButton button;
  bool down;
};

// One filter per binding set. Hysteresis applies to both sticks and digital inputs. After a
// disconnect, focus change or menu transition, each control must be released before reuse.
class NavigationFilter
{
public:
  std::vector<NavTransition> Update(const NavValues& values, bool connected, bool enabled,
                                    std::uint64_t epoch);

private:
  std::array<bool, NAV_BUTTON_COUNT> m_down{};
  std::array<bool, NAV_BUTTON_COUNT> m_require_release{};
  std::optional<std::uint64_t> m_epoch;
  bool m_connected = false;
  bool m_enabled = false;
};

struct NavigationOptions
{
  bool swap_confirm_back = false;
  float scale = 1.0f;
  float safe_area = 0.05f;
  bool operator==(const NavigationOptions&) const = default;
};
NavigationOptions SanitizeNavigationOptions(NavigationOptions options);

struct NavigationStatus
{
  bool gamepad = false;
  std::string device;
  std::array<std::string, NAV_BUTTON_COUNT> bindings;
  NavigationOptions options;
  // Button numbering differs across backends, even for the same physical controller.
  std::string device_source;
};

// Translated on the Qt owner; the graphics owner only consumes these value strings.
struct NavigationText
{
  std::string title;
  std::string preferences;
  std::string close;
  std::string swap;
  std::string scale;
  std::string safe_area;
  std::string back;
  std::string select;
  std::string previous;
  std::string next;
  std::string on;
  std::string off;
  std::string keyboard_prompts;
  std::array<std::string, static_cast<std::size_t>(Action::Count)> actions;
  std::array<std::string, static_cast<std::size_t>(UnavailableReason::Count)> reasons;
  std::array<std::string, STATE_SLOT_COUNT> slot_labels;
  std::string confirm;
  std::string cancel;
  std::string confirm_stop;
  std::string confirm_reset;
  std::string confirm_exit;
  std::string confirm_overwrite;
  std::string error;
  std::string requested;
  std::string loading_slots;
  std::string empty_slot;
  std::string unknown_slot;
  std::string state_browser_help;
  std::string desktop_settings_help;
  std::string desktop_disc_help;
  std::string stop_help;
  std::string standalone_help;
  std::string confirm_exit_idle;
  std::array<std::string, static_cast<std::size_t>(LibraryOperation::Count)> library_actions;
  std::string library_empty;
  std::string library_no_matches;
  std::string library_scanning;
  std::string library_choose_folder;
  std::string library_parent;
  std::string library_roots;
  std::string library_space;
  std::string library_keyboard;
  std::string library_disc_games;
  std::string library_keyboard_prompts;
  std::string library_cancel;
  std::string library_grid;
  std::string library_list;
  std::string library_all;
  std::string disc_change_requested;
  std::array<std::string, 4> home_labels;
  std::array<std::string, 4> home_descriptions;
  std::string home_keyboard_prompts;
  std::string navigate;
  std::string about_title;
  std::string about_description;
};

struct NavigationRequest
{
  std::uint64_t generation = 0;
  Action action = Action::Back;
  std::optional<NavigationOptions> options;
  std::uint64_t input_epoch = 0;
  int slot = 0;
  std::uint64_t confirmation_id = 0;
  std::string path;
  LibraryOperation library_operation = LibraryOperation::None;
  std::uint64_t library_revision = 0;
  std::optional<Common::Settings::Request> setting;
  std::optional<SettingsRequest> settings_menu;
};
}  // namespace BigPicture
