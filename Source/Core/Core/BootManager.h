// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>

namespace Core
{
class System;
}
struct BootParameters;
struct WindowSystemInfo;

namespace BootManager
{
// prepare_window runs synchronously after game settings load and before Core::Init starts
// the graphics owner. Frontends can replace an incompatible surface and return its new WSI.
bool BootCore(Core::System& system, std::unique_ptr<BootParameters> parameters,
              const WindowSystemInfo& wsi,
              const std::function<WindowSystemInfo()>& prepare_window = {});

// Synchronise Dolphin's configuration with the SYSCONF (which may have changed during emulation),
// and restore settings that were overridden by per-game INIs or for some other reason.
void RestoreConfig();
}  // namespace BootManager
