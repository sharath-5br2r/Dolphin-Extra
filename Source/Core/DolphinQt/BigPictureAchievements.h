// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>

#include "Common/BigPicture/Achievements.h"

// Qt-owner adapter. Immutable badge values are reused until their pixels change.
class BigPictureAchievements
{
public:
  std::shared_ptr<const BigPicture::AchievementsSnapshot> Read();
  void Clear() { m_images.clear(); }

private:
  std::map<std::string, std::shared_ptr<const BigPicture::LibraryImage>> m_images;
};
