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
  const std::string path = FindRclonePath();
  if (s_librclone.Open(path.c_str()) || s_librclone.Open("librclone") ||
      s_librclone.Open("librclone.dll") || s_librclone.Open("librclone.so"))
  {
#ifdef _WIN32
    // When Go builds a -buildmode=c-shared DLL on Windows, its initialization bootstrap
    // (_rt0_amd64_windows_lib) is placed into the .ctors PE section. The standard Windows
    // PE loader / MSVC CRT does not automatically call .ctors constructors on DLL load,
    // leaving Go's runtime_init_done as 0 and causing _cgo_wait_runtime_init_done()
    // to deadlock indefinitely inside WaitForSingleObject().
    // We execute the .ctors initializer function table manually here.
    auto* h = reinterpret_cast<HMODULE>(s_librclone.GetHandle());
    if (h)
    {
      auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(h);
      if (dos->e_magic == IMAGE_DOS_SIGNATURE)
      {
        auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(h) + dos->e_lfanew);
        auto* sect = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
          if (std::strncmp(reinterpret_cast<const char*>(sect[i].Name), ".ctors", 8) == 0)
          {
            using CtorFn = void (*)();
            auto** ctor_table =
                reinterpret_cast<CtorFn*>(reinterpret_cast<BYTE*>(h) + sect[i].VirtualAddress);
            if (ctor_table && *ctor_table)
            {
              (*ctor_table)();
            }
            break;
          }
        }
      }
    }
#endif

    s_librclone.GetSymbol("RcloneInitialize", &s_rclone_init);
    s_librclone.GetSymbol("RcloneFinalize", &s_rclone_finalize);
    s_librclone.GetSymbol("RcloneRPC", &s_rclone_rpc);
    s_librclone.GetSymbol("RcloneFreeString", &s_rclone_free);

    if (s_rclone_rpc)
    {
      if (s_rclone_init)
        s_rclone_init();
      s_librclone_initialized = true;
      return true;
    }
  }
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
