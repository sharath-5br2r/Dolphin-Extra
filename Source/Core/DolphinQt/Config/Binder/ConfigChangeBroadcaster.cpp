// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"

namespace ConfigWidget
{
ConfigChangeBroadcaster& ConfigChangeBroadcaster::Instance()
{
  static ConfigChangeBroadcaster instance;
  return instance;
}

void ConfigChangeBroadcaster::Broadcast()
{
  emit Changed();
}
}  // namespace ConfigWidget
