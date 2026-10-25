// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "Common/BigPicture/PresentationState.h"
#include "Common/WindowSystemInfo.h"

class VideoBackendBase;

namespace VideoCommon::BigPicture
{
// The idle renderer owns all graphics globals until its thread has finished and been joined.
// The frontend polls status; Stop never waits for a driver that may need native window messages.
class StandaloneSession
{
public:
  enum class Status
  {
    Starting,
    Ready,
    Stopped,
    Failed
  };
  struct Functions
  {
    std::function<bool()> initialize;
    std::function<bool()> render;
    std::function<void()> shutdown;
    std::function<void(bool, void*, bool, float, std::optional<bool>)> update_surface;
  };
  explicit StandaloneSession(Functions functions, ::BigPicture::PresentationState& presentation =
                                                      ::BigPicture::GetPresentationState());
  ~StandaloneSession();
  StandaloneSession(const StandaloneSession&) = delete;
  StandaloneSession& operator=(const StandaloneSession&) = delete;
  static std::unique_ptr<StandaloneSession> Create(VideoBackendBase* backend,
                                                   const WindowSystemInfo& wsi);
  void Stop();
  Status GetStatus() const;
  bool IsFinished() const;

private:
  struct Control;
  static void Run(std::shared_ptr<Control> control, Functions functions,
                  ::BigPicture::PresentationState& presentation);
  std::shared_ptr<Control> m_control;
  std::thread m_thread;
  friend bool IsStandaloneActive();
  friend bool WakeStandalone();
  friend bool ChangeStandaloneSurface(void* surface);
  friend bool ResizeStandaloneSurface(float scale);
  friend bool SetStandaloneFullscreen(bool fullscreen);
};

bool IsStandaloneActive();
bool WakeStandalone();
bool ChangeStandaloneSurface(void* surface);
bool ResizeStandaloneSurface(float scale = 0);
bool SetStandaloneFullscreen(bool fullscreen);
std::mutex& GetBackendInitializationMutex();
}  // namespace VideoCommon::BigPicture
