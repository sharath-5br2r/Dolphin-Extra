// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace Memcard
{
// Searches a list of common install and bundled locations for the rclone/librclone binary,
// across Windows, macOS, Linux, and Android. Returns an empty string if not found.
std::string FindRclonePath();
}  // namespace Memcard
