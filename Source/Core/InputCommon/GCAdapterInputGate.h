// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cmath>
#include <cstdint>

#include "InputCommon/GCPadStatus.h"

// Native adapter input bypasses ControlReference. Keep calibration/connection flags intact,
// but apply the same modal capture and release requirement to its guest-facing status.
class GCAdapterInputGate
{
public:
  GCPadStatus Filter(GCPadStatus status, bool modal, std::uint64_t epoch, bool menu_channel)
  {
    if (menu_channel)
      return status;
    if (epoch != m_epoch)
    {
      m_epoch = epoch;
      m_require_release = true;
    }
    const bool active =
        (status.button & ~(PAD_GET_ORIGIN | PAD_USE_ORIGIN | PAD_ERR_STATUS)) ||
        std::abs(int(status.stickX) - 128) > 16 || std::abs(int(status.stickY) - 128) > 16 ||
        std::abs(int(status.substickX) - 128) > 16 || std::abs(int(status.substickY) - 128) > 16 ||
        status.triggerLeft > 25 || status.triggerRight > 25;
    if (!active)
      m_require_release = false;
    if (modal || m_require_release)
    {
      GCPadStatus neutral;
      neutral.button = status.button & (PAD_GET_ORIGIN | PAD_USE_ORIGIN | PAD_ERR_STATUS);
      neutral.isConnected = status.isConnected;
      return neutral;
    }
    return status;
  }

private:
  std::uint64_t m_epoch = 0;
  bool m_require_release = false;
};
