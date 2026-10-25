// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/StandaloneSession.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "Common/ScopeGuard.h"
#include "Common/Thread.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon::BigPicture
{
struct StandaloneSession::Control
{
  std::mutex mutex;
  std::condition_variable wake;
  bool stop = false;
  bool surface_changed = false;
  void* surface = nullptr;
  bool resized = false;
  float scale = 0;
  std::optional<bool> fullscreen;
  unsigned wake_revision = 0;
  std::atomic<Status> status{Status::Starting};
  std::atomic<bool> finished{false};
  static std::mutex active_mutex;
  static std::weak_ptr<Control> active;
};
std::mutex StandaloneSession::Control::active_mutex;
std::weak_ptr<StandaloneSession::Control> StandaloneSession::Control::active;

std::mutex& GetBackendInitializationMutex()
{
  static std::mutex mutex;
  return mutex;
}

StandaloneSession::StandaloneSession(Functions functions,
                                     ::BigPicture::PresentationState& presentation)
    : m_control(std::make_shared<Control>())
{
  const std::lock_guard lock(Control::active_mutex);
  if (!Control::active.expired())
  {
    m_control->status = Status::Failed;
    m_control->finished = true;
    return;
  }
  Control::active = m_control;
  m_thread =
      std::thread(&StandaloneSession::Run, m_control, std::move(functions), std::ref(presentation));
}

StandaloneSession::~StandaloneSession()
{
  Stop();
  if (m_thread.joinable())
    m_thread.join();
  const std::lock_guard lock(Control::active_mutex);
  if (Control::active.lock() == m_control)
    Control::active.reset();
}

void StandaloneSession::Stop()
{
  const std::lock_guard lock(m_control->mutex);
  m_control->stop = true;
  m_control->wake.notify_all();
}

StandaloneSession::Status StandaloneSession::GetStatus() const
{
  return m_control->status;
}
bool StandaloneSession::IsFinished() const
{
  return m_control->finished;
}

void StandaloneSession::Run(std::shared_ptr<Control> control, Functions functions,
                            ::BigPicture::PresentationState& presentation)
{
  Common::SetCurrentThreadName("BigPictureGraphics");
  Core::DeclareAsGPUThread();
  Common::ScopeGuard finished([&] {
    Core::UndeclareAsGPUThread();
    control->finished = true;
  });
  {
    const std::lock_guard lock(control->mutex);
    if (control->stop)
    {
      control->status = Status::Stopped;
      return;
    }
  }
  {
    const std::lock_guard init_lock(GetBackendInitializationMutex());
    if (!functions.initialize())
    {
      control->status = Status::Failed;
      return;
    }
  }
  control->status = Status::Ready;
  unsigned failed_frames = 0;
  bool failed = false;
  for (;;)
  {
    unsigned revision;
    {
      std::unique_lock lock(control->mutex);
      if (control->stop)
        break;
      revision = control->wake_revision;
      const bool changed = std::exchange(control->surface_changed, false);
      const bool resized = std::exchange(control->resized, false);
      const auto fullscreen = std::exchange(control->fullscreen, std::nullopt);
      const float scale = std::exchange(control->scale, 0);
      const auto surface = control->surface;
      lock.unlock();
      if (functions.update_surface && (changed || resized || fullscreen))
        functions.update_surface(changed, surface, resized, scale, fullscreen);
    }
    const auto now = ::BigPicture::PresentationState::Clock::now();
    if (presentation.TryBeginPresent(now))
    {
      const bool success = functions.render();
      presentation.EndPresent();
      failed_frames = success ? 0 : failed_frames + 1;
      if (failed_frames >= 60)
      {
        failed = true;
        break;
      }
    }
    const int wait_ms = std::max(
        1, presentation.GetWaitMilliseconds(::BigPicture::PresentationState::Clock::now(), 1000));
    std::unique_lock lock(control->mutex);
    control->wake.wait_for(lock, std::chrono::milliseconds(wait_ms),
                           [&] { return control->stop || control->wake_revision != revision; });
  }
  functions.shutdown();
  control->status = failed ? Status::Failed : Status::Stopped;
}

std::unique_ptr<StandaloneSession> StandaloneSession::Create(VideoBackendBase* backend,
                                                             const WindowSystemInfo& wsi)
{
  Functions functions;
  functions.initialize = [backend, wsi] {
    return Core::IsUninitialized(Core::System::GetInstance()) && wsi.render_surface &&
           wsi.type != WindowSystemType::Headless && backend->SupportsUI() &&
           backend->InitializeUI(wsi);
  };
  functions.render = [backend] { return backend->PresentUI(); };
  functions.shutdown = [backend] {
    g_gfx->WaitForGPUIdle();
    backend->Shutdown();
  };
  functions.update_surface = [](bool changed, void* surface, bool resized, float scale,
                                std::optional<bool> fullscreen) {
    if (changed)
      g_presenter->ChangeSurface(surface);
    if (resized)
    {
      g_presenter->ResizeSurface();
      if (scale > 0)
        g_presenter->SetUIScale(scale);
    }
    if (fullscreen && g_ActiveConfig.ExclusiveFullscreenEnabled())
      g_gfx->SetFullscreen(*fullscreen);
  };
  return std::make_unique<StandaloneSession>(std::move(functions));
}

bool IsStandaloneActive()
{
  const std::lock_guard lock(StandaloneSession::Control::active_mutex);
  return !StandaloneSession::Control::active.expired();
}

bool WakeStandalone()
{
  using Control = StandaloneSession::Control;
  const std::lock_guard active_lock(Control::active_mutex);
  const auto control = Control::active.lock();
  if (!control)
    return false;
  const std::lock_guard lock(control->mutex);
  ++control->wake_revision;
  control->wake.notify_all();
  return true;
}

bool ChangeStandaloneSurface(void* surface)
{
  using Control = StandaloneSession::Control;
  const std::lock_guard active_lock(Control::active_mutex);
  const auto control = Control::active.lock();
  if (!control)
    return false;
  const std::lock_guard lock(control->mutex);
  control->surface = surface;
  control->surface_changed = true;
  ++control->wake_revision;
  control->wake.notify_all();
  return true;
}

bool ResizeStandaloneSurface(float scale)
{
  using Control = StandaloneSession::Control;
  const std::lock_guard active_lock(Control::active_mutex);
  const auto control = Control::active.lock();
  if (!control)
    return false;
  const std::lock_guard lock(control->mutex);
  control->resized = true;
  if (scale > 0)
    control->scale = scale;
  ++control->wake_revision;
  control->wake.notify_all();
  return true;
}

bool SetStandaloneFullscreen(bool fullscreen)
{
  using Control = StandaloneSession::Control;
  const std::lock_guard active_lock(Control::active_mutex);
  const auto control = Control::active.lock();
  if (!control)
    return false;
  const std::lock_guard lock(control->mutex);
  control->fullscreen = fullscreen;
  ++control->wake_revision;
  control->wake.notify_all();
  return true;
}
}  // namespace VideoCommon::BigPicture
