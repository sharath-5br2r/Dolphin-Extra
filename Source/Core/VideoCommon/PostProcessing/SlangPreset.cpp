// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/SlangPreset.h"

#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace VideoCommon
{
namespace
{
constexpr bool IsSeparator(char c)
{
  return c == '/' || c == '\\';
}

// The leading part of a path that ".." must never escape.
struct PathRoot
{
  size_t length = 0;  // bytes of the input consumed
  std::string text;   // normalized rendering: "", "/", "//", "X:/" or "//host/share"
  bool absolute = false;
};

// Recognizes a POSIX root ("/"), a UNC root ("//host/share") and a Windows drive root
// ("X:" / "X:\"). A drive-relative path ("X:foo") is promoted to drive-absolute: preset paths are
// always fully qualified, and promoting keeps ".." off the drive designator.
PathRoot SplitRoot(std::string_view path)
{
  if (path.size() >= 2 && IsSeparator(path[0]) && IsSeparator(path[1]))
  {
    // On Windows the whole "\\host\share" prefix is the root -- the share plays the role a drive
    // letter plays elsewhere -- so ".." must not be able to consume the host or the share name.
    size_t i = 2;
    while (i < path.size() && IsSeparator(path[i]))
      ++i;
    const size_t host_start = i;
    while (i < path.size() && !IsSeparator(path[i]))
      ++i;
    const std::string_view host = path.substr(host_start, i - host_start);
    // "." and ".." are navigation segments, never a host or share name; leave them to the segment
    // loop so "//../a" still collapses to "//a".
    if (host.empty() || host == "." || host == "..")
      return {2, "//", true};
    const size_t host_end = i;
    while (i < path.size() && IsSeparator(path[i]))
      ++i;
    const size_t share_start = i;
    while (i < path.size() && !IsSeparator(path[i]))
      ++i;
    const std::string_view share = path.substr(share_start, i - share_start);
    if (share.empty() || share == "." || share == "..")
      return {host_end, "//" + std::string(host), true};
    return {i, "//" + std::string(host) + '/' + std::string(share), true};
  }
  if (path.size() >= 2 && path[1] == ':' &&
      ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
  {
    const size_t length = (path.size() >= 3 && IsSeparator(path[2])) ? 3 : 2;
    return {length, std::string(path.substr(0, 2)) + '/', true};
  }
  if (!path.empty() && IsSeparator(path[0]))
    return {1, "/", true};
  return {};
}
}  // namespace

// Lexically normalizes a path, collapsing "." and ".." segments without touching the filesystem.
// Both '/' and '\' are accepted as separators and the result always uses '/', which every
// platform's file APIs understand -- this is what makes Windows preset paths (backslashes, drive
// letters) resolve correctly. A leading "/", "//" (UNC) or "X:/" (drive) is preserved, and ".."
// can never escape it.
std::string NormalizePath(const std::string& path)
{
  const PathRoot root = SplitRoot(path);
  const std::string_view view = std::string_view(path).substr(root.length);
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (start <= view.size())
  {
    size_t end = start;
    while (end < view.size() && !IsSeparator(view[end]))
      ++end;
    const std::string_view seg = view.substr(start, end - start);
    if (seg.empty() || seg == ".")
    {
      // skip empty and current-dir segments
    }
    else if (seg == "..")
    {
      if (!parts.empty() && parts.back() != "..")
        parts.pop_back();
      else if (!root.absolute)
        parts.push_back(seg);
    }
    else
    {
      parts.push_back(seg);
    }
    if (end == view.size())
      break;
    start = end + 1;
  }

  // root.text either ends in '/' ("/", "//", "X:/") or is a component prefix that needs one
  // ("//host", "//host/share"); an empty root means a relative path and takes no leading separator.
  std::string result = root.text;
  for (const std::string_view part : parts)
  {
    if (!result.empty() && result.back() != '/')
      result += '/';
    result += part;
  }
  return result;
}

namespace
{
std::string_view Trim(std::string_view s)
{
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return {};
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

// Strips a single pair of surrounding double quotes, if present.
std::string_view Unquote(std::string_view s)
{
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}

// Splits "key = value" on the first '='. Returns false if no '=' present.
bool SplitKeyValue(std::string_view line, std::string* key, std::string* value)
{
  const auto eq = line.find('=');
  if (eq == std::string_view::npos)
    return false;
  *key = std::string(Trim(line.substr(0, eq)));
  *value = std::string(Unquote(Trim(line.substr(eq + 1))));
  return true;
}

std::string ResolvePath(const std::string& base_dir, const std::string& value)
{
  // An absolute shader/reference path stands on its own.
  if (SplitRoot(value).absolute)
    return NormalizePath(value);
  return NormalizePath(base_dir + "/" + value);
}

bool ParseBool(const std::string& value)
{
  return value == "true" || value == "1";
}

ScaleType ParseScaleType(const std::string& value)
{
  if (value == "viewport")
    return ScaleType::Viewport;
  if (value == "absolute")
    return ScaleType::Absolute;
  return ScaleType::Source;
}

SlangWrapMode ParseWrapMode(const std::string& value)
{
  if (value == "repeat")
    return SlangWrapMode::Repeat;
  if (value == "mirrored_repeat")
    return SlangWrapMode::MirroredRepeat;
  if (value == "clamp_to_edge")
    return SlangWrapMode::ClampToEdge;
  return SlangWrapMode::ClampToBorder;
}

float ParseFloat(const std::string& value, float fallback)
{
  const std::string trimmed(Trim(value));
  if (trimmed.empty())
    return fallback;
  char* parse_end = nullptr;
  const float out = std::strtof(trimmed.c_str(), &parse_end);
  if (parse_end == trimmed.c_str())
    return fallback;
  return out;
}

using KeyMap = std::map<std::string, std::string>;

const std::string* Find(const KeyMap& map, const std::string& key)
{
  const auto it = map.find(key);
  return it == map.end() ? nullptr : &it->second;
}

std::vector<std::string> SplitList(const std::string& value, char delim)
{
  std::vector<std::string> out;
  std::string_view view = value;
  size_t start = 0;
  while (start <= view.size())
  {
    const auto pos = view.find(delim, start);
    const auto end = pos == std::string_view::npos ? view.size() : pos;
    const std::string_view seg = Trim(view.substr(start, end - start));
    if (!seg.empty())
      out.emplace_back(seg);
    if (pos == std::string_view::npos)
      break;
    start = end + 1;
  }
  return out;
}

std::string DirectoryOf(const std::string& path)
{
  const auto slash = path.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

constexpr int kMaxReferenceDepth = 16;

std::optional<SlangPresetConfig> ParseSlangPresetImpl(const std::string& text,
                                                      const std::string& base_dir,
                                                      std::string* error,
                                                      const SlangPresetReader& reader, int depth)
{
  KeyMap map;
  std::vector<std::string> references;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line))
  {
    std::string_view view = line;
    if (!view.empty() && view.back() == '\r')
      view.remove_suffix(1);
    view = Trim(view);
    if (view.empty())
      continue;
    if (view.front() == '#')
    {
      constexpr std::string_view kRef = "#reference";
      if (view.size() >= kRef.size() && view.compare(0, kRef.size(), kRef) == 0)
      {
        const std::string_view rest = Trim(view.substr(kRef.size()));
        references.emplace_back(Unquote(rest));
      }
      continue;
    }

    std::string key, value;
    if (SplitKeyValue(view, &key, &value))
      map[key] = value;
  }

  SlangPresetConfig config;

  if (!references.empty())
  {
    if (depth >= kMaxReferenceDepth)
    {
      if (error != nullptr)
        *error = "#reference nesting too deep";
      return std::nullopt;
    }
    for (const std::string& ref : references)
    {
      const std::string ref_path = ResolvePath(base_dir, ref);
      std::string ref_text;
      if (!reader || !reader(ref_path, &ref_text))
      {
        if (error != nullptr)
          *error = "could not read #reference target: " + ref_path;
        return std::nullopt;
      }
      const auto base = ParseSlangPresetImpl(ref_text, DirectoryOf(ref_path), error, reader, depth + 1);
      if (!base)
        return std::nullopt;
      // Later reference wins for structure; params merge in order.
      config.passes = base->passes;
      config.luts = base->luts;
      for (const auto& [k, v] : base->parameter_overrides)
        config.parameter_overrides[k] = v;
    }
  }

  std::set<std::string> consumed;
  auto use = [&](const std::string& k) -> const std::string* {
    consumed.insert(k);
    return Find(map, k);
  };

  const std::string* shaders_value = use("shaders");
  if (shaders_value == nullptr)
  {
    if (references.empty())
    {
      if (error != nullptr)
        *error = "missing 'shaders' count";
      return std::nullopt;
    }
    // Inheriting passes/LUTs from #reference; only parameter overrides below.
  }
  else
  {
    // Local shaders key present: local structure REPLACES inherited.
    config.passes.clear();
    config.luts.clear();

    long pass_count = 0;
    {
      const std::string trimmed(Trim(*shaders_value));
      char* parse_end = nullptr;
      pass_count = std::strtol(trimmed.c_str(), &parse_end, 10);
      if (trimmed.empty() || parse_end == trimmed.c_str() || pass_count < 0)
      {
        if (error != nullptr)
          *error = "invalid 'shaders' count";
        return std::nullopt;
      }
    }

    config.passes.reserve(static_cast<size_t>(pass_count));

    for (long i = 0; i < pass_count; ++i)
  {
    const std::string idx = std::to_string(i);
    const std::string* shader = use("shader" + idx);
    if (shader == nullptr)
    {
      if (error != nullptr)
        *error = "missing 'shader" + idx + "'";
      return std::nullopt;
    }

    SlangPassConfig pass;
    pass.shader_path = ResolvePath(base_dir, *shader);
    if (const std::string* v = use("alias" + idx))
      pass.alias = *v;
    if (const std::string* v = use("filter_linear" + idx))
      pass.filter_linear = ParseBool(*v);
    if (const std::string* v = use("wrap_mode" + idx))
      pass.wrap_mode = ParseWrapMode(*v);
    if (const std::string* v = use("mipmap_input" + idx))
      pass.mipmap_input = ParseBool(*v);
    if (const std::string* v = use("srgb_framebuffer" + idx))
      pass.srgb_framebuffer = ParseBool(*v);
    if (const std::string* v = use("float_framebuffer" + idx))
      pass.float_framebuffer = ParseBool(*v);

    // Scale: axis-specific keys win; fall back to combined scale_type/scale for both axes.
    const std::string* combined_type = use("scale_type" + idx);
    const std::string* combined_scale = use("scale" + idx);
    const std::string* type_x = use("scale_type_x" + idx);
    const std::string* type_y = use("scale_type_y" + idx);
    const std::string* scale_x = use("scale_x" + idx);
    const std::string* scale_y = use("scale_y" + idx);

    if (type_x != nullptr)
      pass.scale_type_x = ParseScaleType(*type_x);
    else if (combined_type != nullptr)
      pass.scale_type_x = ParseScaleType(*combined_type);

    if (type_y != nullptr)
      pass.scale_type_y = ParseScaleType(*type_y);
    else if (combined_type != nullptr)
      pass.scale_type_y = ParseScaleType(*combined_type);

    if (scale_x != nullptr)
      pass.scale_x = ParseFloat(*scale_x, 1.0f);
    else if (combined_scale != nullptr)
      pass.scale_x = ParseFloat(*combined_scale, 1.0f);

    if (scale_y != nullptr)
      pass.scale_y = ParseFloat(*scale_y, 1.0f);
    else if (combined_scale != nullptr)
      pass.scale_y = ParseFloat(*combined_scale, 1.0f);

    config.passes.push_back(std::move(pass));
  }

    // Textures (LUTs): "textures = A;B;C" then per-name keys.
    if (const std::string* textures = use("textures"))
    {
      for (const std::string& name : SplitList(*textures, ';'))
      {
        const std::string* path = use(name);
        if (path == nullptr)
          continue;

        SlangLutConfig lut;
        lut.name = name;
        lut.path = ResolvePath(base_dir, *path);
        if (const std::string* v = use(name + "_wrap_mode"))
          lut.wrap_mode = ParseWrapMode(*v);
        if (const std::string* v = use(name + "_linear"))
          lut.linear = ParseBool(*v);
        if (const std::string* v = use(name + "_mipmap"))
          lut.mipmap = ParseBool(*v);
        config.luts.push_back(std::move(lut));
      }
    }
  }

  // Collect non-structural keys that parse as floats into parameter_overrides.
  for (const auto& [key, value] : map)
  {
    if (consumed.count(key) != 0)
      continue;
    const std::string trimmed(Trim(value));
    if (trimmed.empty())
      continue;
    char* parse_end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &parse_end);
    if (parse_end != trimmed.c_str())
      config.parameter_overrides[key] = parsed;
  }

  return config;
}
}  // namespace

std::optional<SlangPresetConfig> ParseSlangPreset(const std::string& text,
                                                  const std::string& base_dir, std::string* error,
                                                  const SlangPresetReader& reader)
{
  return ParseSlangPresetImpl(text, base_dir, error, reader, 0);
}
}  // namespace VideoCommon
