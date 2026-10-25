// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QPalette>
#include <Qt>

namespace ApplicationTheme
{
// The integer values are stored in Qt.ini. Keep them stable so preferences written by older
// Dolphin builds remain meaningful.
enum class Type : int
{
  System = 0,
  LegacyLight = 1,
  LegacyDark = 2,
  User = 3,
  Light = 4,
  DarkGray = 5,
  Dark = 6,

  MinValue = 0,
  MaxValue = 6,
};

inline constexpr Type DEFAULT_TYPE = Type::Dark;

struct Definition
{
  Type type;
  bool use_fusion;
  Qt::ColorScheme color_scheme;
  std::optional<QPalette> palette;
};

constexpr Type Canonicalize(Type type)
{
  switch (type)
  {
  case Type::LegacyLight:
    return Type::Light;
  case Type::LegacyDark:
    return Type::Dark;
  case Type::System:
  case Type::User:
  case Type::Light:
  case Type::DarkGray:
  case Type::Dark:
    return type;
  default:
    return Type::System;
  }
}

Definition GetDefinition(Type type, bool use_system_dark_fallback = false);
bool IsDark(const QPalette& palette);
}  // namespace ApplicationTheme
