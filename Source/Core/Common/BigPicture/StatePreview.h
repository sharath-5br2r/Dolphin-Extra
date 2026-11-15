// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Common/BigPicture/Library.h"

namespace BigPicture
{
// Optional sidecars leave the savestate format unchanged. The exact state timestamp ties a
// preview to its state, including when another Dolphin version replaces a slot.
std::shared_ptr<const LibraryImage> ReadStatePreview(const std::string& path,
                                                     std::uint64_t timestamp);
bool WriteStatePreview(const std::string& path, std::uint64_t timestamp, const LibraryImage& image);
}  // namespace BigPicture
