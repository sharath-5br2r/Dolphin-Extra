// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>

#include <QString>

namespace GameListCover
{
enum class Error
{
  None,
  InvalidImage,
  CannotWrite,
  CannotRemove,
};

struct Result
{
  Error error = Error::None;
  QString detail;

  bool Succeeded() const { return error == Error::None; }
};

std::string GetManagedCoverPath(std::string_view game_path);
bool HasManagedCover(std::string_view game_path);
Result SaveManagedCover(std::string_view game_path, const QString& source_path);
Result RemoveManagedCover(std::string_view game_path);
}  // namespace GameListCover
