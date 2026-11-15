// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "VideoCommon/BigPicture/Navigation.h"

namespace VideoCommon::BigPicture
{
// A frame-local view of immutable host data. Actions are values returned to the host queue.
struct InGameMenu
{
  MenuPage page;
  std::map<std::string, ::BigPicture::NavigationRequest> requests;
  std::optional<::BigPicture::NavigationRequest> Activate(const MenuResult& result) const;
};

// A command publishes a pending snapshot that disables every item until the host finishes it,
// usually a frame or two later. Drawing that page would cancel a click in progress (ImGui drops
// the press of an item that becomes disabled) and flicker the menu, so the last idle menu stays
// on screen for a short grace period, and a press made meanwhile is held for the idle page that
// follows.
class PendingMenuHold
{
public:
  using Clock = std::chrono::steady_clock;
  static constexpr Clock::duration GRACE = std::chrono::milliseconds(250);

  // Call before replacing `menu`, built from `previous`, with a menu built from `next`.
  void Replace(const ::BigPicture::Snapshot* previous, InGameMenu& menu,
               const ::BigPicture::Snapshot& next, Clock::time_point now);
  // The menu to draw for `snapshot`: the held idle menu, or `menu` itself.
  const InGameMenu& Visible(const ::BigPicture::Snapshot& snapshot, const InGameMenu& menu,
                            Clock::time_point now) const;
  // The request for a result drawn from `visible`. Nothing runs while a command is pending.
  std::optional<::BigPicture::NavigationRequest> Activate(const ::BigPicture::Snapshot& snapshot,
                                                          const InGameMenu& visible,
                                                          const MenuResult& result);
  // Input was reset (focus loss, remapping): a held press belongs to the old input.
  void DiscardHeld() { m_held.reset(); }
  void Reset();

private:
  struct Held
  {
    std::uint64_t generation = 0;
    std::string page;
    MenuResult result;
  };
  std::optional<InGameMenu> m_idle;
  std::uint64_t m_generation = 0;
  Clock::time_point m_since;
  std::optional<Held> m_held;
};

InGameMenu BuildInGameMenu(const ::BigPicture::Snapshot& snapshot,
                           const ::BigPicture::NavigationText& text,
                           ::BigPicture::NavigationOptions options);
}  // namespace VideoCommon::BigPicture
