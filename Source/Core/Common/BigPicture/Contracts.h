// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/BigPicture/Library.h"
#include "Common/BigPicture/Settings.h"
#include "Common/Settings/Descriptor.h"

namespace BigPicture
{
enum class Mode
{
  Desktop,
  Library,
  InGame
};
enum class Screen
{
  Library,
  Pause,
  Settings,
  SaveStates,
  LoadStates,
  Confirmation,
  Error,
  LibraryOptions,
  FilePicker,
  TextInput,
  Home,
  StartGame,
  ExitMenu,
  About
};
enum class Lifecycle
{
  Idle,
  Booting,
  Running,
  Paused,
  Stopping
};
enum class Action
{
  OpenMenu,
  Back,
  Settings,
  SaveStates,
  LoadStates,
  ReturnToDesktop,
  Boot,
  Resume,
  Stop,
  Reset,
  SaveState,
  LoadState,
  Confirm,
  Cancel,
  Screenshot,
  ChangeDisc,
  DesktopSettings,
  RefreshStates,
  Exit,
  OpenGame,
  Fullscreen,
  Library,
  BootIPL,
  BootSystemMenu,
  EditSetting,
  SettingsMenu,
  ShowLibrary,
  StartGame,
  ExitMenu,
  About,
  Count
};

constexpr bool IsBootAction(Action action)
{
  return action == Action::Boot || action == Action::BootIPL || action == Action::BootSystemMenu;
}
enum class UnavailableReason
{
  None,
  Busy,
  NoGame,
  GameActive,
  NetPlay,
  Hardcore,
  InvalidArgument,
  DiscUnavailable,
  DesktopRequired,
  EmptySlot,
  Count
};
enum class ResultStatus
{
  Pending,
  Dispatched,
  Succeeded,
  Cancelled,
  Failed,
  Stale,
  Unavailable
};

struct Command
{
  // IDs are nonzero and monotonically increasing for the lifetime of the host. A command
  // carries the generation of the snapshot it was created from, including confirmations.
  std::uint64_t generation = 0;
  std::uint64_t id = 0;
  Action action = Action::OpenMenu;
  std::string path;
  int slot = 0;
  // Confirm must name the destructive request displayed by its dialog. A delayed activation
  // from an earlier dialog must never confirm a different action in the same session.
  std::uint64_t confirmation_id = 0;
  LibraryOperation library_operation = LibraryOperation::None;
  std::uint64_t library_revision = 0;
  std::optional<Common::Settings::Request> setting;
  std::optional<SettingsRequest> settings_menu;
};

struct Result
{
  Command command;
  ResultStatus status = ResultStatus::Succeeded;
  UnavailableReason reason = UnavailableReason::None;
  std::string message;
};

constexpr std::size_t STATE_SLOT_COUNT = 10;

struct StateSlot
{
  bool occupied = false;
  bool readable = false;
  std::string description;
};

struct HostState
{
  Lifecycle lifecycle = Lifecycle::Idle;
  bool netplay = false;
  bool hardcore = false;
  std::uint64_t pause_revision = 0;
  std::string game_title;
  std::string game_path;
  bool can_change_disc = false;
  bool stop_requires_desktop = false;
  bool state_slots_ready = false;
  std::array<StateSlot, STATE_SLOT_COUNT> state_slots;
  std::string launch_path;
  std::string launch_title;
  std::shared_ptr<const LibrarySnapshot> library;
  std::shared_ptr<const Common::Settings::Snapshot> settings;
  std::shared_ptr<const SettingsMenuSnapshot> settings_menu;
  std::array<bool, 6> boot_ipl_available{};
  bool boot_system_menu_available = false;
  std::string menu_theme = "Dark";
  std::string achievements_username;
  std::shared_ptr<const LibraryImage> achievements_avatar;
};

// Value-only contracts: no Qt, emulator objects, config-layer pointers or GPU resources.
// Publish as shared_ptr<const Snapshot>; a reader may retain it across subsequent host updates.
struct Snapshot
{
  std::uint64_t generation = 1;
  Mode mode = Mode::Desktop;
  HostState host;
  std::vector<Screen> screens;
  std::array<UnavailableReason, static_cast<std::size_t>(Action::Count)> availability{};
  std::optional<Command> confirmation;
  // Confirmations always initially focus Cancel; the navigation toolkit consumes this value.
  Action confirmation_default = Action::Cancel;
  std::optional<Command> pending;
  std::optional<Result> result;
  // Error presentation survives later rejected commands/acknowledgments until dismissed.
  std::optional<Result> error;
};
}  // namespace BigPicture
