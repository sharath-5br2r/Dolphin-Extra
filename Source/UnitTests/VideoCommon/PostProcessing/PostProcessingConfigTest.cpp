// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/PostProcessingConfig.h"

using namespace VideoCommon;

TEST(PostProcessingConfig, LegacyChainResolvesToItsFirstEntry)
{
  // Configs written before the chain feature was removed hold a ';'-separated list. The
  // librashader path already used only the first entry; now every path does, and out loud.
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp;b.slangp"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(""), "");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(";b.slangp"), "");
}

TEST(PostProcessingConfig, TrimsWhitespaceFromFirstEntry)
{
  // A hand-edited GFX.ini may be spaced out.
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(" a.slangp "), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("\ta.slangp\t"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(" a.slangp ;b.slangp"), "a.slangp");
}

TEST(PostProcessingConfig, EmptyTailResolvesLikeNoTailAtAll)
{
  // A trailing separator, with nothing but whitespace or further separators behind it, must resolve
  // to the same preset as the bare name -- the parse must not mistake the empty tail for a second
  // entry, nor let it disturb the trimming of the first.
  //
  // This was called NoWarningForEmptyTail, which it never checked: the assertions below observe the
  // return value only. Suppressing the warning is the dropped.clear() at
  // PostProcessingConfig.cpp:20-21, and observing it would mean initialising LogManager and
  // registering a listener -- process-global state that no other test in this suite touches, and
  // which would have to be torn down exactly right to avoid leaking into whatever runs next under
  // --gtest_shuffle. Not worth it for one warning; the emission is recorded as a coverage gap in
  // the design doc instead.
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp;"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp; "), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp;\t"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp;;"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(" a.slangp ; ;"), "a.slangp");
}
