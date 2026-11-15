// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Common/BigPicture/Library.h"

namespace BigPicture
{
struct Achievement
{
  std::uint32_t id = 0;
  std::string title;
  std::string description;
  std::string points;
  std::string unlocked_at;
  std::string type;
  bool unlocked = false;
  float progress = 0;
  std::string measured;
  std::shared_ptr<const LibraryImage> badge;
};
struct AchievementGroup
{
  std::uint32_t id = 0;
  std::string title;
  std::vector<Achievement> achievements;
};
struct AchievementSubset
{
  std::uint32_t id = 0;
  std::string title;
  std::string summary;
  std::string progress_label;
  float progress = 0;
  std::shared_ptr<const LibraryImage> badge;
  std::vector<AchievementGroup> groups;
};
// The host copies client-owned data under the achievement manager lock. The render owner
// never retains rcheevos pointers or performs network/file operations.
struct AchievementsSnapshot
{
  std::vector<AchievementSubset> subsets;
};
}  // namespace BigPicture
