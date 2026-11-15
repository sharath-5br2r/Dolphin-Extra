// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Core/State.h"

TEST(ContinuationStateTest, ContinuationNamesCannotAliasNumberedSlotsOrEscapeSaveDirectory)
{
  EXPECT_EQ(State::GetContinuationPath("GMSE01"),
            File::GetUserPath(D_STATESAVES_IDX) + "GMSE01.continue.sav");
  EXPECT_TRUE(State::GetContinuationPath("").empty());
  EXPECT_TRUE(State::GetContinuationPath("../GMSE01").empty());
  EXPECT_EQ(State::GetContinuationPath("ID-homebrew 1.0"),
            File::GetUserPath(D_STATESAVES_IDX) + "ID-homebrew 1.0.continue.sav");
  EXPECT_NE(State::GetContinuationPath("GMSE01.s01"),
            File::GetUserPath(D_STATESAVES_IDX) + "GMSE01.s01");
  EXPECT_TRUE(State::GetContinuationPath("C:\\GMSE01").empty());
  EXPECT_NE(State::GetContinuationPath("GMSE01"), State::GetContinuationPath("GMSP01"));
}

TEST(ContinuationStateTest, LibrarySlotPathsMatchNumberedSlotsAndRejectUnsafeGameIDs)
{
  EXPECT_EQ(State::GetSlotPath("GMSE01", 3), File::GetUserPath(D_STATESAVES_IDX) + "GMSE01.s03");
  EXPECT_EQ(State::GetSlotPath("GMSE01", 10), File::GetUserPath(D_STATESAVES_IDX) + "GMSE01.s10");
  EXPECT_TRUE(State::GetSlotPath("", 1).empty());
  EXPECT_TRUE(State::GetSlotPath("../GMSE01", 1).empty());
  EXPECT_TRUE(State::GetSlotPath("C:\\GMSE01", 1).empty());
  EXPECT_FALSE(State::GetSlotInfo("../GMSE01", 1).exists);
  // Executable IDs are made from file names; slots must accept what Save and Load accept.
  EXPECT_EQ(State::GetSlotPath("ID-demo:test", 1),
            File::GetUserPath(D_STATESAVES_IDX) + "ID-demo:test.s01");
}

TEST(ContinuationStateTest, DeletingContinuationLeavesNumberedAndUndoStatesUntouched)
{
  const auto directory = File::CreateTempDir();
  ASSERT_FALSE(directory.empty());
  const auto continuation = directory + "/GMSE01.continue.sav";
  for (const auto& path : {continuation, continuation + ".dtm", continuation + ".preview",
                           directory + "/GMSE01.s01", directory + "/lastState.sav"})
  {
    File::IOFile file(path, "wb");
    ASSERT_TRUE(file.WriteString("existing save"));
  }
  EXPECT_TRUE(State::DeleteContinuation(continuation));
  EXPECT_FALSE(File::Exists(continuation));
  EXPECT_FALSE(File::Exists(continuation + ".dtm"));
  EXPECT_FALSE(File::Exists(continuation + ".preview"));
  EXPECT_TRUE(File::Exists(directory + "/GMSE01.s01"));
  EXPECT_TRUE(File::Exists(directory + "/lastState.sav"));
  EXPECT_TRUE(State::DeleteContinuation(continuation));
  File::DeleteDirRecursively(directory);
}
