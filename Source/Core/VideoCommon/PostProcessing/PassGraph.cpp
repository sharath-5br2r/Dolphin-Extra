// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/PassGraph.h"

#include <cstdlib>
#include <string_view>

namespace VideoCommon
{
std::set<std::string>
ComputeFeedbackAliases(const std::vector<std::vector<std::string>>& all_sampler_names)
{
  constexpr std::string_view kSuffix = "Feedback";
  std::set<std::string> feedback;
  for (const std::vector<std::string>& names : all_sampler_names)
  {
    for (const std::string& name : names)
    {
      if (name.size() <= kSuffix.size())
        continue;
      if (name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0)
        feedback.insert(name.substr(0, name.size() - kSuffix.size()));
    }
  }
  return feedback;
}

std::optional<u32> ParseOriginalHistoryIndex(const std::string& name)
{
  constexpr std::string_view kPrefix = "OriginalHistory";
  if (name.size() <= kPrefix.size() ||
      name.compare(0, kPrefix.size(), kPrefix) != 0)
  {
    return std::nullopt;
  }
  const std::string digits = name.substr(kPrefix.size());
  for (const char c : digits)
  {
    if (c < '0' || c > '9')
      return std::nullopt;
  }
  return static_cast<u32>(std::strtoul(digits.c_str(), nullptr, 10));
}

u32 ComputeMaxHistoryIndex(const std::vector<std::vector<std::string>>& all_sampler_names)
{
  u32 max_index = 0;
  for (const std::vector<std::string>& names : all_sampler_names)
  {
    for (const std::string& name : names)
    {
      if (const std::optional<u32> index = ParseOriginalHistoryIndex(name))
        max_index = std::max(max_index, *index);
    }
  }
  return max_index;
}

std::set<size_t> ComputeMipmapSourcePasses(const std::vector<bool>& mipmap_input_flags)
{
  std::set<size_t> sources;
  for (size_t i = 1; i < mipmap_input_flags.size(); ++i)
  {
    if (mipmap_input_flags[i])
      sources.insert(i - 1);
  }
  return sources;
}
}  // namespace VideoCommon
