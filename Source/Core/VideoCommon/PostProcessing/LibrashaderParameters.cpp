// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/LibrashaderParameters.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"
#include "VideoCommon/PostProcessing/PostProcessingConfig.h"

namespace VideoCommon::LibrashaderParameters
{
namespace
{
// Generation counter, bumped by Save so the post-processor can poll for edits without the dialog
// having to signal manually. Starts at 1 so a zero-initialized last-seen value triggers a load.
std::atomic<u32> s_generation{1};

// One-shot flag for FormatOverrides' unrepresentable-name warning. A preset either declares such a
// parameter or it does not, so one line per run is the whole diagnostic. Only ever touched from the
// Qt thread, which is the only caller of Save.
bool s_reported_unrepresentable_name = false;
}  // namespace

bool Enumerate(const std::string& absolute_preset_path, std::vector<ParameterInfo>* out,
               std::string* error)
{
  out->clear();
  error->clear();

  // Never touch a GPU: this is called from the Qt thread with no device. Guard on availability.
  const Librashader::Availability& avail = Librashader::GetAvailability();
  if (!avail.available)
  {
    *error = avail.reason;
    return false;
  }

  // Parse the preset to get its runtime parameter list. Both calls below are tested against the
  // error handle rather than against its description: an error whose message renders empty would
  // otherwise be read as success, and the code would go on to use a null preset handle.
  libra_shader_preset_t preset = nullptr;
  if (const libra_error_t preset_error =
          Librashader::Common().preset_create(absolute_preset_path.c_str(), &preset))
  {
    *error = Librashader::DescribeAndFreeError(preset_error);
    return false;
  }

  // Get the runtime parameters. The list is freed below; the preset is freed after that.
  // preset_get_runtime_params takes a const preset pointer.
  libra_preset_param_list_t param_list = {};
  if (const libra_error_t params_error =
          Librashader::Common().preset_get_runtime_params(&preset, &param_list))
  {
    *error = Librashader::DescribeAndFreeError(params_error);
    Librashader::Common().preset_free(&preset);
    return false;
  }

  // Copy the fields. Keep the names identical to the librashader C API so this loop is auditable
  // against librashader.h.
  out->reserve(param_list.length);
  for (uint64_t i = 0; i < param_list.length; ++i)
  {
    const libra_preset_param_t& p = param_list.parameters[i];
    ParameterInfo info;
    info.name = p.name ? p.name : "";
    info.description = p.description ? p.description : "";
    info.initial = p.initial;
    info.minimum = p.minimum;
    info.maximum = p.maximum;
    info.step = p.step;
    out->push_back(std::move(info));
  }

  // preset_free_runtime_params takes the list by value, not by pointer: pass param_list itself.
  // This is the kind of thing that is silently wrong elsewhere.
  Librashader::Common().preset_free_runtime_params(param_list);
  Librashader::Common().preset_free(&preset);

  return true;
}

Overrides ParseOverrides(const std::vector<std::string>& entries)
{
  Overrides result;
  result.reserve(entries.size());

  for (const std::string& entry : entries)
  {
    const size_t eq_pos = entry.find('=');
    if (eq_pos == std::string::npos)
      continue;  // No '=': skip

    const std::string name = entry.substr(0, eq_pos);
    const std::string value_str = entry.substr(eq_pos + 1);

    if (name.empty())
      continue;  // Empty name: skip

    float value = 0.0f;
    if (!TryParse(value_str, &value))
      continue;  // Unparseable: skip

    result.emplace_back(name, value);
  }

  return result;
}

std::vector<std::string> FormatOverrides(const Overrides& overrides)
{
  std::vector<std::string> result;
  result.reserve(overrides.size());

  for (const auto& [name, value] : overrides)
  {
    // The stored format is `name=value` pairs joined by ';', so a name containing either character
    // cannot round-trip. `beam;width=2.000000` splits into `beam` -- dropped by ParseOverrides for
    // having no '=' -- and `width=2.000000`, which is then applied to a DIFFERENT parameter. Every
    // name in the shader pack comes from `#pragma parameter` and contains neither character, but
    // documenting that is not the same as enforcing it: dropping the value is the only outcome that
    // cannot corrupt an unrelated parameter.
    if (name.find(';') != std::string::npos || name.find('=') != std::string::npos)
    {
      if (!s_reported_unrepresentable_name)
      {
        WARN_LOG_FMT(VIDEO,
                     "Librashader: parameter name '{}' contains ';' or '=', which the override "
                     "storage format cannot represent; its value will not be persisted",
                     name);
        s_reported_unrepresentable_name = true;
      }
      continue;
    }

    result.push_back(fmt::format("{}={:f}", name, value));
  }

  return result;
}

int DecimalsForStep(float step)
{
  // PCSX2's logarithmic formula (ShaderChainParams.cpp:93-100). The shader pack declares 73
  // distinct step values; a six-entry lookup table leaves 67 undefined.
  if (!(step > 0.0f))
    return 3;
  // Small bias so 0.01f (slightly below 0.01) still yields 2, not 3.
  const double digits = std::ceil(-std::log10(static_cast<double>(step)) - 1e-6);
  return std::clamp(static_cast<int>(digits), 0, 4);
}

bool IsDefaultValue(float value, float initial)
{
  // PCSX2's relative epsilon (ShaderChainParams.cpp:102-106), so floating-point roundtrip error
  // does not mark every value as edited.
  return std::abs(value - initial) <= 1e-6f * std::max(1.0f, std::abs(initial));
}

std::string KeyForPreset(const std::string& absolute_preset_path, const std::string& preset_spec)
{
  const std::string user_root = File::GetUserPath(D_SHADERS_IDX) + "shaders_slang" DIR_SEP;
  if (absolute_preset_path.starts_with(user_root))
    return absolute_preset_path.substr(user_root.size());

  const std::string sys_root =
      File::GetSysDirectory() + SHADERS_DIR DIR_SEP "shaders_slang" DIR_SEP;
  if (absolute_preset_path.starts_with(sys_root))
    return absolute_preset_path.substr(sys_root.size());

  return ResolveConfiguredPreset(preset_spec);
}

Overrides Load(const std::string& preset_relative_path)
{
  const Config::Location location(Config::System::GFX, "LibrashaderParameters",
                                  preset_relative_path);
  const std::optional<std::string> value = Config::GetAsString(location);
  if (!value || value->empty())
    return {};

  // Split on ';' before calling ParseOverrides. ';' is a safe separator because FormatOverrides
  // refuses to write a name containing ';' or '=' at all, so no stored entry can contain either
  // outside its role as separator or as the name/value delimiter.
  const std::vector<std::string> entries = SplitString(*value, ';');
  return ParseOverrides(entries);
}

void Save(const std::string& preset_relative_path, const Overrides& overrides)
{
  const Config::Location location(Config::System::GFX, "LibrashaderParameters",
                                  preset_relative_path);
  Config::Layer* const base_layer = Config::GetLayer(Config::LayerType::Base).get();

  if (overrides.empty())
  {
    // Every value is default: remove the key instead of writing an empty string. This is why "only
    // non-default values are persisted" works.
    base_layer->DeleteKey(location);
  }
  else
  {
    // Join with ';' after calling FormatOverrides. Simple loop join since there's no JoinStrings.
    const std::vector<std::string> entries = FormatOverrides(overrides);
    std::string joined;
    for (size_t i = 0; i < entries.size(); ++i)
    {
      if (i > 0)
        joined += ";";
      joined += entries[i];
    }
    base_layer->Set(location, joined);
  }

  // Bump the generation after writing, so the post-processor's poll detects the change.
  s_generation.fetch_add(1, std::memory_order_relaxed);
}

u32 CurrentGeneration()
{
  return s_generation.load(std::memory_order_relaxed);
}
}  // namespace VideoCommon::LibrashaderParameters
