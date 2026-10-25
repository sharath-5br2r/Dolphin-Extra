// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>
#include <vector>

#include <QDirIterator>
#include <QStringList>

namespace QtUtils
{
// Discover installed text fonts on the Qt owner. Paths, rather than QFont objects or font
// bytes, cross the bridge. Fonts are read lazily by the graphics owner when a menu opens.
inline std::vector<std::string> FindMenuFontFallbacks(const QStringList& directories)
{
  std::map<QString, QString> fonts;
  for (const auto& directory : directories)
  {
    QDirIterator files(directory,
                       {QStringLiteral("*.ttf"), QStringLiteral("*.ttc"), QStringLiteral("*.otf")},
                       QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    for (int i = 0; i < 4096 && files.hasNext(); ++i)
    {
      files.next();
      fonts.try_emplace(files.fileName().toLower(), files.filePath());
    }
  }
  std::vector<std::string> result;
  const auto add_first = [&](const QStringList& names) {
    for (const auto& name : names)
    {
      if (const auto it = fonts.find(name.toLower()); it != fonts.end())
      {
        result.push_back(it->second.toStdString());
        return;
      }
    }
  };
  add_first({QStringLiteral("segoeui.ttf"), QStringLiteral("NotoSans-Regular.ttf"),
             QStringLiteral("DejaVuSans.ttf"), QStringLiteral("Arial.ttf")});
  add_first({QStringLiteral("NotoSansCJK-Regular.ttc"), QStringLiteral("NotoSansCJKjp-Regular.otf"),
             QStringLiteral("NotoSansJP-Regular.ttf"), QStringLiteral("meiryo.ttc"),
             QStringLiteral("Hiragino Sans GB.ttc"), QStringLiteral("Arial Unicode.ttf"),
             QStringLiteral("msgothic.ttc")});
  return result;
}
}  // namespace QtUtils
