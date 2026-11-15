// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "Common/BigPicture/Library.h"

class QFileInfo;

namespace BigPictureFiles
{
struct Listing
{
  std::vector<BigPicture::LibraryFile> files;
  std::string parent;
  // Translated; empty on success.
  std::string error;
};

// Lists a folder for the controller-operated pickers: subfolders first, then the files that
// include_file accepts (none when it is empty). An empty path lists home and the drives.
// Runs off the Qt thread; listing stops early, with a partial result, once cancelled.
Listing List(const std::string& path, const std::function<bool(const QFileInfo&)>& include_file,
             const std::atomic<bool>& cancelled);
}  // namespace BigPictureFiles
