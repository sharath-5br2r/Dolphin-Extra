// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VideoCommon
{
// Reads the file at `path` into `*out`; returns false if it cannot be read.
using SlangFileReader = std::function<bool(const std::string& path, std::string* out)>;

// Recursively expands `#include "..."` directives in slang source text. base_dir roots the
// first level; nested includes resolve relative to their includer's directory (matching
// glslang's ShaderIncluder). Unreadable includes are left as-is. This must run BEFORE
// ParseSlangShader, because crt-royale ships passes whose #pragma stage bodies live in an
// included header.
std::string ExpandSlangIncludes(const std::string& text, const std::string& base_dir,
                                const SlangFileReader& reader);
struct SlangParameter
{
  std::string id;
  std::string label;
  float default_value = 0.0f;
  float min_value = 0.0f;
  float max_value = 0.0f;
  float step = 0.0f;
};

bool ResolveShaderParameter(const std::map<std::string, float>& overrides,
                            const std::vector<SlangParameter>& parameters, std::string_view name,
                            float* out);

struct SlangShaderSource
{
  std::string name;             // from #pragma name (may be empty)
  std::string format;           // from #pragma format (may be empty)
  std::string vertex_source;    // code selected by #pragma stage vertex + common prologue
  std::string fragment_source;  // code selected by #pragma stage fragment + common prologue
  std::vector<SlangParameter> parameters;
};

// Parses a .slang file's raw text. Lines before the first "#pragma stage" are
// common to both stages. Returns nullopt with *error on malformed input.
std::optional<SlangShaderSource> ParseSlangShader(const std::string& text, std::string* error);
}  // namespace VideoCommon
