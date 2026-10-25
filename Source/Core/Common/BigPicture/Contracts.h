// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
  Error
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
  Count
};
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
  bool can_change_disc = false;
  bool stop_requires_desktop = false;
  bool state_slots_ready = false;
  std::array<StateSlot, STATE_SLOT_COUNT> state_slots;
  std::string launch_path;
  std::string launch_title;
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
