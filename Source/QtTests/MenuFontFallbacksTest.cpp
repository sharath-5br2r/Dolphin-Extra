// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "DolphinQt/QtUtils/MenuFontFallbacks.h"

TEST(MenuFontFallbacksTest, FindsInstalledTextFallbacksAndToleratesMissingFamilies)
{
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  EXPECT_TRUE(QtUtils::FindMenuFontFallbacks({directory.path()}).empty());
  ASSERT_TRUE(QDir(directory.path()).mkdir(QStringLiteral("nested")));
  const auto latin = directory.filePath(QStringLiteral("nested/DejaVuSans.ttf"));
  const auto cjk = directory.filePath(QStringLiteral("meiryo.ttc"));
  for (const auto& path : {latin, cjk, directory.filePath(QStringLiteral("unknown.ttf"))})
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QFile::WriteOnly));
  }
  const auto fonts = QtUtils::FindMenuFontFallbacks({directory.path(), directory.path()});
  ASSERT_EQ(fonts.size(), 2u);
  EXPECT_EQ(fonts[0], latin.toStdString());
  EXPECT_EQ(fonts[1], cjk.toStdString());
}
