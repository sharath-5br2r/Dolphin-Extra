// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "DolphinQt/GameList/CoverManager.h"

TEST(CoverManagerTest, UsesDolphinsAdjacentPerGameCoverConvention)
{
  EXPECT_EQ(GameListCover::GetManagedCoverPath("/games/Metroid Prime.iso"),
            "/games/Metroid Prime.cover.png");
  EXPECT_EQ(GameListCover::GetManagedCoverPath("/games/Mario.Kart.Wii.rvz"),
            "/games/Mario.Kart.Wii.cover.png");
  EXPECT_EQ(GameListCover::GetManagedCoverPath("boot.dol"), "boot.cover.png");
  EXPECT_TRUE(GameListCover::GetManagedCoverPath("").empty());
}

TEST(CoverManagerTest, WritesAndReplacesManagedCoversAsPng)
{
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QString game_path = directory.filePath(QStringLiteral("Sample Game.iso"));
  const QString source_path = directory.filePath(QStringLiteral("source.image"));
  const std::string game_path_string = game_path.toStdString();
  const QString cover_path =
      QString::fromStdString(GameListCover::GetManagedCoverPath(game_path_string));

  QImage first_image(3, 2, QImage::Format_ARGB32);
  first_image.fill(QColor(20, 80, 140, 255));
  ASSERT_TRUE(first_image.save(source_path, "PNG"));

  EXPECT_TRUE(GameListCover::SaveManagedCover(game_path_string, source_path).Succeeded());
  EXPECT_TRUE(GameListCover::HasManagedCover(game_path_string));
  EXPECT_EQ(QImageReader::imageFormat(cover_path), QByteArrayLiteral("png"));

  const QImage saved_first_image(cover_path);
  ASSERT_FALSE(saved_first_image.isNull());
  EXPECT_EQ(saved_first_image.size(), first_image.size());
  EXPECT_EQ(saved_first_image.pixelColor(1, 1), first_image.pixelColor(1, 1));

  QImage replacement_image(4, 5, QImage::Format_RGB32);
  replacement_image.fill(QColor(190, 40, 30));
  ASSERT_TRUE(replacement_image.save(source_path, "PNG"));

  EXPECT_TRUE(GameListCover::SaveManagedCover(game_path_string, source_path).Succeeded());
  const QImage saved_replacement_image(cover_path);
  ASSERT_FALSE(saved_replacement_image.isNull());
  EXPECT_EQ(saved_replacement_image.size(), replacement_image.size());
  EXPECT_EQ(saved_replacement_image.pixelColor(2, 3), replacement_image.pixelColor(2, 3));
}

TEST(CoverManagerTest, RejectsInvalidImagesWithoutReplacingTheCover)
{
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QString game_path = directory.filePath(QStringLiteral("Sample Game.iso"));
  const QString valid_source_path = directory.filePath(QStringLiteral("valid.png"));
  const QString invalid_source_path = directory.filePath(QStringLiteral("invalid.png"));
  const std::string game_path_string = game_path.toStdString();
  const QString cover_path =
      QString::fromStdString(GameListCover::GetManagedCoverPath(game_path_string));

  QImage image(2, 2, QImage::Format_RGB32);
  image.fill(Qt::green);
  ASSERT_TRUE(image.save(valid_source_path, "PNG"));
  ASSERT_TRUE(GameListCover::SaveManagedCover(game_path_string, valid_source_path).Succeeded());

  QFile invalid_source(invalid_source_path);
  ASSERT_TRUE(invalid_source.open(QIODevice::WriteOnly));
  ASSERT_EQ(invalid_source.write("not an image"), 12);
  invalid_source.close();

  const GameListCover::Result result =
      GameListCover::SaveManagedCover(game_path_string, invalid_source_path);
  EXPECT_EQ(result.error, GameListCover::Error::InvalidImage);
  EXPECT_FALSE(result.detail.isEmpty());

  const QImage saved_image(cover_path);
  ASSERT_FALSE(saved_image.isNull());
  EXPECT_EQ(saved_image.pixelColor(0, 0), image.pixelColor(0, 0));
}

TEST(CoverManagerTest, RemovesOnlyThePerGameCover)
{
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QString game_path = directory.filePath(QStringLiteral("Sample Game.iso"));
  const QString managed_path = directory.filePath(QStringLiteral("Sample Game.cover.png"));
  const QString shared_path = directory.filePath(QStringLiteral("cover.png"));
  const std::string game_path_string = game_path.toStdString();

  QImage image(2, 2, QImage::Format_RGB32);
  image.fill(Qt::blue);
  ASSERT_TRUE(image.save(managed_path, "PNG"));
  ASSERT_TRUE(image.save(shared_path, "PNG"));

  EXPECT_TRUE(GameListCover::HasManagedCover(game_path_string));
  EXPECT_TRUE(GameListCover::RemoveManagedCover(game_path_string).Succeeded());
  EXPECT_FALSE(QFile::exists(managed_path));
  EXPECT_TRUE(QFile::exists(shared_path));
  EXPECT_FALSE(GameListCover::HasManagedCover(game_path_string));
  EXPECT_TRUE(GameListCover::RemoveManagedCover(game_path_string).Succeeded());
}
