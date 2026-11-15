// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/BigPicture/Contracts.h"

namespace BigPicture
{
// All methods, including host callbacks, run on the frontend's owning thread. Drawing only
// consumes immutable snapshots. Execute must recheck the emulator's policy at execution time.
class Host
{
public:
  virtual ~Host() = default;
  virtual HostState ReadState() const = 0;
  virtual Result Execute(const Command& command) = 0;
};

UnavailableReason GetAvailability(Action action, const HostState& state);

class Controller
{
public:
  explicit Controller(Host& host);

  void Submit(const Command& command);
  // Completes a pending command that the host dispatched and finishes asynchronously. Results for
  // a command that is no longer pending (stale, superseded or invalidated) are ignored.
  void Complete(std::uint64_t command_id, Result result);
  void Refresh();
  // Called before every external boot, even if state notifications coalesce. Stop completion
  // is observed by Refresh. This invalidates commands from a previous emulation/graphics session.
  void InvalidateSession();
  void InvalidatePresentation();
  std::shared_ptr<const Snapshot> GetSnapshot() const;

private:
  void Publish();
  void SetRoot();
  void Finish(Result result);
  void Dispatch(const Command& command);
  void Reject(const Command& command, UnavailableReason reason);
  bool IsValidGameAction(const Command& command) const;

  Host& m_host;
  Snapshot m_state;
  std::shared_ptr<const Snapshot> m_snapshot;
  std::uint64_t m_last_id = 0;
  bool m_dispatching = false;
};
}  // namespace BigPicture
