// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace VideoCommon
{
enum class ScaleType
{
  Source,
  Viewport,
  Absolute
};

enum class SlangWrapMode
{
  ClampToEdge,
  ClampToBorder,
  Repeat,
  MirroredRepeat
};

struct SlangPassConfig
{
  std::string shader_path;  // absolute, preset-relative resolved
  std::string alias;        // may be empty
  bool filter_linear = false;
  SlangWrapMode wrap_mode = SlangWrapMode::ClampToBorder;
  bool mipmap_input = false;
  bool srgb_framebuffer = false;
  bool float_framebuffer = false;
  ScaleType scale_type_x = ScaleType::Source;
  ScaleType scale_type_y = ScaleType::Source;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
};

struct SlangLutConfig
{
  std::string name;  // sampler name referenced by shaders
  std::string path;  // absolute, preset-relative resolved
  SlangWrapMode wrap_mode = SlangWrapMode::ClampToBorder;
  bool linear = false;
  bool mipmap = false;
};

struct SlangPresetConfig
{
  std::vector<SlangPassConfig> passes;
  std::vector<SlangLutConfig> luts;
  std::map<std::string, float> parameter_overrides;  // preset-level #pragma parameter overrides
};

using SlangPresetReader = std::function<bool(const std::string& path, std::string* out)>;

// Lexically normalizes a path, collapsing '.' and '..'. Accepts '/' and '\' as separators and
// always emits '/'. Preserves a leading '/', '//' (UNC) or 'X:/' (Windows drive) root.
std::string NormalizePath(const std::string& path);

// Parses preset text. base_dir is the directory containing the preset (for path resolution).
// Returns std::nullopt with *error set on malformed input.
std::optional<SlangPresetConfig> ParseSlangPreset(const std::string& text,
                                                  const std::string& base_dir, std::string* error,
                                                  const SlangPresetReader& reader = {});
}  // namespace VideoCommon
