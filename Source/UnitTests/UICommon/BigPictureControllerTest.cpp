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
  FakeHost host;
  Controller controller{host};
  std::uint64_t id = 0;
};

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

TEST_F(BigPictureControllerTest, VoidStateOperationsAreDispatchedRatherThanSuccessful)
{
  InGame();
  Send(Action::SaveState, 10);
  EXPECT_EQ(controller.GetSnapshot()->result->status, ResultStatus::Dispatched);
  EXPECT_FALSE(controller.GetSnapshot()->pending);
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
