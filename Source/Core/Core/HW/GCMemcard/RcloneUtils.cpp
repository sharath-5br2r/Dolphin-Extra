// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/RcloneUtils.h"

#include <cstdlib>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#define X_OK 0
#define access _access
#endif

#include "Common/FileUtil.h"

namespace Memcard
{
std::string FindRclonePath()
{
  std::vector<std::string> candidates;

  const std::string exe_dir = File::GetExeDirectory();
  const std::string user_dir = File::GetUserPath(D_USER_IDX);

#ifdef _WIN32
  if (!exe_dir.empty())
  {
    candidates.push_back(exe_dir + "\\librclone.dll");
    candidates.push_back(exe_dir + "\\librclone.exe");
    candidates.push_back(exe_dir + "\\rclone.exe");
  }
  if (!user_dir.empty())
  {
    candidates.push_back(user_dir + "librclone.dll");
    candidates.push_back(user_dir + "rclone.exe");
  }
  candidates.push_back("rclone.exe");
#elif defined(__APPLE__)
  const std::string bundle_dir = File::GetBundleDirectory();
  if (!bundle_dir.empty())
  {
    candidates.push_back(bundle_dir + "/Contents/Frameworks/librclone.dylib");
    candidates.push_back(bundle_dir + "/Contents/MacOS/librclone");
    candidates.push_back(bundle_dir + "/Contents/MacOS/rclone");
  }
  if (!exe_dir.empty())
  {
    candidates.push_back(exe_dir + "/librclone.dylib");
    candidates.push_back(exe_dir + "/librclone");
    candidates.push_back(exe_dir + "/rclone");
  }
  candidates.push_back("/usr/local/bin/librclone");
  candidates.push_back("/usr/local/bin/rclone");
  candidates.push_back("/opt/homebrew/bin/librclone");
  candidates.push_back("/opt/homebrew/bin/rclone");
#elif defined(ANDROID)
  const std::string native_lib_dir = File::GetGpuDriverDirectory(D_GPU_DRIVER_DIR);
  if (!native_lib_dir.empty())
  {
    candidates.push_back(native_lib_dir + "/librclone.so");
    candidates.push_back(native_lib_dir + "/rclone");
  }
  candidates.push_back(user_dir + "librclone.so");
  candidates.push_back(user_dir + "rclone");
  candidates.push_back("/data/data/com.termux/files/usr/bin/librclone");
  candidates.push_back("/data/data/com.termux/files/usr/bin/rclone");
  candidates.push_back("/data/local/tmp/librclone");
  candidates.push_back("/data/local/tmp/rclone");
  if (const char* data_dir = getenv("EXTERNAL_STORAGE"))
  {
    candidates.push_back(std::string(data_dir) + "/librclone.so");
    candidates.push_back(std::string(data_dir) + "/rclone");
  }
#else
  // Linux / BSD / Steam Deck
  if (!exe_dir.empty())
  {
    candidates.push_back(exe_dir + "/librclone.so");
    candidates.push_back(exe_dir + "/librclone");
    candidates.push_back(exe_dir + "/rclone");
  }
  candidates.push_back("/usr/bin/librclone");
  candidates.push_back("/usr/bin/rclone");
  candidates.push_back("/usr/local/bin/librclone");
  candidates.push_back("/usr/local/bin/rclone");
  candidates.push_back("/home/deck/bin/librclone");
  candidates.push_back("/home/deck/bin/rclone");

  if (const char* home = getenv("HOME"))
  {
    candidates.push_back(std::string(home) + "/bin/librclone");
    candidates.push_back(std::string(home) + "/bin/rclone");
  }

  // When running inside an AppImage, access host filesystem
  const std::vector<std::string> base_candidates = candidates;
  for (const std::string& path : base_candidates)
    candidates.push_back("/proc/1/root" + path);
#endif

  for (const std::string& path : candidates)
  {
    if (access(path.c_str(), X_OK) == 0)
      return path;
  }
  return {};
}
}  // namespace Memcard
