// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/RcloneUtils.h"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#else
#include <io.h>
#include <windows.h>
#define X_OK 0
#define access _access
#endif

#include "Common/DynamicLibrary.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCMemcard/CloudSync.h"

namespace Memcard
{
struct RcloneRPCResult
{
  char* Output;
  int Status;
};

#ifdef HAVE_STATIC_RCLONE
extern "C" {
void RcloneInitialize();
void RcloneFinalize();
RcloneRPCResult RcloneRPC(char* method, char* input);
void RcloneFreeString(char* str);
}
#endif

using RcloneInitializeFn = void (*)();
using RcloneFinalizeFn = void (*)();
using RcloneRPCFn = RcloneRPCResult (*)(char* method, char* input);
using RcloneFreeStringFn = void (*)(char* str);

static std::mutex s_librclone_mutex;
static bool s_librclone_initialized = false;
static RcloneInitializeFn s_rclone_init = nullptr;
static RcloneFinalizeFn s_rclone_finalize = nullptr;
static RcloneRPCFn s_rclone_rpc = nullptr;
static RcloneFreeStringFn s_rclone_free = nullptr;

std::string GetRcloneConfigPath()
{
  const std::string custom_config = Config::Get(Config::MAIN_CLOUDSYNC_CONFIG_FILE);
  if (!custom_config.empty() && File::Exists(custom_config))
    return custom_config;

  const std::string user_conf = File::GetUserPath(D_USER_IDX) + "rclone.conf";
  if (File::Exists(user_conf))
    return user_conf;

  // If a custom path was specified even if not yet created, prefer it
  if (!custom_config.empty())
    return custom_config;

  return user_conf;
}

static bool EnsureLibrcloneLoaded()
{
  if (s_librclone_initialized)
    return true;

#ifdef HAVE_STATIC_RCLONE
  s_rclone_init = &RcloneInitialize;
  s_rclone_finalize = &RcloneFinalize;
  s_rclone_rpc = &RcloneRPC;
  s_rclone_free = &RcloneFreeString;
  s_rclone_init();
  s_librclone_initialized = true;
  return true;
#else
  return false;
#endif
}

std::optional<std::string> RcloneRPC(const std::string& method, const std::string& json_input)
{
  std::lock_guard<std::mutex> lock(s_librclone_mutex);

  // Directly invoke via in-process librclone
  if (EnsureLibrcloneLoaded() && s_rclone_rpc)
  {
    std::string in = json_input.empty() ? "{}" : json_input;
    std::string m = method;
    RcloneRPCResult res = s_rclone_rpc(m.data(), in.data());
    if (res.Output)
    {
      std::string out(res.Output);
      if (s_rclone_free)
        s_rclone_free(res.Output);
      else
        std::free(res.Output);

      if (res.Status == 200 || res.Status == 0)
        return out;
    }
  }

  return std::nullopt;
}
}  // namespace Memcard
