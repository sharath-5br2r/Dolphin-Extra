// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Pure graph-analysis helpers that decide which render-stage features a .slangp pass chain needs:
// feedback double-buffering (<Alias>Feedback), original frame history (OriginalHistoryN), and
// mipmap generation for mipmap_input passes. These are GPU-free and unit-testable.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/PassGraph.h"

using namespace VideoCommon;

TEST(PassGraph, FeedbackAliasesDetectsSelfFeedback)
{
  // Pass "AfterglowPass" reads its own previous-frame output as "AfterglowPassFeedback".
  const std::vector<std::vector<std::string>> sampler_names = {
      {"Source"},
      {"Source", "OriginalHistory0", "AfterglowPassFeedback"},
      {"Source", "AfterglowPass"},
  };
  const auto feedback = ComputeFeedbackAliases(sampler_names);
  EXPECT_EQ(feedback.size(), 1u);
  EXPECT_TRUE(feedback.count("AfterglowPass"));
  // The plain alias reference must NOT be treated as feedback.
  EXPECT_FALSE(feedback.count("AfterglowPassFeedback"));
}

TEST(PassGraph, FeedbackAliasesEmptyWhenNoFeedbackReferenced)
{
  const std::vector<std::vector<std::string>> sampler_names = {
      {"Source"},
      {"Source", "StockPass"},
  };
  EXPECT_TRUE(ComputeFeedbackAliases(sampler_names).empty());
}

TEST(PassGraph, FeedbackAliasesMultipleDistinct)
{
  const std::vector<std::vector<std::string>> sampler_names = {
      {"Source", "AfterglowPassFeedback"},
      {"Source", "AvgLumPassFeedback"},
      {"Source", "AvgLumPassFeedback"},  // duplicate reference collapses to one
  };
  const auto feedback = ComputeFeedbackAliases(sampler_names);
  EXPECT_EQ(feedback.size(), 2u);
  EXPECT_TRUE(feedback.count("AfterglowPass"));
  EXPECT_TRUE(feedback.count("AvgLumPass"));
}

TEST(PassGraph, FeedbackAliasesIgnoresBareFeedbackWord)
{
  // "Feedback" with no base name is not a valid feedback reference.
  const std::vector<std::vector<std::string>> sampler_names = {{"Feedback"}};
  EXPECT_TRUE(ComputeFeedbackAliases(sampler_names).empty());
}

TEST(PassGraph, ParseHistoryIndex)
{
  EXPECT_EQ(ParseOriginalHistoryIndex("OriginalHistory0"), 0u);
  EXPECT_EQ(ParseOriginalHistoryIndex("OriginalHistory1"), 1u);
  EXPECT_EQ(ParseOriginalHistoryIndex("OriginalHistory7"), 7u);
  EXPECT_FALSE(ParseOriginalHistoryIndex("Original").has_value());
  EXPECT_FALSE(ParseOriginalHistoryIndex("OriginalHistory").has_value());
  EXPECT_FALSE(ParseOriginalHistoryIndex("OriginalHistoryX").has_value());
  EXPECT_FALSE(ParseOriginalHistoryIndex("Source").has_value());
}

TEST(PassGraph, MaxHistoryIndexIgnoresHistory0)
{
  // OriginalHistory0 == Original (current frame), so it needs no history buffer.
  const std::vector<std::vector<std::string>> only_zero = {{"Source", "OriginalHistory0"}};
  EXPECT_EQ(ComputeMaxHistoryIndex(only_zero), 0u);

  const std::vector<std::vector<std::string>> deep = {
      {"Source", "OriginalHistory0"},
      {"OriginalHistory2", "OriginalHistory1"},
  };
  EXPECT_EQ(ComputeMaxHistoryIndex(deep), 2u);

  const std::vector<std::vector<std::string>> none = {{"Source", "StockPass"}};
  EXPECT_EQ(ComputeMaxHistoryIndex(none), 0u);
}

TEST(PassGraph, MipmapSourcePassesMarksPrecedingPass)
{
  // Passes 3 and 4 declare mipmap_input; their Source is the output of passes 2 and 3.
  const std::vector<bool> mipmap_input = {false, false, false, true, true};
  const auto sources = ComputeMipmapSourcePasses(mipmap_input);
  EXPECT_EQ(sources.size(), 2u);
  EXPECT_TRUE(sources.count(2));
  EXPECT_TRUE(sources.count(3));
}

TEST(PassGraph, MipmapSourcePassesIgnoresPassZeroInput)
{
  // Pass 0's Source is the game frame we don't own, so mipmap_input on pass 0 marks nothing.
  const std::vector<bool> mipmap_input = {true, false};
  EXPECT_TRUE(ComputeMipmapSourcePasses(mipmap_input).empty());
}

TEST(PassGraph, MipmapSourcePassesEmptyWhenNone)
{
  const std::vector<bool> mipmap_input = {false, false, false};
  EXPECT_TRUE(ComputeMipmapSourcePasses(mipmap_input).empty());
}
