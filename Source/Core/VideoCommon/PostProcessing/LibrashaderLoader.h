// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Runtime-independent part of the librashader C API. With no LIBRA_RUNTIME_* macro defined this
// header pulls in nothing but the C standard library, so it is safe in VideoCommon. Each backend
// defines its own LIBRA_RUNTIME_* macro in its own .cpp/.mm and resolves its chain functions
// through GetSymbol(); librashader_ld.h is deliberately unused, because its single
// libra_instance_t is macro-gated and its loader is static inline, so two translation units with
// different runtime sets would disagree on the struct's layout.
#include <librashader.h>

#include <string>

namespace VideoCommon::Librashader
{
struct Availability
{
  bool available = false;
  // Human-readable failure cause, for the log line the user will be asked about. Empty on success.
  std::string reason;
};

struct CommonFunctions
{
  PFN_libra_instance_abi_version abi_version = nullptr;
  PFN_libra_instance_api_version api_version = nullptr;
  PFN_libra_error_errno error_errno = nullptr;
  PFN_libra_error_write error_write = nullptr;
  PFN_libra_error_free_string error_free_string = nullptr;
  PFN_libra_error_free error_free = nullptr;
  PFN_libra_preset_create preset_create = nullptr;
  PFN_libra_preset_free preset_free = nullptr;
  PFN_libra_preset_get_runtime_params preset_get_runtime_params = nullptr;
  PFN_libra_preset_free_runtime_params preset_free_runtime_params = nullptr;
};

// Loads the library on first call and caches the result. Thread-safe.
const Availability& GetAvailability();

// Runtime-independent entry points. Only valid when GetAvailability().available.
const CommonFunctions& Common();

// Raw symbol lookup for a backend's libra_<api>_* functions. nullptr when unavailable or absent.
void* GetSymbol(const char* name);

// Formats a libra_error_t and frees it. Returns "" for nullptr. Do NOT use the returned string as
// the success test: librashader is free to hand back a non-null error whose message renders empty,
// which would then read as success. Test the handle, then call this to describe it.
std::string DescribeAndFreeError(libra_error_t error);

// Absolute path to the packaged library: next to the executable on Windows,
// Contents/Frameworks inside the bundle on macOS, the bare soname on Android and Linux.
std::string LibraryPath();

// Loads from an explicit path without touching the cached global. Test seam.
Availability LoadFromPath(const std::string& path);
}  // namespace VideoCommon::Librashader
