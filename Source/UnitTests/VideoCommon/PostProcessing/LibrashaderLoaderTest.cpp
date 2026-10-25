// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace Librashader = VideoCommon::Librashader;

TEST(LibrashaderLoader, MissingLibraryReportsWhyRatherThanCrashing)
{
  const Librashader::Availability availability =
      Librashader::LoadFromPath("/nonexistent/librashader-does-not-exist.dylib");
  EXPECT_FALSE(availability.available);
  // The reason is what a user sees in the log when post-processing silently degrades, so it must
  // not be empty and must name the path that was tried.
  EXPECT_FALSE(availability.reason.empty());
  EXPECT_NE(availability.reason.find("librashader-does-not-exist"), std::string::npos);
}

TEST(LibrashaderLoader, LibraryPathMatchesHowThePlatformPackagesIt)
{
  const std::string path = Librashader::LibraryPath();
  ASSERT_FALSE(path.empty());
  // Absolute where the platform's own search order cannot find the packaged library, bare where it
  // can. "Always absolute" is the wrong contract: on Android the library is in the APK's lib
  // directory, which only the dynamic linker knows how to locate.
#if defined(ANDROID)
  EXPECT_EQ(path, "librashader.so");
#elif defined(_WIN32)
  // Next to the executable. A bare name would be resolved against the process search path, which
  // need not contain the install directory.
  EXPECT_EQ(path.substr(1, 2), ":\\") << "path was: " << path;
#elif defined(__APPLE__)
  // Inside the .app bundle, where dlopen's default search never looks -- the same treatment
  // VulkanLoader gives libMoltenVK.
  EXPECT_EQ(path.front(), '/') << "path was: " << path;
  EXPECT_NE(path.find("/Contents/Frameworks/librashader.dylib"), std::string::npos)
      << "path was: " << path;
#else
  EXPECT_EQ(path, "librashader.so");
#endif
}

TEST(LibrashaderLoader, DescribeAndFreeErrorAcceptsNull)
{
  EXPECT_TRUE(Librashader::DescribeAndFreeError(nullptr).empty());
}

TEST(LibrashaderLoader, GetSymbolIsNullForAnAbsentName)
{
  // Safe whether or not the vendored library is present in the test environment: no library
  // exports this name.
  EXPECT_EQ(Librashader::GetSymbol("libra_this_symbol_does_not_exist"), nullptr);
}
