// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

namespace Memcard
{
// Returns the effective path to rclone.conf.
// Checks Config::MAIN_CLOUDSYNC_CONFIG_FILE first; if non-empty, returns it.
// Next, checks for rclone.conf in Dolphin's User directory (e.g. sandbox safe).
// Otherwise returns the default rclone.conf path in Dolphin's User directory.
std::string GetRcloneConfigPath();

// Invokes an rclone RPC method using librclone / rclone rc --loopback.
// Passes the configured rclone.conf.
// Returns the JSON output on success, or std::nullopt on error.
std::optional<std::string> RcloneRPC(const std::string& method, const std::string& json_input);
}  // namespace Memcard
