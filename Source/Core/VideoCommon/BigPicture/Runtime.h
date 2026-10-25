// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace VideoCommon::BigPicture
{
// Wake existing owners; never submit UI-thread work into AsyncRequests' CPU/video SPSC queue.
void Wakeup();
// Called only by the graphics owner while guest execution is paused.
void PresentPaused();
}  // namespace VideoCommon::BigPicture
