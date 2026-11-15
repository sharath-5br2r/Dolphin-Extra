// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include "UICommon/BigPicture/Controller.h"

namespace
{
using namespace BigPicture;

class FakeHost final : public Host
{
public:
  HostState ReadState() const override { return state; }
  Result Execute(const Command& command) override
  {
    executed.push_back(command);
    if (execute)
      return execute(command);
    return {command, ResultStatus::Dispatched};
  }
  HostState state;
  std::vector<Command> executed;
  std::function<Result(const Command&)> execute;
};

class BigPictureControllerTest : public testing::Test
{
protected:
  Command Make(Action action, int slot = 0, std::string path = {})
  {
    Command command{controller.GetSnapshot()->generation, ++id, action, std::move(path), slot};
    if (action == Action::Confirm && controller.GetSnapshot()->confirmation)
      command.confirmation_id = controller.GetSnapshot()->confirmation->id;
    return command;
  }
  void Send(Action action, int slot = 0, std::string path = {})
  {
    controller.Submit(Make(action, slot, std::move(path)));
  }
  void InGame(Lifecycle lifecycle = Lifecycle::Running)
  {
    host.state.lifecycle = lifecycle;
    controller.Refresh();
    Send(Action::OpenMenu);
  }
  void GroupedLibrary()
  {
    Send(Action::OpenMenu);
    Send(Action::ShowLibrary);
    auto library = std::make_shared<LibrarySnapshot>();
    LibraryGame game;
    game.path = "disc1.iso";
    game.discs = {{"disc1.iso", "Disc 1"}, {"disc2.iso", "Disc 2"}};
    library->games.push_back(game);
    host.state.library = library;
    host.execute = [this](const Command& command) {
      if (command.action == Action::Library &&
          command.library_operation == LibraryOperation::ChooseBootDisc)
      {
        auto picker = std::make_shared<LibrarySnapshot>(*host.state.library);
        picker->files = picker->games.front().discs;
        picker->picker_boot_disc = true;
        ++picker->picker_revision;
        host.state.library = picker;
        return Result{command, ResultStatus::Succeeded};
      }
      host.state.lifecycle = Lifecycle::Booting;
      return Result{command, ResultStatus::Dispatched};
    };
    controller.Refresh();
  }
  void GameLibrary()
  {
    Send(Action::OpenMenu);
    Send(Action::ShowLibrary);
    auto library = std::make_shared<LibrarySnapshot>();
    LibraryGame game;
    game.path = "game.iso";
    game.game_id = "GTEST1";
    library->games.push_back(game);
    host.state.library = library;
    host.execute = [this](const Command& command) {
      if (command.action == Action::BootState)
      {
        host.state.lifecycle = Lifecycle::Booting;
        return Result{command, ResultStatus::Dispatched};
      }
      return Result{command, ResultStatus::Succeeded};
    };
    controller.Refresh();
  }
  FakeHost host;
  Controller controller{host};
  std::uint64_t id = 0;
};

TEST_F(BigPictureControllerTest, GameFoldersOpenFromOptionsAndConfirmRemoval)
{
  Send(Action::OpenMenu);
  Send(Action::ShowLibrary);
  host.state.library = std::make_shared<LibrarySnapshot>();
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Succeeded}; };
  controller.Refresh();
  const auto library = [&](LibraryOperation operation, std::string path = {}) {
    auto command = Make(Action::Library, 0, std::move(path));
    command.library_operation = operation;
    controller.Submit(command);
  };
  // Removal is only offered from the folder list.
  library(LibraryOperation::RemoveFolder, "C:/Games");
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  library(LibraryOperation::Options);
  library(LibraryOperation::Folders);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameFolders);
  library(LibraryOperation::Recursive);
  library(LibraryOperation::Refresh);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameFolders);
  const auto executed = host.executed.size();

  library(LibraryOperation::RemoveFolder, "C:/Games");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Confirmation);
  EXPECT_EQ(controller.GetSnapshot()->confirmation_default, Action::Cancel);
  EXPECT_EQ(host.executed.size(), executed);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameFolders);
  EXPECT_EQ(host.executed.size(), executed);
  library(LibraryOperation::RemoveFolder, "C:/Games");
  Send(Action::Confirm);
  ASSERT_EQ(host.executed.size(), executed + 1);
  EXPECT_EQ(host.executed.back().library_operation, LibraryOperation::RemoveFolder);
  EXPECT_EQ(host.executed.back().path, "C:/Games");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameFolders);

  // Adding a folder returns to the list once it is chosen.
  library(LibraryOperation::AddFolder);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  library(LibraryOperation::ChooseFolder, "C:/Added");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameFolders);
}

TEST_F(BigPictureControllerTest, GameOptionsTargetOneLibraryGameAndRouteItsScreens)
{
  GameLibrary();
  Send(Action::GameOptions, 0, "missing.iso");
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Library);
  Send(Action::GameOptions, 0, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameOptions);
  EXPECT_EQ(controller.GetSnapshot()->options_path, "game.iso");
  EXPECT_TRUE(host.executed.empty());

  // Every action from the options screen must name the game it was opened for.
  Send(Action::GameProperties, 0, "missing.iso");
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  EXPECT_TRUE(host.executed.empty());
  Send(Action::GameProperties, 0, "game.iso");
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Settings);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameOptions);

  Send(Action::GameStates, 0, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::LoadStates);
  const auto depth = controller.GetSnapshot()->screens.size();
  Send(Action::GameStates, 0, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->screens.size(), depth);
  Send(Action::BootState, 0, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  Send(Action::BootState, 3, "game.iso");
  EXPECT_EQ(host.executed.back().action, Action::BootState);
  EXPECT_EQ(host.executed.back().slot, 3);
  ASSERT_TRUE(controller.GetSnapshot()->pending);

  // Like Boot, a state boot completes when the game runs.
  host.state.lifecycle = Lifecycle::Running;
  controller.Refresh();
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Desktop);
}

TEST_F(BigPictureControllerTest, GameStatesFollowLoadingPolicyAndPlayTimeResetIsConfirmed)
{
  GameLibrary();
  Send(Action::GameOptions, 0, "game.iso");
  host.state.hardcore = true;
  controller.Refresh();
  Send(Action::GameStates, 0, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::Hardcore);
  EXPECT_TRUE(host.executed.empty());
  host.state.hardcore = false;
  controller.Refresh();

  Send(Action::ResetPlayTime, 0, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Confirmation);
  EXPECT_EQ(controller.GetSnapshot()->confirmation_default, Action::Cancel);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameOptions);
  EXPECT_TRUE(host.executed.empty());
  Send(Action::ResetPlayTime, 0, "game.iso");
  Send(Action::Confirm);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed.back().action, Action::ResetPlayTime);
  EXPECT_EQ(host.executed.back().path, "game.iso");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::GameOptions);

  // Game options are a library feature; a running game has its own menu.
  Send(Action::Back);
  host.state.lifecycle = Lifecycle::Running;
  controller.Refresh();
  EXPECT_NE(controller.GetSnapshot()->availability[static_cast<std::size_t>(Action::GameOptions)],
            UnavailableReason::None);
}

TEST_F(BigPictureControllerTest, AchievementsRequireAnActiveLoginAndReturnToPause)
{
  InGame();
  Send(Action::Achievements);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::LoginRequired);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
  EXPECT_TRUE(host.executed.empty());
  host.state.achievements_logged_in = true;
  host.state.hardcore = true;
  Send(Action::Achievements);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Achievements);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
  host.state.achievements_logged_in = false;
  Send(Action::Achievements);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::LoginRequired);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
}

TEST_F(BigPictureControllerTest, StopContinuationChoiceIsBoundToConfirmationAndRechecksSupport)
{
  InGame();
  host.state.can_save_continuation = true;
  Send(Action::Stop);
  const auto confirmation = controller.GetSnapshot()->confirmation->id;
  auto stale = Make(Action::Confirm, 1);
  stale.confirmation_id = confirmation + 1;
  controller.Submit(stale);
  EXPECT_TRUE(host.executed.empty());
  Send(Action::Confirm, -1);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  EXPECT_TRUE(host.executed.empty());
  host.state.can_save_continuation = false;
  Send(Action::Confirm, 1);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_TRUE(controller.GetSnapshot()->confirmation);
  host.state.can_save_continuation = true;
  Send(Action::Confirm, 1);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed.back().action, Action::Stop);
  EXPECT_EQ(host.executed.back().slot, 1);
  EXPECT_EQ(host.executed.back().confirmation_id, confirmation);
}

TEST_F(BigPictureControllerTest, ExitWithoutContinuationRemainsAvailableWhenSavingIsUnavailable)
{
  InGame();
  Send(Action::Exit);
  Send(Action::Confirm);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed.back().action, Action::Exit);
  EXPECT_EQ(host.executed.back().slot, 0);
}

TEST_F(BigPictureControllerTest, ContinuationPromptIsModalAndRechecksLoadingPolicy)
{
  host.state.continuation_pending = true;
  host.state.can_load_continuation = true;
  controller.Refresh();
  ASSERT_EQ(controller.GetSnapshot()->screens.back(), Screen::Continuation);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Library);
  Send(Action::Boot, 0, "another.iso");
  EXPECT_TRUE(host.executed.empty());
  host.state.can_load_continuation = false;
  Send(Action::ContinueGame);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::Hardcore);
  EXPECT_TRUE(host.executed.empty());
  host.execute = [&](const Command& command) {
    host.state.continuation_pending = false;
    host.state.lifecycle = Lifecycle::Booting;
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::FreshBoot);
  EXPECT_EQ(host.executed.back().action, Action::FreshBoot);
  controller.Refresh();
  EXPECT_TRUE(controller.GetSnapshot()->pending);
  host.state.lifecycle = Lifecycle::Running;
  controller.Refresh();
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
}

TEST_F(BigPictureControllerTest, DeferredBootShowsContinuationChoiceWithoutReportingBootFailure)
{
  Send(Action::OpenMenu);
  host.execute = [&](const Command& command) {
    host.state.continuation_pending = true;
    host.state.can_load_continuation = true;
    return Result{command, ResultStatus::Succeeded};
  };
  Send(Action::Boot, 0, "game.iso");
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  controller.Refresh();
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Continuation);
  EXPECT_FALSE(controller.GetSnapshot()->error);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
}

TEST_F(BigPictureControllerTest, FailedContinuationDeletionCanBeDismissedAndRetried)
{
  host.state.continuation_pending = true;
  controller.Refresh();
  host.execute = [](const Command& command) {
    return Result{command, ResultStatus::Failed, UnavailableReason::None, "Read only"};
  };
  Send(Action::DeleteContinuation);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Error);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Continuation);
  EXPECT_TRUE(host.state.continuation_pending);
  host.execute = [&](const Command& command) {
    host.state.continuation_pending = false;
    return Result{command, ResultStatus::Cancelled};
  };
  Send(Action::Back);
  controller.Refresh();
  EXPECT_FALSE(host.state.continuation_pending);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Home);
}

TEST_F(BigPictureControllerTest, OpeningAndNestedBackPreserveGuestPauseState)
{
  InGame(Lifecycle::Paused);
  Send(Action::Settings);
  Send(Action::SaveStates);
  EXPECT_EQ(controller.GetSnapshot()->screens.size(), 3u);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Settings);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Desktop);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_EQ(host.state.lifecycle, Lifecycle::Paused);
}

TEST_F(BigPictureControllerTest, PublishedSnapshotsRemainImmutable)
{
  const auto old = controller.GetSnapshot();
  Send(Action::OpenMenu);
  EXPECT_EQ(old->mode, Mode::Desktop);
  EXPECT_TRUE(old->screens.empty());
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Library);
}

TEST_F(BigPictureControllerTest, PerGameSettingsCanOnlyOpenForALaunchedGame)
{
  Send(Action::OpenMenu);
  Send(Action::Settings, 1);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Home);
  Send(Action::Settings);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Settings);
  Send(Action::Back);
  host.state.lifecycle = Lifecycle::Paused;
  controller.Refresh();
  Send(Action::OpenMenu);
  Send(Action::Settings, 2);
  EXPECT_NE(controller.GetSnapshot()->screens.back(), Screen::Settings);
  Send(Action::Settings, 1);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Settings);
}

TEST_F(BigPictureControllerTest, CancelAndBackNeverExecuteDestructiveActions)
{
  InGame();
  for (const auto cancel : {Action::Cancel, Action::Back})
  {
    Send(Action::Stop);
    ASSERT_TRUE(controller.GetSnapshot()->confirmation);
    EXPECT_EQ(controller.GetSnapshot()->confirmation_default, Action::Cancel);
    Send(cancel);
    EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Cancelled);
    EXPECT_FALSE(controller.GetSnapshot()->confirmation);
  }
  EXPECT_TRUE(host.executed.empty());
}

TEST_F(BigPictureControllerTest, ConfirmationIsModalAndRechecksPolicy)
{
  InGame();
  Send(Action::Reset);
  Send(Action::SaveState, 1);
  EXPECT_TRUE(host.executed.empty());
  host.state.netplay = true;
  Send(Action::Confirm);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::NetPlay);
  EXPECT_FALSE(controller.GetSnapshot()->confirmation);
}

TEST_F(BigPictureControllerTest, FullscreenPreservesExitConfirmationAndItsApprovalToken)
{
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Succeeded}; };
  Send(Action::OpenMenu);
  Send(Action::ExitMenu);
  Send(Action::Exit);
  const auto before = controller.GetSnapshot();
  ASSERT_TRUE(before->confirmation);
  EXPECT_EQ(before->availability[static_cast<std::size_t>(Action::Fullscreen)],
            UnavailableReason::None);
  Send(Action::Fullscreen);
  const auto after = controller.GetSnapshot();
  ASSERT_TRUE(after->confirmation);
  EXPECT_EQ(after->confirmation->id, before->confirmation->id);
  EXPECT_EQ(after->confirmation->action, Action::Exit);
  EXPECT_EQ(after->screens, before->screens);
  EXPECT_EQ(after->generation, before->generation);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed.back().action, Action::Fullscreen);
  Send(Action::Confirm);
  ASSERT_EQ(host.executed.size(), 2u);
  EXPECT_EQ(host.executed.back().action, Action::Exit);
}

TEST_F(BigPictureControllerTest, FullscreenPreservesErrorsAndTransitionRestrictions)
{
  InGame();
  host.execute = [](const Command& command) {
    return Result{command, command.action == Action::Fullscreen ? ResultStatus::Succeeded :
                                                                  ResultStatus::Failed};
  };
  Send(Action::Reset);
  Send(Action::Confirm);
  const auto before = controller.GetSnapshot();
  ASSERT_TRUE(before->error);
  Send(Action::Fullscreen);
  EXPECT_EQ(controller.GetSnapshot()->screens, before->screens);
  ASSERT_TRUE(controller.GetSnapshot()->error);
  EXPECT_EQ(controller.GetSnapshot()->error->command.id, before->error->command.id);
  EXPECT_EQ(host.executed.back().action, Action::Fullscreen);
  host.state.lifecycle = Lifecycle::Stopping;
  Send(Action::Fullscreen);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::Busy);
}

TEST_F(BigPictureControllerTest, CommandsAndConfirmationsCannotCrossSessions)
{
  InGame();
  Send(Action::Reset);
  const auto confirm = Make(Action::Confirm);
  controller.InvalidateSession();
  controller.Submit(confirm);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_FALSE(controller.GetSnapshot()->confirmation);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
}

TEST_F(BigPictureControllerTest, DuplicateCommandIsNotExecutedTwice)
{
  InGame();
  const auto save = Make(Action::SaveState, 1);
  controller.Submit(save);
  controller.Submit(save);
  EXPECT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
}

TEST_F(BigPictureControllerTest, BootWaitsForCompletionAndRejectsQueuedOldCommands)
{
  Send(Action::OpenMenu);
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Booting;
    return Result{command, ResultStatus::Dispatched};
  };
  const auto boot = Make(Action::Boot, 0, "game.iso");
  const auto old_save = Make(Action::SaveState, 1);
  controller.Submit(boot);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Pending);
  controller.Submit(old_save);
  EXPECT_EQ(host.executed.size(), 1u);
  host.state.lifecycle = Lifecycle::Running;
  controller.Refresh();
  ASSERT_TRUE(controller.GetSnapshot()->result);
  EXPECT_EQ(controller.GetSnapshot()->result->command.id, boot.id);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Desktop);
}

TEST_F(BigPictureControllerTest, AsynchronousBootFailureReturnsAnErrorToLibrary)
{
  Send(Action::OpenMenu);
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Booting;
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::Boot, 0, "game.iso");
  host.state.lifecycle = Lifecycle::Idle;
  controller.Refresh();
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Failed);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Library);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Error);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Home);
}

TEST_F(BigPictureControllerTest, GracefulStopRemainsPendingUntilActualStop)
{
  InGame();
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Stopping;
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::Stop);
  Send(Action::Confirm);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Pending);
  Send(Action::Boot, 0, "next.iso");
  EXPECT_EQ(host.executed.size(), 1u);
  host.state.lifecycle = Lifecycle::Idle;
  controller.Refresh();
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Library);
}

TEST_F(BigPictureControllerTest, ExternalStopDuringBootInvalidatesPendingWork)
{
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Booting;
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::Boot, 0, "game.iso");
  host.state.lifecycle = Lifecycle::Stopping;
  controller.Refresh();
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_FALSE(controller.GetSnapshot()->pending);
}

TEST_F(BigPictureControllerTest, FailedHostActionPreservesMessageAndCanBeDismissed)
{
  InGame();
  host.execute = [](const Command& command) {
    return Result{command, ResultStatus::Failed, UnavailableReason::None, "Disk full"};
  };
  Send(Action::SaveState, 1);
  EXPECT_EQ(controller.GetSnapshot()->result->message, "Disk full");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Error);
  Send(Action::Reset);
  ASSERT_TRUE(controller.GetSnapshot()->error);
  EXPECT_EQ(controller.GetSnapshot()->error->message, "Disk full");
  EXPECT_EQ(host.executed.size(), 1u);
  Send(Action::Back);
  EXPECT_FALSE(controller.GetSnapshot()->error);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
}

TEST_F(BigPictureControllerTest, StateOperationsStayPendingUntilTheCoreReportsTheirResult)
{
  InGame();
  Send(Action::SaveState, 10);
  ASSERT_TRUE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Pending);
  const auto save = host.executed.back().id;
  Send(Action::LoadState, 1);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::Busy);

  // Only the pending command can be completed.
  controller.Complete(save + 100, {{}, ResultStatus::Failed});
  ASSERT_TRUE(controller.GetSnapshot()->pending);
  controller.Complete(save, {{}, ResultStatus::Succeeded});
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
  EXPECT_EQ(controller.GetSnapshot()->result->command.slot, 10);

  // A failure is shown on the error screen with the host's message.
  Send(Action::LoadState, 1);
  controller.Complete(host.executed.back().id,
                      {{}, ResultStatus::Failed, UnavailableReason::None, "Unreadable"});
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Error);
  EXPECT_EQ(controller.GetSnapshot()->error->message, "Unreadable");
  EXPECT_EQ(controller.GetSnapshot()->error->command.action, Action::LoadState);
}

TEST_F(BigPictureControllerTest, InvalidSlotsPathsAndUnsupportedActionsNeverReachHost)
{
  Send(Action::Boot);
  InGame();
  for (int slot : {-1, 0, 11})
  {
    Send(Action::SaveState, slot);
    Send(Action::LoadState, slot);
  }
  Send(Action::Count);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
}

TEST_F(BigPictureControllerTest, HardcoreAndNetplayRestrictionsAreSharedWithSnapshot)
{
  InGame();
  host.state.hardcore = true;
  controller.Refresh();
  EXPECT_EQ(controller.GetSnapshot()->availability[static_cast<std::size_t>(Action::LoadState)],
            UnavailableReason::Hardcore);
  Send(Action::LoadState, 1);
  EXPECT_TRUE(host.executed.empty());
  host.state.netplay = true;
  Send(Action::Resume);
  Send(Action::Reset);
  EXPECT_TRUE(host.executed.empty());
}

TEST_F(BigPictureControllerTest, ImmediateBootFailureInvalidatesQueuedCommands)
{
  Send(Action::OpenMenu);
  host.execute = [](const Command& command) {
    return Result{command, ResultStatus::Failed, UnavailableReason::None, "Boot failed"};
  };
  const auto boot = Make(Action::Boot, 0, "bad.iso");
  const auto queued_boot = Make(Action::Boot, 0, "other.iso");
  controller.Submit(boot);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Failed);
  controller.Submit(queued_boot);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_EQ(host.executed.size(), 1u);
}

TEST_F(BigPictureControllerTest, HostCancellationAndReentrantCommandsDoNotDispatchTwice)
{
  InGame();
  host.execute = [this](const Command& command) {
    Send(Action::SaveState, 1);
    controller.Refresh();
    return Result{command, ResultStatus::Cancelled};
  };
  Send(Action::Stop);
  Send(Action::Confirm);
  EXPECT_EQ(host.executed.size(), 1u);
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_FALSE(controller.GetSnapshot()->confirmation);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Cancelled);
}

TEST_F(BigPictureControllerTest, ExplicitResumeClosesMenuOnlyAfterHostSuccess)
{
  InGame(Lifecycle::Paused);
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Running;
    return Result{command, ResultStatus::Succeeded};
  };
  Send(Action::Resume);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Desktop);
  EXPECT_TRUE(controller.GetSnapshot()->screens.empty());
  EXPECT_EQ(host.executed.size(), 1u);
}

TEST_F(BigPictureControllerTest, ReentrantStopCompletionStillInvalidatesTheOldSession)
{
  InGame();
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Idle;
    controller.Refresh();
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::Stop);
  const auto confirm = Make(Action::Confirm);
  const auto old_boot = Make(Action::Boot, 0, "old.iso");
  controller.Submit(confirm);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
  controller.Submit(old_boot);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_EQ(host.executed.size(), 1u);
}

TEST_F(BigPictureControllerTest, OldDialogCannotConfirmANewerDestructiveRequest)
{
  InGame();
  Send(Action::Reset);
  const auto old_request_id = controller.GetSnapshot()->confirmation->id;
  Send(Action::Cancel);
  Send(Action::Stop);
  auto confirm = Make(Action::Confirm);
  confirm.confirmation_id = old_request_id;
  controller.Submit(confirm);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  ASSERT_TRUE(controller.GetSnapshot()->confirmation);
  EXPECT_EQ(controller.GetSnapshot()->confirmation->action, Action::Stop);
}
TEST_F(BigPictureControllerTest, OccupiedSlotsRequireMatchingCancelableOverwriteConfirmation)
{
  InGame();
  host.state.state_slots_ready = true;
  host.state.state_slots[9].occupied = true;
  Send(Action::SaveStates);
  Send(Action::SaveState, 10);
  ASSERT_TRUE(controller.GetSnapshot()->confirmation);
  EXPECT_EQ(controller.GetSnapshot()->confirmation->slot, 10);
  EXPECT_TRUE(host.executed.empty());
  Send(Action::Cancel);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::SaveStates);
  Send(Action::SaveState, 10);
  Send(Action::Confirm);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed[0].slot, 10);
  EXPECT_EQ(host.executed[0].confirmation_id, host.executed[0].id);
  controller.Complete(host.executed[0].id, {{}, ResultStatus::Succeeded});
  Send(Action::ReturnToDesktop);
  Send(Action::SaveState, 10);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  EXPECT_FALSE(controller.GetSnapshot()->confirmation);
  EXPECT_EQ(host.executed.size(), 1u);
}

TEST_F(BigPictureControllerTest, ExitWaitsForGracefulStopAndRechecksDesktopOnlyPolicy)
{
  InGame();
  Send(Action::Exit);
  host.state.stop_requires_desktop = true;
  Send(Action::Confirm);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::DesktopRequired);
  host.state.stop_requires_desktop = false;
  host.execute = [this](const Command& command) {
    host.state.lifecycle = Lifecycle::Stopping;
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::Exit);
  Send(Action::Confirm);
  ASSERT_TRUE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->pending->action, Action::Exit);
  host.state.lifecycle = Lifecycle::Idle;
  controller.Refresh();
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
}

TEST_F(BigPictureControllerTest, DiscAndScreenshotPoliciesAreRecheckedWhenDispatched)
{
  InGame();
  Send(Action::ChangeDisc);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::DiscUnavailable);
  host.state.can_change_disc = true;
  host.state.netplay = true;
  Send(Action::ChangeDisc);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::NetPlay);
  Send(Action::Screenshot);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed[0].action, Action::Screenshot);
  host.state.lifecycle = Lifecycle::Stopping;
  Send(Action::Screenshot);
  EXPECT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::Busy);
}
TEST_F(BigPictureControllerTest, LibrarySetupAndKeyboardPreserveNestedBackAndRejectOutOfContextText)
{
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Succeeded}; };
  Send(Action::OpenMenu);
  Send(Action::ShowLibrary);
  const auto send = [&](LibraryOperation operation) {
    auto command = Make(Action::Library);
    command.library_operation = operation;
    controller.Submit(command);
  };
  send(LibraryOperation::AddFolder);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  send(LibraryOperation::EditPath);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::TextInput);
  send(LibraryOperation::TextDone);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Library);
  const auto executions = host.executed.size();
  send(LibraryOperation::TextKey);
  EXPECT_EQ(host.executed.size(), executions);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  send(LibraryOperation::Options);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::LibraryOptions);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Library);
}

TEST_F(BigPictureControllerTest, GroupedBootOpensNavigablePickerAndBackCancelsWithoutBooting)
{
  GroupedLibrary();
  const auto generation = controller.GetSnapshot()->generation;
  Send(Action::Boot, 0, "disc1.iso");
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(host.executed.back().library_operation, LibraryOperation::ChooseBootDisc);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  EXPECT_EQ(controller.GetSnapshot()->generation, generation);
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Library);
  EXPECT_EQ(host.state.lifecycle, Lifecycle::Idle);
  EXPECT_EQ(host.executed.size(), 1u);
}

TEST_F(BigPictureControllerTest, GroupedPickerBootsTheChosenSecondDiscWithoutOpeningAnotherPicker)
{
  GroupedLibrary();
  Send(Action::Boot, 0, "disc1.iso");
  auto choice = Make(Action::Boot, 0, "disc2.iso");
  choice.library_revision = host.state.library->picker_revision;
  controller.Submit(choice);
  ASSERT_EQ(host.executed.size(), 2u);
  EXPECT_EQ(host.executed.back().action, Action::Boot);
  EXPECT_EQ(host.executed.back().path, "disc2.iso");
  ASSERT_TRUE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->pending->action, Action::Boot);
}

TEST_F(BigPictureControllerTest, GroupedPickerRejectsSelectionsAfterCancelAndFromAnOlderPicker)
{
  GroupedLibrary();
  Send(Action::Boot, 0, "disc1.iso");
  const auto revision = host.state.library->picker_revision;
  Send(Action::Cancel);
  auto cancelled = Make(Action::Boot, 0, "disc2.iso");
  cancelled.library_revision = revision;
  controller.Submit(cancelled);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_EQ(host.executed.size(), 1u);
  Send(Action::Boot, 0, "disc1.iso");
  ASSERT_EQ(host.executed.size(), 2u);
  auto old = Make(Action::Boot, 0, "disc2.iso");
  old.library_revision = revision;
  controller.Submit(old);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_EQ(host.executed.size(), 2u);
}

TEST_F(BigPictureControllerTest, GroupedPickerRestrictsChoicesAndRechecksBootPolicy)
{
  GroupedLibrary();
  Send(Action::Boot, 0, "disc1.iso");
  auto other = Make(Action::Boot, 0, "unrelated.iso");
  other.library_revision = host.state.library->picker_revision;
  controller.Submit(other);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  auto browse = Make(Action::Library);
  browse.library_operation = LibraryOperation::EnterDirectory;
  controller.Submit(browse);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  host.state.netplay = true;
  auto choice = Make(Action::Boot, 0, "disc2.iso");
  choice.library_revision = host.state.library->picker_revision;
  controller.Submit(choice);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::NetPlay);
  EXPECT_EQ(host.executed.size(), 1u);
}

TEST_F(BigPictureControllerTest, PickerBootFromAnObsoleteListingCannotExecute)
{
  Send(Action::OpenMenu);
  auto library = std::make_shared<LibrarySnapshot>();
  library->picker_revision = 8;
  host.state.library = library;
  auto command = Make(Action::Boot, 0, "game.iso");
  command.library_revision = 7;
  controller.Submit(command);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_TRUE(host.executed.empty());
}

TEST_F(BigPictureControllerTest, ControllerDiscPickerKeepsPauseAndRechecksNetPlay)
{
  InGame(Lifecycle::Paused);
  host.state.can_change_disc = true;
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Succeeded}; };
  auto open = Make(Action::Library);
  open.library_operation = LibraryOperation::OpenDisc;
  controller.Submit(open);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  EXPECT_EQ(host.state.lifecycle, Lifecycle::Paused);
  host.state.netplay = true;
  Send(Action::ChangeDisc, 0, "other.iso");
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::NetPlay);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  host.state.netplay = false;
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Dispatched}; };
  Send(Action::ChangeDisc, 0, "other.iso");
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Dispatched);
  EXPECT_EQ(host.state.lifecycle, Lifecycle::Paused);
}

TEST_F(BigPictureControllerTest, DiscChangeRequiresASelectedPathInsteadOfADesktopHandoff)
{
  InGame(Lifecycle::Paused);
  host.state.can_change_disc = true;
  Send(Action::ChangeDisc);
  EXPECT_EQ(controller.GetSnapshot()->result->reason, UnavailableReason::InvalidArgument);
  EXPECT_TRUE(host.executed.empty());
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
}

TEST_F(BigPictureControllerTest, SystemBootUsesTheSameAsynchronousLifecycleAsGameBoot)
{
  Send(Action::OpenMenu);
  host.execute = [&](const Command& command) {
    host.state.lifecycle = Lifecycle::Booting;
    return Result{command, ResultStatus::Dispatched};
  };
  Send(Action::BootIPL, 1);
  ASSERT_TRUE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->pending->action, Action::BootIPL);
  host.state.lifecycle = Lifecycle::Running;
  controller.Refresh();
  EXPECT_FALSE(controller.GetSnapshot()->pending);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Desktop);
}

TEST_F(BigPictureControllerTest, PauseCoverReadUsesRunningGameWithoutEnablingLibraryMutations)
{
  host.state.game_path = "running.iso";
  host.state.library = std::make_shared<LibrarySnapshot>();
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Succeeded}; };
  InGame(Lifecycle::Paused);
  auto cover = Make(Action::Library, 0, "running.iso");
  cover.library_operation = LibraryOperation::Cover;
  controller.Submit(cover);
  ASSERT_EQ(host.executed.size(), 1u);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Succeeded);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Pause);
  EXPECT_EQ(host.state.lifecycle, Lifecycle::Paused);
  auto other = Make(Action::Library, 0, "another.iso");
  other.library_operation = LibraryOperation::Cover;
  controller.Submit(other);
  EXPECT_EQ(host.executed.size(), 1u);
  auto mutation = Make(Action::Library);
  mutation.library_operation = LibraryOperation::AddFolder;
  controller.Submit(mutation);
  EXPECT_EQ(host.executed.size(), 1u);
  Send(Action::Stop);
  ASSERT_TRUE(controller.GetSnapshot()->confirmation);
  cover.id = ++id;
  controller.Submit(cover);
  EXPECT_EQ(host.executed.size(), 1u);
  EXPECT_TRUE(controller.GetSnapshot()->confirmation);
}
TEST_F(BigPictureControllerTest, HomeRoutesLibraryLaunchAndExitWithoutLeavingTheSession)
{
  host.execute = [](const Command& command) { return Result{command, ResultStatus::Succeeded}; };
  Send(Action::OpenMenu);
  EXPECT_EQ(controller.GetSnapshot()->screens, std::vector{Screen::Home});
  Send(Action::About);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::About);
  Send(Action::Back);
  Send(Action::ShowLibrary);
  EXPECT_EQ(controller.GetSnapshot()->screens, (std::vector{Screen::Home, Screen::Library}));
  Send(Action::Back);
  Send(Action::StartGame);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::StartGame);
  auto command = Make(Action::Library);
  command.library_operation = LibraryOperation::OpenFile;
  controller.Submit(command);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::FilePicker);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::StartGame);
  Send(Action::Back);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::ExitMenu);
  Send(Action::Exit);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Confirmation);
  Send(Action::Cancel);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::ExitMenu);
  Send(Action::Back);
  EXPECT_EQ(controller.GetSnapshot()->screens.back(), Screen::Home);
  Send(Action::ReturnToDesktop);
  EXPECT_EQ(controller.GetSnapshot()->mode, Mode::Desktop);
}
}  // namespace
