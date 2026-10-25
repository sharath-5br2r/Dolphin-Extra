// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "VideoCommon/PostProcessing/LibrashaderParameters.h"

namespace VideoCommon
{
// The editor rules every front end shares, so the desktop dialog and the Android screen cannot
// drift apart: how a parameter maps onto a slider, and when editing is offered at all.

// Ranges with more whole steps than this are capped, which stretches the value per position.
constexpr int MAX_SLIDER_POSITIONS = 10000;

// A slider runs over the integer positions [0, positions], one per whole step of the parameter.
// positions == 0 means the parameter gets no slider: a non-positive step, an empty or inverted
// range, or a range narrower than one step has no usable positions, so those get the numeric field
// alone.
struct SliderMapping
{
  int positions = 0;
  float increment = 0.0f;
};

SliderMapping MakeSliderMapping(const LibrashaderParameters::ParameterInfo& info);

// The value at a slider position. The last position is the maximum exactly, even when the range is
// not a whole number of steps, and the position nearest the default yields the default exactly:
// minimum + p * increment accumulates float noise on wide ranges, which would otherwise leave Reset
// enabled at the default position and persist that noise. Out-of-range positions clamp.
float SliderValueAt(const LibrashaderParameters::ParameterInfo& info, const SliderMapping& mapping,
                    int position);

// The slider position nearest a value, clamped to [0, positions].
int SliderPositionFor(const LibrashaderParameters::ParameterInfo& info,
                      const SliderMapping& mapping, float value);

// Whether stored parameter values take effect on this backend (by its config name). These are the
// backends whose librashader runtime is in scope for parameter editing. OGL is excluded because on
// Android it is GLES, which librashader's GL runtime cannot drive; there the built-in engine runs
// and ignores stored values.
bool IsParameterEditingBackend(std::string_view video_backend);

// Kept in this order: the Android binding passes it across JNI as its ordinal.
enum class ParameterEditingAvailability
{
  Available,
  NoPreset,
  LibraryUnavailable,
  UnsupportedBackend,
};

// Reports the first unmet requirement, in the order a user can act on them. configured_preset is
// the raw GFX_ENHANCE_POST_SHADER value; it is resolved the way the post-processor resolves it, so
// a legacy chain counts as its first entry.
ParameterEditingAvailability GetParameterEditingAvailability(bool library_available,
                                                             std::string_view video_backend,
                                                             std::string_view configured_preset);

// The model behind a shader parameter editor: one row per parameter the preset declares, holding
// its current value. Front ends draw the rows and forward edits here; the editor applies the shared
// rules and persists every change.
class ShaderParameterEditor
{
public:
  // Kept in this order: the Android binding passes it across JNI as its ordinal.
  enum class Status
  {
    Ok,
    NoParameters,
    Error,
  };

  struct Row
  {
    LibrashaderParameters::ParameterInfo info;
    SliderMapping slider;
    float value = 0.0f;
  };

  // Rows for `infos`, in preset order, each starting at its stored value when `stored` has one and
  // at its default otherwise. A stored value outside the declared range is clamped into it, and the
  // clamped value is what the next edit persists. Stored names the preset does not declare are
  // ignored: a pack update can rename a parameter, and the old entry must not block the editor.
  ShaderParameterEditor(std::string key, std::vector<LibrashaderParameters::ParameterInfo> infos,
                        const LibrashaderParameters::Overrides& stored);

  // Opens the preset a GFX_ENHANCE_POST_SHADER value names: resolves it the way the post-processor
  // does, enumerates its parameters and applies the values stored for it. Never null; a preset that
  // cannot be loaded gives a Status::Error editor carrying the reason.
  static std::unique_ptr<ShaderParameterEditor> Open(const std::string& configured_preset);

  // An editor with no rows that reports why it could not load.
  static std::unique_ptr<ShaderParameterEditor> Failed(std::string key, std::string error);

  Status GetStatus() const { return m_status; }
  const std::string& GetError() const { return m_error; }
  // The [LibrashaderParameters] key the values are stored under.
  const std::string& GetKey() const { return m_key; }
  const std::vector<Row>& GetRows() const { return m_rows; }

  int GetSliderPosition(size_t index) const;
  bool IsDefault(size_t index) const;
  // The value with DecimalsForStep places and a '.' separator, whatever the locale.
  std::string FormatValue(size_t index) const;

  // The mutators persist through LibrashaderParameters::Save, which a running chain picks up on its
  // next frame, and return whether anything changed. A change below 1e-7 is no change: it neither
  // writes nor bumps the generation, so a redundant edit cannot make the chain push every parameter.
  //
  // SetValue clamps to the declared range when the parameter has a usable one (a positive step and
  // maximum > minimum); without one there is nothing to clamp to.
  bool SetValue(size_t index, float value);
  // Ignored (false) for a row without a slider.
  bool SetSliderPosition(size_t index, int position);
  bool Reset(size_t index);
  bool ResetAll();

  // The non-default values, in preset order. Only these are stored, so a reset parameter follows
  // the preset's own default even after a pack update changes it.
  LibrashaderParameters::Overrides CollectOverrides() const;

private:
  ShaderParameterEditor() = default;

  bool Assign(Row& row, float value);
  void Save() const;

  std::string m_key;
  Status m_status = Status::Ok;
  std::string m_error;
  std::vector<Row> m_rows;
};
}  // namespace VideoCommon
