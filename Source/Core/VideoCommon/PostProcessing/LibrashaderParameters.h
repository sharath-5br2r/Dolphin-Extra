// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"

namespace VideoCommon::LibrashaderParameters
{
// A preset parameter, enumerated from librashader's preset_get_runtime_params. This is the
// librashader-engine counterpart to SlangParameter (SlangShader.h:25-33), which the built-in engine
// parses per shader file. The difference is aggregation: SlangParameter is per-pass with no
// whole-preset dedup; ParameterInfo is the preset-wide list, already aggregated and in preset
// order.
struct ParameterInfo
{
  std::string name;
  std::string description;
  float initial = 0.0f;
  float minimum = 0.0f;
  float maximum = 0.0f;
  float step = 0.0f;
};

using Overrides = std::vector<std::pair<std::string, float>>;

// Enumerates the parameters declared by a preset (aggregated across all passes). Returns true on
// success; on failure, returns false and fills *error with a human-readable reason. Never touches a
// GPU: guarded on GetAvailability().available, and fills *error with the availability reason when
// false.
bool Enumerate(const std::string& absolute_preset_path, std::vector<ParameterInfo>* out,
               std::string* error);

// Parses a vector of "name=value" strings into overrides, used by Load after splitting the INI
// value on ';'. Skips malformed entries (no '=', unparseable value, empty name) so they cannot
// poison the list.
Overrides ParseOverrides(const std::vector<std::string>& entries);

// Formats overrides back into "name=value" strings, used by Save before joining with ';'. Only
// non-default values are persisted: resetting a parameter removes it rather than writing the
// default back, so a preset's own defaults can change on a pack update without users being pinned
// to the old ones. A name containing ';' or '=' is skipped (and warned about once): the storage
// format cannot represent it, and half of such an entry would parse as some other parameter.
std::vector<std::string> FormatOverrides(const Overrides& overrides);

// Computes the number of decimal places a spin box should display for a given step, using PCSX2's
// logarithmic formula. The shader pack declares 73 distinct step values; a lookup table cannot
// cover them.
int DecimalsForStep(float step);

// Tests whether a value is within epsilon of the initial default, using PCSX2's relative epsilon so
// floating-point roundtrip error does not mark every value as edited.
bool IsDefaultValue(float value, float initial);

// Derives the key Load and Save file a preset's overrides under: the resolved preset's path
// relative to whichever shaders root it was found in. `absolute_preset_path` is
// ResolvePresetPath(preset_spec)'s result, passed in rather than recomputed so a chain rebuild
// resolves the preset only once. A preset found outside a shaders_slang root, or one that did not
// resolve at all, falls back to the configured name -- resolved through ResolveConfiguredPreset,
// never the raw spec, because a legacy ';'-separated GFX.ini value would otherwise key the
// overrides off a string no other code path produces.
std::string KeyForPreset(const std::string& absolute_preset_path, const std::string& preset_spec);

// Loads the stored overrides for a preset. The section is LibrashaderParameters, the key is the
// preset's path relative to the shaders root (matching PCSX2's shape), and the value is a
// ';'-separated list of "name=value" entries. Returns empty when no overrides are stored.
Overrides Load(const std::string& preset_relative_path);

// Saves overrides for a preset. Writes to the base layer under [LibrashaderParameters], with the
// preset-relative path as the key. When every value is default (overrides is empty), removes the
// key instead of writing an empty string -- which is why "only non-default values are persisted"
// works.
void Save(const std::string& preset_relative_path, const Overrides& overrides);

// Returns the current generation counter. Bumped by Save after writing, so the post-processor can
// poll for edits without the dialog having to signal manually.
u32 CurrentGeneration();
}  // namespace VideoCommon::LibrashaderParameters
