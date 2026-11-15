// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureFiles.h"

#include <algorithm>

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace BigPictureFiles
{
namespace
{
constexpr std::size_t MAX_FILES = 4096;
}  // namespace

Listing List(const std::string& path, const std::function<bool(const QFileInfo&)>& include_file,
             const std::atomic<bool>& cancelled)
{
  Listing listing;
  if (path.empty())
  {
    listing.files.push_back({QDir::homePath().toStdString(),
                             QCoreApplication::translate("BigPictureFiles", "Home").toStdString(),
                             true});
    for (const auto& drive : QDir::drives())
    {
      const auto drive_path = drive.absoluteFilePath().toStdString();
      listing.files.push_back({drive_path, drive_path, true});
    }
    return listing;
  }
  const QFileInfo info(QString::fromStdString(path));
  if (!info.isDir() || !info.isReadable())
  {
    listing.error =
        QCoreApplication::translate("BigPictureFiles", "This folder is unavailable or unreadable.")
            .toStdString();
    return listing;
  }
  QDir dir(info.absoluteFilePath());
  if (!dir.isRoot())
  {
    dir.cdUp();
    listing.parent = dir.absolutePath().toStdString();
  }
  QDirIterator it(info.absoluteFilePath(),
                  QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
  std::size_t examined = 0;
  while (it.hasNext() && listing.files.size() < MAX_FILES && examined++ < MAX_FILES * 4 &&
         !cancelled)
  {
    it.next();
    const auto entry = it.fileInfo();
    if (entry.isDir() || (include_file && include_file(entry)))
    {
      listing.files.push_back(
          {entry.absoluteFilePath().toStdString(), entry.fileName().toStdString(), entry.isDir()});
    }
  }
  if (it.hasNext())
    listing.error = QCoreApplication::translate(
                        "BigPictureFiles", "This folder is too large. Enter a more specific path.")
                        .toStdString();
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::ranges::sort(listing.files, [&](const auto& a, const auto& b) {
    if (a.directory != b.directory)
      return a.directory;
    return collator.compare(QString::fromStdString(a.name), QString::fromStdString(b.name)) < 0;
  });
  return listing;
}
}  // namespace BigPictureFiles
