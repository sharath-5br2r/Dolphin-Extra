// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "Common/BigPicture/PresentationState.h"

namespace
{
using namespace std::chrono_literals;
using namespace BigPicture;

std::shared_ptr<const Snapshot> Menu(std::uint64_t generation = 1)
{
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->generation = generation;
  snapshot->mode = Mode::InGame;
  snapshot->host.lifecycle = Lifecycle::Paused;
  return snapshot;
}

TEST(BigPicturePresentationTest, PausedPresentsArePacedAndDisabledMenusSleep)
{
  PresentationState state;
  const auto now = PresentationState::Clock::now();
  EXPECT_EQ(state.GetWaitMilliseconds(now, 0), 0);
  EXPECT_FALSE(state.TryBeginPresent(now));
  state.SetRendererReady(true);
  state.Publish(Menu());
  ASSERT_TRUE(state.TryBeginPresent(now));
  EXPECT_FALSE(state.TryBeginPresent(now));
  state.EndPresent();
  EXPECT_FALSE(state.TryBeginPresent(now + 10ms));
  EXPECT_EQ(state.GetWaitMilliseconds(now, 0), 17);
  EXPECT_TRUE(state.TryBeginPresent(now + 17ms));
  state.EndPresent();
  state.Publish(std::make_shared<const Snapshot>());
  EXPECT_FALSE(state.TryBeginPresent(now + 1s));
  EXPECT_EQ(state.GetWaitMilliseconds(now, 0), 0);
}

TEST(BigPicturePresentationTest, SuspensionDrainsInFlightWorkAndRejectsNewPresents)
{
  PresentationState state;
  state.SetRendererReady(true);
  state.Publish(Menu());
  ASSERT_TRUE(state.TryBeginPresent(PresentationState::Clock::now()));
  auto lock = std::async(std::launch::async, [&] { state.Suspend(); });
  const auto deadline = PresentationState::Clock::now() + 2s;
  while (state.CanWake() && PresentationState::Clock::now() < deadline)
    std::this_thread::yield();
  EXPECT_FALSE(state.CanWake());
  EXPECT_EQ(lock.wait_for(0ms), std::future_status::timeout);
  EXPECT_FALSE(state.TryBeginPresent(PresentationState::Clock::now() + 1s));
  state.EndPresent();
  ASSERT_EQ(lock.wait_for(2s), std::future_status::ready);
  lock.get();
  EXPECT_FALSE(state.TryBeginPresent(PresentationState::Clock::now() + 1s));
  state.Suspend();
  state.Resume();
  EXPECT_FALSE(state.CanWake());
  state.Resume();
  EXPECT_TRUE(state.CanWake());
  EXPECT_TRUE(state.TryBeginPresent(PresentationState::Clock::now() + 1s));
  state.EndPresent();
}

TEST(BigPicturePresentationTest, FocusLossAndRendererTeardownClearInputAndCapture)
{
  PresentationState state;
  state.SetRendererReady(true);
  state.Publish(Menu());
  const auto input = state.TakeInput();
  state.PublishCapture(input.epoch, true, true);
  state.SetFocused(false);
  EXPECT_TRUE(state.TakeInput().reset);
  EXPECT_FALSE(state.TryBeginPresent(PresentationState::Clock::now()));
  EXPECT_TRUE(state.GetCapture().modal);
  state.SetFocused(true);
  state.SetRendererReady(false);
  EXPECT_FALSE(state.GetCapture().keyboard);
  EXPECT_FALSE(state.GetCapture().mouse);
  EXPECT_FALSE(state.GetCapture().modal);
}

TEST(BigPicturePresentationTest, LateRenderCaptureCannotResurrectClosedMenuCapture)
{
  PresentationState state;
  state.Publish(Menu());
  const auto input = state.TakeInput();
  state.Publish(std::make_shared<const Snapshot>());
  state.PublishCapture(input.epoch, true, true);
  EXPECT_FALSE(state.GetCapture().keyboard);
  EXPECT_FALSE(state.GetCapture().mouse);
}

TEST(BigPicturePresentationTest, LateNonModalCaptureCannotSurviveFocusLoss)
{
  PresentationState state;
  state.SetRendererReady(true);
  const auto frame = state.TakeInput();
  state.PublishCapture(frame.epoch, true, true);
  state.SetFocused(false);
  state.PublishCapture(frame.epoch, true, true);
  EXPECT_FALSE(state.GetCapture().keyboard);
  EXPECT_FALSE(state.GetCapture().mouse);
}

TEST(BigPicturePresentationTest, InputQueueRejectsOldSessionsAndRetainsKeyReleases)
{
  PresentationState state;
  state.Publish(Menu());
  state.Enqueue({InputEvent::Type::Key, 1, 27, true});
  state.Publish(Menu(2));
  state.Enqueue({InputEvent::Type::Key, 1, 27, true});
  state.Enqueue({InputEvent::Type::Key, 2, 27, false});
  const auto input = state.TakeInput();
  ASSERT_EQ(input.events.size(), 1u);
  EXPECT_FALSE(input.events.front().down);
  EXPECT_TRUE(input.reset);
}

TEST(BigPicturePresentationTest, QueueOverflowResetsPressedStateBeforeNewestEvents)
{
  PresentationState state;
  state.Publish(Menu());
  state.TakeInput();
  for (int i = 0; i < 600; ++i)
    state.Enqueue({InputEvent::Type::Key, 1, 27, true});
  state.Enqueue({InputEvent::Type::Key, 1, 27, false});
  const auto input = state.TakeInput();
  EXPECT_LE(input.events.size(), 512u);
  EXPECT_TRUE(input.reset);
  ASSERT_FALSE(input.events.empty());
  EXPECT_FALSE(input.events.back().down);
}

TEST(BigPicturePresentationTest, InputEpochDoesNotAffectGameplayUntilMenuHasCapturedInput)
{
  PresentationState state;
  const auto epoch = state.GetInputEpoch();
  state.SetRendererReady(true);
  state.Publish(std::make_shared<const Snapshot>());
  state.SetFocused(false);
  state.ResetInput();
  EXPECT_EQ(state.GetInputEpoch(), epoch);
  state.Publish(Menu());
  state.ResetInput();
  EXPECT_GT(state.GetInputEpoch(), epoch);
}

TEST(BigPicturePresentationTest, ExplicitOverlayWakeHasBoundedWorkWithoutOpeningAMenu)
{
  PresentationState state;
  state.SetRendererReady(true);
  const auto now = PresentationState::Clock::now();
  state.RequestFrames();
  ASSERT_TRUE(state.TryBeginPresent(now));
  state.FrameCompleted();
  state.EndPresent();
  ASSERT_TRUE(state.TryBeginPresent(now + 20ms));
  state.FrameCompleted();
  state.EndPresent();
  EXPECT_FALSE(state.TryBeginPresent(now + 40ms));
  EXPECT_EQ(state.GetWaitMilliseconds(now + 40ms, 0), 0);
}
}  // namespace
