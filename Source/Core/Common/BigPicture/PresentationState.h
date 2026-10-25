// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/BigPicture/Contracts.h"
#include "Common/BigPicture/Navigation.h"

namespace BigPicture
{
struct CaptureState
{
  bool keyboard = false;
  bool mouse = false;
  bool modal = false;
};

struct InputEvent
{
  enum class Type
  {
    Key,
    Text,
    Navigation,
    MousePosition,
    MouseButtons,
    Focus,
    Reset
  };
  Type type = Type::Reset;
  std::uint64_t generation = 0;
  std::uint32_t key = 0;
  bool down = false;
  std::string text;
  float x = 0;
  float y = 0;
  std::uint32_t buttons = 0;
};

struct FrameInput
{
  std::uint64_t epoch = 0;
  std::vector<int> key_map;
  bool reset = false;
  std::deque<InputEvent> events;
  NavigationStatus navigation;
};

// No graphics resources or host callbacks live here. A suspended state rejects presentation
// leases before draining the one possible in-flight present. Callbacks never run under its lock.
class PresentationState
{
public:
  using Clock = std::chrono::steady_clock;
  void Publish(std::shared_ptr<const Snapshot> snapshot);
  std::shared_ptr<const Snapshot> GetSnapshot() const;
  void SetFocused(bool focused);
  void SetRendererReady(bool ready);
  bool IsModal() const { return m_modal.load(); }
  std::optional<std::uint64_t> BeginMenuRequest(std::uint64_t generation,
                                                std::uint64_t input_epoch);
  void FinishMenuRequest(std::optional<std::uint64_t> input_epoch);
  void SetKeyMap(std::vector<int> key_map);
  void Enqueue(InputEvent event);
  FrameInput TakeInput();
  void ResetInput();
  void RequestFrames();
  void SetNavigationStatus(NavigationStatus status);
  void SetNavigationText(NavigationText text);
  NavigationText GetNavigationText() const;
  void SetMenuFontPaths(std::vector<std::string> paths);
  std::vector<std::string> GetMenuFontPaths() const;
  void RequestNavigation(NavigationRequest request,
                         std::optional<std::uint64_t> input_epoch = std::nullopt);
  bool IsInputCurrent(std::uint64_t epoch) const;
  std::uint64_t GetFrameInputEpoch() const;
  std::deque<NavigationRequest> TakeNavigationRequests();

  void PublishCapture(std::uint64_t epoch, bool keyboard, bool mouse);
  CaptureState GetCapture() const;
  // Changes on modal transitions and input resets, including disconnect/focus loss. Input
  // references use it to require neutral controls before allowing them back into gameplay.
  std::uint64_t GetInputEpoch() const { return m_input_epoch.load(); }

  bool TryBeginPresent(Clock::time_point now);
  void EndPresent();
  int GetWaitMilliseconds(Clock::time_point now, int idle_timeout) const;
  bool CanWake() const;
  void Suspend();
  void Resume();
  void FrameCompleted();
  std::uint64_t GetFrameCount() const { return m_frames.load(); }

private:
  bool NeedsPresentationLocked() const;
  void ResetInputLocked();
  mutable std::mutex m_mutex;
  std::condition_variable m_idle;
  std::shared_ptr<const Snapshot> m_snapshot;
  std::deque<InputEvent> m_events;
  std::vector<int> m_key_map;
  NavigationStatus m_navigation;
  NavigationText m_navigation_text;
  std::vector<std::string> m_menu_font_paths;
  std::deque<NavigationRequest> m_navigation_requests;
  bool m_reset = true;
  bool m_focused = true;
  bool m_ready = false;
  bool m_menu_visible = false;
  bool m_menu_pending = false;
  unsigned m_suspensions = 0;
  bool m_presenting = false;
  Clock::time_point m_next_present{};
  // Publish visible/pending capture together; accepting the Qt request must not open a gap.
  std::atomic<bool> m_modal{false};
  std::atomic<unsigned> m_capture{0};
  std::atomic<std::uint64_t> m_input_epoch{0};
  std::atomic<std::uint64_t> m_frames{0};
  std::uint64_t m_frame_epoch = 0;
  unsigned m_requested_frames = 0;
};

PresentationState& GetPresentationState();
}  // namespace BigPicture
