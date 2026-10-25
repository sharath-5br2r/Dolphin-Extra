// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <vector>

#include <OptionParser.h>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "UICommon/CommandLineParse.h"

TEST(BigPictureCommandLineTest, GuiEntryCanLaunchWithoutAGame)
{
  Config::Init();
  auto parser = CommandLineParse::CreateParser(CommandLineParse::ParserOptions::IncludeGUIOptions);
  const auto& values =
      CommandLineParse::ParseArguments(parser.get(), std::vector<std::string>{"--big-picture"});
  EXPECT_TRUE(values.is_set("big_picture"));
  EXPECT_FALSE(values.is_set("exec"));
  EXPECT_FALSE(values.is_set("batch"));
  Config::Shutdown();
}

TEST(BigPictureCommandLineTest, BatchConflictIsRejectedBeforeCreatingAUserInterface)
{
  EXPECT_EXIT(
      {
        auto parser =
            CommandLineParse::CreateParser(CommandLineParse::ParserOptions::IncludeGUIOptions);
        try
        {
          CommandLineParse::ParseArguments(
              parser.get(),
              std::vector<std::string>{"--big-picture", "--batch", "--exec", "game.iso"});
        }
        catch (int status)
        {
          std::exit(status);
        }
        std::exit(99);
      },
      testing::ExitedWithCode(2), "--big-picture cannot be combined with --batch");
}

TEST(BigPictureCommandLineTest, NoGuiFrontendDoesNotAcceptTheGuiEntryOption)
{
  EXPECT_EXIT(
      {
        auto parser =
            CommandLineParse::CreateParser(CommandLineParse::ParserOptions::OmitGUIOptions);
        try
        {
          CommandLineParse::ParseArguments(parser.get(), std::vector<std::string>{"--big-picture"});
        }
        catch (int status)
        {
          std::exit(status);
        }
        std::exit(99);
      },
      testing::ExitedWithCode(2), "--big-picture");
}
