// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QThread>
#include <gtest/gtest.h>

#include "Common/BigPicture/PresentationState.h"
#include "DolphinQt/BigPictureHost.h"

namespace
{
using namespace BigPicture;

TEST(BigPictureHostTest, QueuedNavigationCannotExecuteAfterFocusReset)
{
  auto& presentation = GetPresentationState();
  presentation.SetFocused(true);
  BigPictureHost host(
      nullptr, [] { return HostState{Lifecycle::Running}; },
      [](const Command& command) { return Result{command}; });
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu,
                     presentation.GetFrameInputEpoch());
  presentation.SetFocused(false);
  presentation.SetFocused(true);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu,
                     presentation.GetFrameInputEpoch());
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::InGame);
}

TEST(BigPictureHostTest, WorkerCommandsExecuteOnlyOnQtOwnerAndSnapshotsStayValid)
{
  HostState state{Lifecycle::Running};
  int calls = 0;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        EXPECT_EQ(QThread::currentThread(), QCoreApplication::instance()->thread());
        ++calls;
        return Result{command, ResultStatus::Dispatched};
      });
  const auto old = host.GetSnapshot();
  std::thread worker([&] {
    const auto snapshot = host.GetSnapshot();
    host.Submit({snapshot->generation, 1, Action::SaveState, {}, 1});
  });
  worker.join();
  EXPECT_EQ(calls, 0);
  QCoreApplication::processEvents();
  EXPECT_EQ(calls, 1);
  EXPECT_FALSE(old->result);
  ASSERT_TRUE(host.GetSnapshot()->result);
  EXPECT_EQ(host.GetSnapshot()->result->status, ResultStatus::Dispatched);
}

TEST(BigPictureHostTest, SessionChangeAndPolicyChangeAreCheckedAfterQueueing)
{
  HostState state{Lifecycle::Running};
  int calls = 0;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        ++calls;
        return Result{command, ResultStatus::Dispatched};
      });
  host.Submit({host.GetSnapshot()->generation, 1, Action::LoadState, {}, 1});
  state.hardcore = true;
  QCoreApplication::processEvents();
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(host.GetSnapshot()->result->reason, UnavailableReason::Hardcore);
  host.Submit({host.GetSnapshot()->generation, 2, Action::SaveState, {}, 1});
  host.InvalidateSession();
  QCoreApplication::processEvents();
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(host.GetSnapshot()->result->status, ResultStatus::Stale);
}

TEST(BigPictureHostTest, DestructionDropsQueuedActions)
{
  int calls = 0;
  {
    BigPictureHost host(
        nullptr, [] { return HostState{Lifecycle::Running}; },
        [&](const Command& command) {
          ++calls;
          return Result{command, ResultStatus::Dispatched};
        });
    host.Submit({host.GetSnapshot()->generation, 1, Action::SaveState, {}, 1});
  }
  QCoreApplication::processEvents();
  EXPECT_EQ(calls, 0);
}

TEST(BigPictureHostTest, ClosingMenuOnlyResumesItsOwnPause)
{
  HostState state{Lifecycle::Running};
  int pauses = 0;
  int resumes = 0;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [](const Command& command) { return Result{command, ResultStatus::Dispatched}; },
      [&](bool pause) {
        state.lifecycle = pause ? Lifecycle::Paused : Lifecycle::Running;
        ++state.pause_revision;
        pause ? ++pauses : ++resumes;
      });
  host.Submit({host.GetSnapshot()->generation, 1, Action::OpenMenu});
  QCoreApplication::processEvents();
  EXPECT_EQ(pauses, 1);
  EXPECT_EQ(state.lifecycle, Lifecycle::Paused);
  host.RequestBack();
  QCoreApplication::processEvents();
  EXPECT_EQ(resumes, 1);
  EXPECT_EQ(state.lifecycle, Lifecycle::Running);
}

TEST(BigPictureHostTest, ExternalPauseResumeAndFocusLossInvalidateMenuPauseOwnership)
{
  for (int change = 0; change < 3; ++change)
  {
    HostState state{Lifecycle::Running};
    int resumes = 0;
    BigPictureHost host(
        nullptr, [&] { return state; },
        [](const Command& command) { return Result{command, ResultStatus::Dispatched}; },
        [&](bool pause) {
          state.lifecycle = pause ? Lifecycle::Paused : Lifecycle::Running;
          ++state.pause_revision;
          if (!pause)
            ++resumes;
        });
    host.Submit({host.GetSnapshot()->generation, 1, Action::OpenMenu});
    QCoreApplication::processEvents();
    if (change == 0)
      ++state.pause_revision;  // Another pause request even though it was already paused.
    else if (change == 1)
    {
      state.lifecycle = Lifecycle::Running;
      ++state.pause_revision;
    }
    else
      host.InvalidatePauseOwnership();
    host.Refresh();
    host.RequestBack();
    QCoreApplication::processEvents();
    EXPECT_EQ(resumes, 0);
  }
}

TEST(BigPictureHostTest, AlreadyPausedAndNetplaySessionsNeverAcquireMenuPause)
{
  for (const bool netplay : {false, true})
  {
    HostState state{netplay ? Lifecycle::Running : Lifecycle::Paused, netplay};
    int changes = 0;
    BigPictureHost host(
        nullptr, [&] { return state; },
        [](const Command& command) { return Result{command, ResultStatus::Dispatched}; },
        [&](bool) { ++changes; });
    host.Submit({host.GetSnapshot()->generation, 1, Action::OpenMenu});
    QCoreApplication::processEvents();
    host.RequestBack();
    QCoreApplication::processEvents();
    EXPECT_EQ(changes, 0);
  }
}

TEST(BigPictureHostTest, InterveningFocusOrPauseCannotAcquireMenuPauseOwnership)
{
  for (const bool external_pause : {false, true})
  {
    HostState state{Lifecycle::Running};
    BigPictureHost* current = nullptr;
    int resumes = 0;
    BigPictureHost host(
        nullptr, [&] { return state; },
        [](const Command& command) { return Result{command, ResultStatus::Dispatched}; },
        [&](bool pause) {
          if (pause)
          {
            state.lifecycle = Lifecycle::Paused;
            ++state.pause_revision;
            if (external_pause)
              ++state.pause_revision;
            else
              current->InvalidatePauseOwnership();
          }
          else
            ++resumes;
        });
    current = &host;
    host.Submit({host.GetSnapshot()->generation, 1, Action::OpenMenu});
    QCoreApplication::processEvents();
    host.RequestBack();
    QCoreApplication::processEvents();
    EXPECT_EQ(resumes, 0);
  }
}
TEST(BigPictureHostTest, QueuedSlotAndConfirmationPayloadsReachTheMatchingHostAction)
{
  HostState state{Lifecycle::Paused};
  state.state_slots_ready = true;
  state.state_slots[9].occupied = true;
  std::vector<Command> executed;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        executed.push_back(command);
        return Result{command, ResultStatus::Dispatched};
      });
  const auto generation = host.GetSnapshot()->generation;
  host.RequestAction(generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(generation, Action::SaveStates);
  QCoreApplication::processEvents();
  host.RequestAction(generation, Action::SaveState, {}, 10);
  QCoreApplication::processEvents();
  ASSERT_TRUE(host.GetSnapshot()->confirmation);
  const auto request_id = host.GetSnapshot()->confirmation->id;
  host.RequestAction(generation, Action::Confirm, {}, 0, request_id + 1);
  QCoreApplication::processEvents();
  EXPECT_TRUE(executed.empty());
  host.RequestAction(generation, Action::Confirm, {}, 0, request_id);
  QCoreApplication::processEvents();
  ASSERT_EQ(executed.size(), 1u);
  EXPECT_EQ(executed[0].action, Action::SaveState);
  EXPECT_EQ(executed[0].slot, 10);
  EXPECT_EQ(executed[0].confirmation_id, request_id);
}

TEST(BigPictureHostTest, LoadInvalidatesImplicitResumeButExplicitResumeRemainsAvailable)
{
  HostState state{Lifecycle::Running};
  int resumes = 0;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        if (command.action == Action::LoadState)
          ++state.pause_revision;
        if (command.action == Action::Resume)
        {
          state.lifecycle = Lifecycle::Running;
          ++resumes;
          return Result{command, ResultStatus::Succeeded};
        }
        return Result{command, ResultStatus::Dispatched};
      },
      [&](bool pause) {
        state.lifecycle = pause ? Lifecycle::Paused : Lifecycle::Running;
        ++state.pause_revision;
        if (!pause)
          ++resumes;
      });
  const auto generation = host.GetSnapshot()->generation;
  host.RequestAction(generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(generation, Action::LoadStates);
  QCoreApplication::processEvents();
  host.RequestAction(generation, Action::LoadState, {}, 1);
  QCoreApplication::processEvents();
  host.RequestBack();
  QCoreApplication::processEvents();
  host.RequestBack();
  QCoreApplication::processEvents();
  EXPECT_EQ(state.lifecycle, Lifecycle::Paused);
  EXPECT_EQ(resumes, 0);
  host.RequestAction(generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(generation, Action::Resume);
  QCoreApplication::processEvents();
  EXPECT_EQ(state.lifecycle, Lifecycle::Running);
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
  EXPECT_EQ(resumes, 1);
}
TEST(BigPictureHostTest, StopToNativeLibraryDoesNotAutomaticallyOpenTheMenuOnTheNextBoot)
{
  HostState state{Lifecycle::Running};
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        if (command.action == Action::Stop)
          state.lifecycle = Lifecycle::Stopping;
        return Result{command, ResultStatus::Dispatched};
      });
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, Action::Stop);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, Action::Confirm, {}, 0,
                     host.GetSnapshot()->confirmation->id);
  QCoreApplication::processEvents();
  state.lifecycle = Lifecycle::Idle;
  host.FinishStopToDesktop();
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
  state.lifecycle = Lifecycle::Booting;
  host.InvalidateSession();
  state.lifecycle = Lifecycle::Running;
  host.Refresh();
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
  EXPECT_TRUE(host.GetSnapshot()->screens.empty());
}
TEST(BigPictureHostTest, SynchronousStopCompletionReturnsToDesktopAfterDispatchUnwinds)
{
  HostState state{Lifecycle::Running};
  BigPictureHost* current = nullptr;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        if (command.action == Action::Stop)
        {
          state.lifecycle = Lifecycle::Idle;
          current->FinishStopToDesktop();
        }
        return Result{command, ResultStatus::Dispatched};
      });
  current = &host;
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, Action::Stop);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, Action::Confirm, {}, 0,
                     host.GetSnapshot()->confirmation->id);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
  EXPECT_FALSE(host.GetSnapshot()->pending);
}
TEST(BigPictureHostTest, PendingBootSurvivesIdleOwnerTeardownAndStartsWithoutOpeningTheMenu)
{
  HostState state;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        if (command.action == Action::Boot)
          state.lifecycle = Lifecycle::Booting;
        return Result{command, ResultStatus::Dispatched};
      });
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, Action::Boot, {}, 0, 0, "game.iso");
  QCoreApplication::processEvents();
  const auto accepted = host.GetSnapshot();
  ASSERT_TRUE(accepted->pending);
  EXPECT_EQ(accepted->pending->path, "game.iso");
  host.PrepareForBoot();
  EXPECT_TRUE(host.GetSnapshot()->pending);
  state.lifecycle = Lifecycle::Running;
  host.Refresh();
  EXPECT_EQ(host.GetSnapshot()->result->status, ResultStatus::Succeeded);
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
}

TEST(BigPictureHostTest, RendererRecreationPreservesAnInlineBootErrorAndRejectsOldActivations)
{
  HostState state;
  BigPictureHost host(
      nullptr, [&] { return state; },
      [&](const Command& command) {
        state.lifecycle = Lifecycle::Booting;
        return Result{command, ResultStatus::Dispatched};
      });
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, Action::Boot, {}, 0, 0, "bad.iso");
  QCoreApplication::processEvents();
  state.lifecycle = Lifecycle::Idle;
  host.Refresh();
  const auto failed = host.GetSnapshot();
  ASSERT_TRUE(failed->error);
  host.InvalidatePresentation();
  EXPECT_GT(host.GetSnapshot()->generation, failed->generation);
  EXPECT_EQ(host.GetSnapshot()->screens.back(), Screen::Error);
  host.RequestAction(failed->generation, Action::Back);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->result->status, ResultStatus::Stale);
  EXPECT_TRUE(host.GetSnapshot()->error);
  host.RequestBack();
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->screens.back(), Screen::Library);
}

TEST(BigPictureHostTest, NativeBootFromTheIdleMenuDoesNotAcquireAPlayPause)
{
  HostState state;
  int pauses = 0;
  BigPictureHost host(
      nullptr, [&] { return state; }, [](const Command& command) { return Result{command}; },
      [&](bool) { ++pauses; });
  host.RequestAction(host.GetSnapshot()->generation, Action::OpenMenu);
  QCoreApplication::processEvents();
  host.PrepareForBoot();
  state.lifecycle = Lifecycle::Running;
  host.Refresh();
  EXPECT_EQ(host.GetSnapshot()->mode, Mode::Desktop);
  EXPECT_EQ(pauses, 0);
}
}  // namespace
