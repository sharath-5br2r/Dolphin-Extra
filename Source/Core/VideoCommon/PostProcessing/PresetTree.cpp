// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/PresetTree.h"

#include <algorithm>
#include <string_view>

namespace VideoCommon
{
namespace
{
// Splits on '/' and skips empty segments, so a stray leading or doubled separator cannot produce a
// blank folder node.
std::vector<std::string_view> SplitSegments(std::string_view path)
{
  std::vector<std::string_view> segments;
  for (size_t start = 0; start < path.size();)
  {
    const size_t end = std::min(path.find('/', start), path.size());
    if (end > start)
      segments.push_back(path.substr(start, end - start));
    start = end + 1;
  }
  return segments;
}
}  // namespace

std::vector<PresetTreeNode> BuildPresetTree(const std::vector<std::string>& presets)
{
  std::vector<std::string> sorted = presets;
  std::ranges::sort(sorted);
  const auto duplicates = std::ranges::unique(sorted);
  sorted.erase(duplicates.begin(), duplicates.end());

  std::vector<PresetTreeNode> roots;
  for (const std::string& preset : sorted)
  {
    const std::vector<std::string_view> segments = SplitSegments(preset);
    if (segments.empty())
      continue;

    std::vector<PresetTreeNode>* level = &roots;
    std::string folder_path;
    for (size_t i = 0; i + 1 < segments.size(); ++i)
    {
      if (!folder_path.empty())
        folder_path += '/';
      folder_path += segments[i];

      // Sorting by whole path puts every entry sharing a prefix in one contiguous run, so a
      // folder that already exists is always the last node added at this level. (A leaf can sit
      // there with the same `path` -- "misc" and "misc/x" both give a top-level path of "misc" --
      // hence the is_preset test.)
      if (level->empty() || level->back().is_preset || level->back().path != folder_path)
        level->push_back(PresetTreeNode{.label = std::string(segments[i]), .path = folder_path});

      level = &level->back().children;
    }

    level->push_back(
        PresetTreeNode{.label = std::string(segments.back()), .path = preset, .is_preset = true});
  }
  return roots;
}
}  // namespace VideoCommon
