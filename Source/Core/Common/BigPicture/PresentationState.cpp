// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/BigPicture/PresentationState.h"

#include <algorithm>
#include <utility>

namespace BigPicture
{
PresentationState& GetPresentationState()
{
  static PresentationState state;
  return state;
}

void PresentationState::ResetInputLocked()
{
  m_events.clear();
  m_navigation_requests.clear();
  m_reset = true;
  ++m_frame_epoch;
  if (IsModal())
    ++m_input_epoch;
  m_menu_pending = false;
  m_capture = 0;
}

void PresentationState::Publish(std::shared_ptr<const Snapshot> snapshot)
{
  std::lock_guard lock(m_mutex);
  const bool modal =
      snapshot &&
      ((snapshot->mode == Mode::InGame && (snapshot->host.lifecycle == Lifecycle::Running ||
                                           snapshot->host.lifecycle == Lifecycle::Paused)) ||
       (snapshot->mode == Mode::Library && snapshot->host.lifecycle == Lifecycle::Idle));
  if (modal && !m_menu_visible)
    ++m_input_epoch;
  if (!m_snapshot || !snapshot || m_snapshot->generation != snapshot->generation ||
      m_menu_visible != modal ||
      (m_menu_pending && snapshot->host.lifecycle != Lifecycle::Running &&
       snapshot->host.lifecycle != Lifecycle::Paused))
  {
    ResetInputLocked();
    m_requested_frames = 0;
  }
  m_snapshot = std::move(snapshot);
  m_menu_visible = modal;
  if (modal || !m_snapshot)
    m_menu_pending = false;
  m_modal = m_menu_visible || m_menu_pending;
}

std::optional<std::uint64_t> PresentationState::BeginMenuRequest(std::uint64_t generation,
                                                                 std::uint64_t input_epoch)
{
  std::lock_guard lock(m_mutex);
  if (!m_focused || IsModal() || !m_snapshot || m_snapshot->generation != generation ||
      input_epoch != m_frame_epoch ||
      (m_snapshot->host.lifecycle != Lifecycle::Running &&
       m_snapshot->host.lifecycle != Lifecycle::Paused))
    return std::nullopt;
  // Consume the opening chord immediately, including while Qt is busy delivering the request.
  ResetInputLocked();
  ++m_input_epoch;
  m_menu_pending = true;
  m_modal = true;
  return m_frame_epoch;
}

void PresentationState::FinishMenuRequest(std::optional<std::uint64_t> input_epoch)
{
  std::lock_guard lock(m_mutex);
  if (m_menu_pending && (!input_epoch || *input_epoch == m_frame_epoch))
  {
    ResetInputLocked();
    m_modal = m_menu_visible;
  }
}

std::shared_ptr<const Snapshot> PresentationState::GetSnapshot() const
{
  std::lock_guard lock(m_mutex);
  return m_snapshot;
}

void PresentationState::SetFocused(bool focused)
{
  std::lock_guard lock(m_mutex);
  if (m_focused != focused)
    ResetInputLocked();
  m_focused = focused;
  if (!focused)
    m_menu_pending = false;
  m_modal = m_menu_visible || m_menu_pending;
}

void PresentationState::SetRendererReady(bool ready)
{
  std::lock_guard lock(m_mutex);
  m_ready = ready;
  ResetInputLocked();
  if (!ready)
  {
    m_menu_visible = false;
    m_menu_pending = false;
    m_requested_frames = 0;
  }
  m_modal = m_menu_visible || m_menu_pending;
}

void PresentationState::SetKeyMap(std::vector<int> key_map)
{
  std::lock_guard lock(m_mutex);
  m_key_map = std::move(key_map);
}

void PresentationState::Enqueue(InputEvent event)
{
  std::lock_guard lock(m_mutex);
  if (event.generation != (m_snapshot ? m_snapshot->generation : 0))
    return;
  // Bound memory without discarding a release and leaving an old press alive. An overflow
  // resets the complete input state before processing the newest event.
  if (m_events.size() >= 512)
  {
    ResetInputLocked();
    m_modal = m_menu_visible;
  }
  m_events.push_back(std::move(event));
}

FrameInput PresentationState::TakeInput()
{
  std::lock_guard lock(m_mutex);
  FrameInput input{m_frame_epoch, m_key_map, std::exchange(m_reset, false), {}, m_navigation};
  input.events.swap(m_events);
  return input;
}

void PresentationState::ResetInput()
{
  std::lock_guard lock(m_mutex);
  ResetInputLocked();
  m_modal = m_menu_visible;
}

void PresentationState::SetNavigationStatus(NavigationStatus status)
{
  std::lock_guard lock(m_mutex);
  m_navigation = std::move(status);
}

void PresentationState::SetNavigationText(NavigationText text)
{
  std::lock_guard lock(m_mutex);
  m_navigation_text = std::move(text);
}

NavigationText PresentationState::GetNavigationText() const
{
  std::lock_guard lock(m_mutex);
  return m_navigation_text;
}

void PresentationState::SetMenuFontPaths(std::vector<std::string> paths)
{
  std::lock_guard lock(m_mutex);
  m_menu_font_paths = std::move(paths);
}

std::vector<std::string> PresentationState::GetMenuFontPaths() const
{
  std::lock_guard lock(m_mutex);
  return m_menu_font_paths;
}

bool PresentationState::IsInputCurrent(std::uint64_t epoch) const
{
  std::lock_guard lock(m_mutex);
  return m_focused && epoch == m_frame_epoch;
}

std::uint64_t PresentationState::GetFrameInputEpoch() const
{
  std::lock_guard lock(m_mutex);
  return m_frame_epoch;
}

void PresentationState::RequestNavigation(NavigationRequest request,
                                          std::optional<std::uint64_t> input_epoch)
{
  std::lock_guard lock(m_mutex);
  if (!m_focused || (input_epoch && *input_epoch != m_frame_epoch) || !m_snapshot ||
      m_snapshot->generation != request.generation || m_snapshot->mode == Mode::Desktop ||
      m_navigation_requests.size() >= 32)
    return;
  request.input_epoch = m_frame_epoch;
  m_navigation_requests.push_back(std::move(request));
}

std::deque<NavigationRequest> PresentationState::TakeNavigationRequests()
{
  std::lock_guard lock(m_mutex);
  std::deque<NavigationRequest> requests;
  requests.swap(m_navigation_requests);
  return requests;
}

void PresentationState::RequestFrames()
{
  std::lock_guard lock(m_mutex);
  // One frame applies focus; the next publishes ImGui's resulting capture state.
  m_requested_frames = 2;
}

void PresentationState::FrameCompleted()
{
  std::lock_guard lock(m_mutex);
  ++m_frames;
  if (m_requested_frames != 0)
    --m_requested_frames;
}

void PresentationState::PublishCapture(std::uint64_t epoch, bool keyboard, bool mouse)
{
  std::lock_guard lock(m_mutex);
  if (epoch == m_frame_epoch)
    m_capture = unsigned(keyboard) | (unsigned(mouse) << 1);
}

CaptureState PresentationState::GetCapture() const
{
  const unsigned capture = m_capture.load();
  const bool modal = IsModal();
  return {modal || bool(capture & 1), modal || bool(capture & 2), modal};
}

bool PresentationState::NeedsPresentationLocked() const
{
  return m_ready && m_focused && (m_menu_visible || m_capture != 0 || m_requested_frames != 0) &&
         m_suspensions == 0;
}

bool PresentationState::CanWake() const
{
  std::lock_guard lock(m_mutex);
  return m_suspensions == 0;
}

bool PresentationState::TryBeginPresent(Clock::time_point now)
{
  std::lock_guard lock(m_mutex);
  if (!NeedsPresentationLocked() || m_presenting || now < m_next_present)
    return false;
  m_presenting = true;
  m_next_present = now + std::chrono::microseconds(16667);
  return true;
}

void PresentationState::EndPresent()
{
  std::lock_guard lock(m_mutex);
  m_presenting = false;
  m_idle.notify_all();
}

int PresentationState::GetWaitMilliseconds(Clock::time_point now, int idle_timeout) const
{
  std::lock_guard lock(m_mutex);
  if (!NeedsPresentationLocked())
    return idle_timeout;
  return static_cast<int>(std::clamp<std::int64_t>(
      std::chrono::ceil<std::chrono::milliseconds>(m_next_present - now).count(), 1, 17));
}

void PresentationState::Suspend()
{
  std::unique_lock lock(m_mutex);
  ++m_suspensions;
  m_idle.wait(lock, [this] { return !m_presenting; });
}

void PresentationState::Resume()
{
  std::lock_guard lock(m_mutex);
  if (m_suspensions != 0)
    --m_suspensions;
}
}  // namespace BigPicture
