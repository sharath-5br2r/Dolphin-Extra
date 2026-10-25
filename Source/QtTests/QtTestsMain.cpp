// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <gtest/gtest.h>

// Widget tests need a QApplication. macOS and Linux runs pass -platform offscreen; the bundled
// Windows Qt has no offscreen plugin (only qwindows and qdirect2d), so Windows runs use the
// desktop session on the UAT host.
int main(int argc, char** argv)
{
  QApplication app{argc, argv};
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
