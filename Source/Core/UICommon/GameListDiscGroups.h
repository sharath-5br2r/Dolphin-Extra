// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include "UICommon/GameFile.h"

namespace UICommon
{
// Keep every file available for booting and disc changes; only the library presentation is grouped.
class GameListDiscGroups
{
public:
  using Games = std::vector<std::shared_ptr<const GameFile>>;

  void Rebuild(std::span<const std::shared_ptr<const GameFile>> games)
  {
    m_groups.clear();
    for (const auto& game : games)
    {
      if (DiscIO::IsDisc(game->GetPlatform()) && !game->GetGameID().empty() &&
          !game->IsModDescriptor() && !game->IsDatelDisc())
      {
        m_groups[GetKey(*game)].push_back(game);
      }
    }

    std::erase_if(m_groups, [](const auto& entry) {
      const auto& discs = entry.second;
      return std::ranges::all_of(discs, [&](const auto& disc) {
        return disc->GetDiscNumber() == discs.front()->GetDiscNumber();
      });
    });

    for (auto& [key, discs] : m_groups)
    {
      std::ranges::sort(discs, [](const auto& a, const auto& b) {
        return std::tuple{a->GetDiscNumber(), a->GetFilePath()} <
               std::tuple{b->GetDiscNumber(), b->GetFilePath()};
      });
    }
  }

  const Games& GetDiscs(const GameFile& game) const
  {
    static const Games empty;
    if (game.IsModDescriptor() || game.IsDatelDisc())
      return empty;
    const auto it = m_groups.find(GetKey(game));
    return it == m_groups.end() ? empty : it->second;
  }

  bool IsRepresentative(const GameFile& game) const
  {
    const auto& discs = GetDiscs(game);
    return discs.empty() || discs.front()->GetFilePath() == game.GetFilePath();
  }

private:
  // The full game ID includes the region. Do not combine different revisions or platforms.
  using Key = std::tuple<DiscIO::Platform, std::string, u16, DiscIO::Country>;
  static Key GetKey(const GameFile& game)
  {
    return {game.GetPlatform(), game.GetGameID(), game.GetRevision(), game.GetCountry()};
  }

  std::map<Key, Games> m_groups;
};
}  // namespace UICommon
