// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

#include <fmt/format.h>

#include "Common/FileUtil.h"

#if defined(_WIN32)
#include <windows.h>

#include "Common/CommonFuncs.h"
#include "Common/StringUtil.h"
#else
#include <dlfcn.h>
#endif

namespace VideoCommon::Librashader
{
namespace
{
#if defined(_WIN32)
using LibraryHandle = HMODULE;

LibraryHandle OpenLibrary(const std::string& path)
{
  return LoadLibraryW(UTF8ToWString(path).c_str());
}

void CloseLibrary(LibraryHandle handle)
{
  FreeLibrary(handle);
}

void* GetSymbolFromHandle(LibraryHandle handle, const char* name)
{
  return reinterpret_cast<void*>(GetProcAddress(handle, name));
}

std::string GetLoadError()
{
  return Common::GetLastErrorString();
}
#else
using LibraryHandle = void*;

LibraryHandle OpenLibrary(const std::string& path)
{
  return dlopen(path.c_str(), RTLD_LAZY);
}

void CloseLibrary(LibraryHandle handle)
{
  dlclose(handle);
}

void* GetSymbolFromHandle(LibraryHandle handle, const char* name)
{
  return dlsym(handle, name);
}

std::string GetLoadError()
{
  const char* const error = dlerror();
  return error != nullptr ? std::string(error) : "unknown error";
}
#endif

struct LoadedLibrary
{
  LibraryHandle handle = nullptr;
  CommonFunctions functions;
};

// Fills `out` from the library at `path`. Split out from both public entry points so the test seam
// can load into a scratch struct while GetAvailability() loads into the process-wide one.
Availability Load(const std::string& path, LoadedLibrary* out)
{
  Availability result;

  const LibraryHandle handle = OpenLibrary(path);
  if (handle == nullptr)
  {
    result.reason = fmt::format("failed to load {}: {}", path, GetLoadError());
    return result;
  }

  // Every failure past this point closes the handle before returning: a library we have rejected
  // must not stay mapped, or a later GetSymbol() through a stale handle could still succeed.
  const auto fail = [&](std::string reason) {
    CloseLibrary(handle);
    result.available = false;
    result.reason = std::move(reason);
    return result;
  };

  CommonFunctions functions;

  // The two version queries are resolved by hand because their typedefs are named after the
  // libra_instance_* symbols rather than after the struct members, and because they have to be
  // called before anything else is resolved.
  functions.abi_version = reinterpret_cast<PFN_libra_instance_abi_version>(
      GetSymbolFromHandle(handle, "libra_instance_abi_version"));
  functions.api_version = reinterpret_cast<PFN_libra_instance_api_version>(
      GetSymbolFromHandle(handle, "libra_instance_api_version"));
  if (functions.abi_version == nullptr || functions.api_version == nullptr)
    return fail(fmt::format("{}: no libra_instance_abi_version/api_version", path));

  // Check the versions before resolving anything else, so a library from the wrong librashader
  // release is reported as a version mismatch rather than as whichever symbol it happens to lack.
  // ABI versions are not backwards compatible (librashader.h: "It is not valid to load a
  // librashader C API instance for any ABI version not equal to LIBRASHADER_CURRENT_ABI"), so that
  // one is an equality test; the API version only ever grows, so a newer library is fine.
  const LIBRASHADER_ABI_VERSION abi = functions.abi_version();
  if (abi != LIBRASHADER_CURRENT_ABI)
    return fail(fmt::format("{}: ABI version {}, expected {}", path, abi, LIBRASHADER_CURRENT_ABI));

  const LIBRASHADER_API_VERSION api = functions.api_version();
  if (api < LIBRASHADER_CURRENT_VERSION)
  {
    return fail(fmt::format("{}: API version {}, expected {} or newer", path, api,
                            LIBRASHADER_CURRENT_VERSION));
  }

  // Resolves one required entry point, or bails out naming the symbol. librashader_ld.h substitutes
  // silent no-op stubs for absent symbols instead; that is what turns a truncated or mismatched
  // library into post-processing that quietly does nothing, with no line in the log to explain it.
#define RESOLVE_REQUIRED(name)                                                                     \
  functions.name =                                                                                 \
      reinterpret_cast<PFN_libra_##name>(GetSymbolFromHandle(handle, "libra_" #name));             \
  if (functions.name == nullptr)                                                                   \
    return fail(fmt::format("{}: missing required symbol libra_{}", path, #name));

  RESOLVE_REQUIRED(error_errno)
  RESOLVE_REQUIRED(error_write)
  RESOLVE_REQUIRED(error_free_string)
  RESOLVE_REQUIRED(error_free)
  RESOLVE_REQUIRED(preset_create)
  RESOLVE_REQUIRED(preset_free)
  RESOLVE_REQUIRED(preset_get_runtime_params)
  RESOLVE_REQUIRED(preset_free_runtime_params)

#undef RESOLVE_REQUIRED

  out->handle = handle;
  out->functions = functions;
  result.available = true;
  return result;
}

LoadedLibrary& Global()
{
  static LoadedLibrary s_library;
  return s_library;
}
}  // namespace

std::string LibraryPath()
{
  // Check ANDROID first: it is Linux, so it would otherwise fall through to the generic #else arm.
#if defined(ANDROID)
  return "librashader.so";
#elif defined(__APPLE__)
  return File::GetBundleDirectory() + "/Contents/Frameworks/librashader.dylib";
#elif defined(_WIN32)
  return File::GetExeDirectory() + "\\librashader.dll";
#else
  return "librashader.so";
#endif
}

Availability LoadFromPath(const std::string& path)
{
  // Deliberately loads into a local: callers use this to probe a path, and a probe that replaced
  // the process-wide handle would let a test decide which library the emulator runs against.
  LoadedLibrary scratch;
  const Availability availability = Load(path, &scratch);
  if (scratch.handle != nullptr)
    CloseLibrary(scratch.handle);
  return availability;
}

const Availability& GetAvailability()
{
  static const Availability s_availability = Load(LibraryPath(), &Global());
  return s_availability;
}

const CommonFunctions& Common()
{
  // Forces the load rather than trusting the caller to have asked for availability first; an
  // unloaded library then yields the all-null table the header promises, not a use of stale state.
  GetAvailability();
  return Global().functions;
}

void* GetSymbol(const char* name)
{
  GetAvailability();
  const LoadedLibrary& library = Global();
  if (library.handle == nullptr)
    return nullptr;
  return GetSymbolFromHandle(library.handle, name);
}

std::string DescribeAndFreeError(libra_error_t error)
{
  if (error == nullptr)
    return "";

  const CommonFunctions& common = Common();
  if (common.error_write == nullptr || common.error_free_string == nullptr ||
      common.error_free == nullptr || common.error_errno == nullptr)
  {
    // Reachable only if a caller holds an error from a library that has since failed to load, which
    // cannot happen in practice. The error object leaks: freeing it needs the function that is
    // missing.
    return "librashader error (the library's error functions are unavailable)";
  }

  // Both write and free return 0 on success and 1 when handed a null error
  // (librashader.h:1388-1401).
  char* message = nullptr;
  if (common.error_write(error, &message) != 0 || message == nullptr)
  {
    const int errno_value = static_cast<int>(common.error_errno(error));
    common.error_free(&error);
    return fmt::format("errno {}", errno_value);
  }

  const std::string description(message);
  common.error_free_string(&message);
  common.error_free(&error);
  return description;
}
}  // namespace VideoCommon::Librashader
