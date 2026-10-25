// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Runtime.h"
#include "VideoCommon/BigPicture/StandaloneSession.h"

#include "Common/BigPicture/PresentationState.h"
#include "Common/ScopeGuard.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/System.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/Present.h"

namespace VideoCommon::BigPicture
{
void Wakeup()
{
  if (WakeStandalone())
    return;
  auto& system = Core::System::GetInstance();
  auto& state = ::BigPicture::GetPresentationState();
  if (!state.CanWake() ||
      state.GetWaitMilliseconds(::BigPicture::PresentationState::Clock::now(), 0) == 0 ||
      !Core::IsRunning(system))
    return;
  system.GetCPU().WakeupForUI();
  system.GetFifo().WakeupForUI();
}

void PresentPaused()
{
  auto& state = ::BigPicture::GetPresentationState();
  if (!state.TryBeginPresent(::BigPicture::PresentationState::Clock::now()))
    return;
  Common::ScopeGuard done([&state] { state.EndPresent(); });
  if (g_presenter)
    g_presenter->Present();
}
}  // namespace VideoCommon::BigPicture
