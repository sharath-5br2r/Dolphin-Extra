// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/PostProcessingConfig.h"

#include "Common/Logging/Log.h"

namespace VideoCommon
{
std::string ResolveConfiguredPreset(const std::string& preset_spec)
{
  // Take only the first entry of a ';'-separated chain. Chains were removed, but configs written
  // before that may still hold several presets joined with ';'. The librashader path already used
  // only the first entry; now every path does.
  const auto separator = preset_spec.find(';');
  std::string dropped =
      separator == std::string::npos ? std::string() : preset_spec.substr(separator + 1);
  // A tail of nothing but separators and whitespace ("shader;", "shader; ") drops nothing a user
  // would want to hear about, so it is not worth a warning.
  if (dropped.find_first_not_of(" \t;") == std::string::npos)
    dropped.clear();

  std::string name = preset_spec.substr(0, separator);
  const auto first = name.find_first_not_of(" \t");
  if (first == std::string::npos)
  {
    // Nothing before the separator (";shader"): the entries after it are dropped like any other
    // tail, except that here they were the whole request. Warn anyway -- this is the mis-typed
    // input most in need of an explanation, and it used to fail silently.
    if (!dropped.empty())
    {
      WARN_LOG_FMT(VIDEO,
                   "Post-processing: preset '{}' has an empty first entry, so nothing is loaded; "
                   "only one preset is supported, so '{}' is ignored",
                   preset_spec, dropped);
    }
    return {};
  }
  const auto last = name.find_last_not_of(" \t");
  name = name.substr(first, last - first + 1);

  if (!dropped.empty())
  {
    WARN_LOG_FMT(VIDEO,
                 "Post-processing: only one preset is supported; using '{}' and ignoring '{}'",
                 name, dropped);
  }

  return name;
}
}  // namespace VideoCommon
