// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/ShaderParameterEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "VideoCommon/PostProcessing/LibrashaderPostProcessing.h"
#include "VideoCommon/PostProcessing/PostProcessingConfig.h"

namespace VideoCommon
{
namespace Parameters = LibrashaderParameters;

SliderMapping MakeSliderMapping(const Parameters::ParameterInfo& info)
{
  // A slider needs at least one whole step: has_range rules out a non-positive step and an
  // inverted range, whole_steps rules out a range narrower than a single step.
  const bool has_range = info.step > 0.0f && info.maximum > info.minimum;
  const double whole_steps = has_range ? std::round((info.maximum - info.minimum) / info.step) : 0.0;
  if (whole_steps < 1.0)
    return {};

  constexpr double cap = static_cast<double>(MAX_SLIDER_POSITIONS);
  SliderMapping mapping;
  mapping.positions = static_cast<int>(std::min(whole_steps, cap));
  mapping.increment = whole_steps <= cap ? info.step :
                                           (info.maximum - info.minimum) /
                                               static_cast<float>(MAX_SLIDER_POSITIONS);
  return mapping;
}

float SliderValueAt(const Parameters::ParameterInfo& info, const SliderMapping& mapping,
                    int position)
{
  position = std::clamp(position, 0, mapping.positions);
  if (position == mapping.positions)
    return info.maximum;

  const float value = info.minimum + static_cast<float>(position) * mapping.increment;
  if (std::abs(value - info.initial) < mapping.increment * 0.5f)
    return info.initial;
  return value;
}

int SliderPositionFor(const Parameters::ParameterInfo& info, const SliderMapping& mapping,
                      float value)
{
  if (mapping.positions == 0)
    return 0;
  const long position = std::lround((value - info.minimum) / mapping.increment);
  return static_cast<int>(std::clamp<long>(position, 0, mapping.positions));
}

bool IsParameterEditingBackend(std::string_view video_backend)
{
  // The backends' CONFIG_NAMEs. Kept as literals: VideoCommon cannot include the backends.
  static constexpr std::array<std::string_view, 3> supported = {"Vulkan", "D3D", "D3D12"};
  return std::ranges::find(supported, video_backend) != supported.end();
}

ParameterEditingAvailability GetParameterEditingAvailability(bool library_available,
                                                             std::string_view video_backend,
                                                             std::string_view configured_preset)
{
  if (ResolveConfiguredPreset(std::string(configured_preset)).empty())
    return ParameterEditingAvailability::NoPreset;
  if (!library_available)
    return ParameterEditingAvailability::LibraryUnavailable;
  if (!IsParameterEditingBackend(video_backend))
    return ParameterEditingAvailability::UnsupportedBackend;
  return ParameterEditingAvailability::Available;
}

ShaderParameterEditor::ShaderParameterEditor(std::string key,
                                             std::vector<Parameters::ParameterInfo> infos,
                                             const Parameters::Overrides& stored)
    : m_key(std::move(key)), m_status(infos.empty() ? Status::NoParameters : Status::Ok)
{
  m_rows.reserve(infos.size());
  for (Parameters::ParameterInfo& info : infos)
  {
    Row row;
    row.value = info.initial;
    const auto it = std::ranges::find_if(
        stored, [&info](const auto& entry) { return entry.first == info.name; });
    if (it != stored.end())
    {
      row.value = it->second;
      if (info.maximum > info.minimum)
        row.value = std::clamp(row.value, info.minimum, info.maximum);
    }
    row.slider = MakeSliderMapping(info);
    row.info = std::move(info);
    m_rows.push_back(std::move(row));
  }
}

std::unique_ptr<ShaderParameterEditor> ShaderParameterEditor::Open(const std::string& configured_preset)
{
  const std::string preset = ResolveConfiguredPreset(configured_preset);
  if (preset.empty())
    return Failed({}, "no post-processing shader is selected");

  const std::string path = ResolvePresetPath(preset);
  std::string key = Parameters::KeyForPreset(path, preset);
  if (path.empty())
    return Failed(std::move(key), "invalid preset path");

  std::vector<Parameters::ParameterInfo> infos;
  std::string error;
  if (!Parameters::Enumerate(path, &infos, &error))
    return Failed(std::move(key), std::move(error));

  const Parameters::Overrides stored = Parameters::Load(key);
  return std::make_unique<ShaderParameterEditor>(std::move(key), std::move(infos), stored);
}

std::unique_ptr<ShaderParameterEditor> ShaderParameterEditor::Failed(std::string key,
                                                                     std::string error)
{
  // The private constructor is out of make_unique's reach.
  std::unique_ptr<ShaderParameterEditor> editor(new ShaderParameterEditor());
  editor->m_key = std::move(key);
  editor->m_status = Status::Error;
  editor->m_error = std::move(error);
  return editor;
}

int ShaderParameterEditor::GetSliderPosition(size_t index) const
{
  const Row& row = m_rows.at(index);
  return SliderPositionFor(row.info, row.slider, row.value);
}

bool ShaderParameterEditor::IsDefault(size_t index) const
{
  const Row& row = m_rows.at(index);
  return Parameters::IsDefaultValue(row.value, row.info.initial);
}

std::string ShaderParameterEditor::FormatValue(size_t index) const
{
  // fmt never consults the locale for floats unless asked to ('L'), which is what makes this safe
  // to show on a device set to a ','-decimal language.
  const Row& row = m_rows.at(index);
  return fmt::format("{:.{}f}", row.value, Parameters::DecimalsForStep(row.info.step));
}

bool ShaderParameterEditor::SetValue(size_t index, float value)
{
  Row& row = m_rows.at(index);
  if (row.info.step > 0.0f && row.info.maximum > row.info.minimum)
    value = std::clamp(value, row.info.minimum, row.info.maximum);
  return Assign(row, value);
}

bool ShaderParameterEditor::SetSliderPosition(size_t index, int position)
{
  Row& row = m_rows.at(index);
  if (row.slider.positions == 0)
    return false;
  return Assign(row, SliderValueAt(row.info, row.slider, position));
}

bool ShaderParameterEditor::Reset(size_t index)
{
  // Not through SetValue: the default is the preset's to declare, even outside its own range.
  Row& row = m_rows.at(index);
  return Assign(row, row.info.initial);
}

bool ShaderParameterEditor::ResetAll()
{
  bool changed = false;
  for (Row& row : m_rows)
  {
    changed |= row.value != row.info.initial;
    row.value = row.info.initial;
  }
  if (changed)
    Save();
  return changed;
}

Parameters::Overrides ShaderParameterEditor::CollectOverrides() const
{
  Parameters::Overrides overrides;
  for (const Row& row : m_rows)
  {
    if (!Parameters::IsDefaultValue(row.value, row.info.initial))
      overrides.emplace_back(row.info.name, row.value);
  }
  return overrides;
}

bool ShaderParameterEditor::Assign(Row& row, float value)
{
  if (std::abs(value - row.value) < 1e-7f)
    return false;
  row.value = value;
  Save();
  return true;
}

void ShaderParameterEditor::Save() const
{
  // An in-memory write to the base layer (the INI is written when the front end saves the layer),
  // so there is nothing to debounce here even for a slider drag. The write is to the base layer
  // whichever page opened the editor: parameter values are global.
  Parameters::Save(m_key, CollectOverrides());
}
}  // namespace VideoCommon
