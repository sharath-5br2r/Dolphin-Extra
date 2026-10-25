// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GameList/CoverManager.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>

#include "Common/StringUtil.h"

namespace GameListCover
{
std::string GetManagedCoverPath(std::string_view game_path)
{
  std::string directory;
  std::string filename;
  if (!SplitPath(game_path, &directory, &filename, nullptr))
    return {};

  return directory + filename + ".cover.png";
}

bool HasManagedCover(std::string_view game_path)
{
  const std::string cover_path = GetManagedCoverPath(game_path);
  return !cover_path.empty() && QFile::exists(QString::fromStdString(cover_path));
}

Result SaveManagedCover(std::string_view game_path, const QString& source_path)
{
  QImageReader reader(source_path);
  reader.setAutoTransform(true);
  QImage image = reader.read();
  if (image.isNull())
    return {Error::InvalidImage, reader.errorString()};

  const QString cover_path = QString::fromStdString(GetManagedCoverPath(game_path));
  QSaveFile output(cover_path);
  if (!output.open(QIODevice::WriteOnly))
    return {Error::CannotWrite, output.errorString()};

  QImageWriter writer(&output, QByteArrayLiteral("png"));
  if (!writer.write(image))
  {
    output.cancelWriting();
    return {Error::CannotWrite, writer.errorString()};
  }

  if (!output.commit())
    return {Error::CannotWrite, output.errorString()};

  return {};
}

Result RemoveManagedCover(std::string_view game_path)
{
  const QString cover_path = QString::fromStdString(GetManagedCoverPath(game_path));
  QFile file(cover_path);
  if (!file.exists() || file.remove())
    return {};

  return {Error::CannotRemove, file.errorString()};
}
}  // namespace GameListCover
