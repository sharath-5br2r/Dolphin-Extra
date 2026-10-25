// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "Core/Core.h"
#include "VideoCommon/BigPicture/StandaloneSession.h"

namespace
{
using namespace std::chrono_literals;
using VideoCommon::BigPicture::StandaloneSession;

template <typename Predicate>
bool WaitUntil(Predicate predicate, std::chrono::steady_clock::duration timeout = 2s)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!predicate() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);
  return predicate();
}

class BigPictureStandaloneSessionTest : public testing::Test
{
protected:
  void SetUp() override
  {
    auto snapshot = std::make_shared<BigPicture::Snapshot>();
    snapshot->mode = BigPicture::Mode::Library;
    snapshot->screens = {BigPicture::Screen::Library};
    presentation.Publish(snapshot);
  }
  StandaloneSession::Functions Functions()
  {
    return {[this] {
              owner = std::this_thread::get_id();
              EXPECT_TRUE(Core::IsGPUThread());
              presentation.SetRendererReady(true);
              ++initializations;
              return true;
            },
            [this] {
              EXPECT_EQ(owner, std::this_thread::get_id());
              EXPECT_EQ(shutdowns, 0u);
              ++frames;
              presentation.FrameCompleted();
              return true;
            },
            [this] {
              EXPECT_EQ(owner, std::this_thread::get_id());
              presentation.SetRendererReady(false);
              ++shutdowns;
            },
            {}};
  }
  BigPicture::PresentationState presentation;
  std::thread::id owner;
  std::atomic<unsigned> initializations = 0, frames = 0, shutdowns = 0;
};

TEST_F(BigPictureStandaloneSessionTest, OneOwnerInitializesPresentsAndDestroysBeforeRelease)
{
  auto session = std::make_unique<StandaloneSession>(Functions(), presentation);
  ASSERT_TRUE(WaitUntil([&] { return frames >= 2; }));
  EXPECT_NE(owner, std::this_thread::get_id());
  EXPECT_TRUE(VideoCommon::BigPicture::IsStandaloneActive());
  StandaloneSession rejected(Functions(), presentation);
  EXPECT_EQ(rejected.GetStatus(), StandaloneSession::Status::Failed);
  EXPECT_EQ(initializations, 1u);
  session->Stop();
  ASSERT_TRUE(WaitUntil([&] { return session->IsFinished(); }));
  EXPECT_EQ(shutdowns, 1u);
  EXPECT_TRUE(VideoCommon::BigPicture::IsStandaloneActive());
  session.reset();
  EXPECT_FALSE(VideoCommon::BigPicture::IsStandaloneActive());
}

TEST_F(BigPictureStandaloneSessionTest, StopDuringCreationDrainsWithoutPresenting)
{
  auto functions = Functions();
  const auto initialize = functions.initialize;
  std::mutex mutex;
  std::condition_variable wake;
  bool entered = false, release = false;
  functions.initialize = [&] {
    std::unique_lock lock(mutex);
    entered = true;
    wake.notify_all();
    wake.wait(lock, [&] { return release; });
    return initialize();
  };
  StandaloneSession session(std::move(functions), presentation);
  {
    std::unique_lock lock(mutex);
    const bool started = wake.wait_for(lock, 2s, [&] { return entered; });
    EXPECT_TRUE(started);
    session.Stop();
    release = true;
    wake.notify_all();
  }
  ASSERT_TRUE(WaitUntil([&] { return session.IsFinished(); }));
  EXPECT_EQ(frames, 0u);
  EXPECT_EQ(shutdowns, 1u);
  EXPECT_EQ(session.GetStatus(), StandaloneSession::Status::Stopped);
}

TEST_F(BigPictureStandaloneSessionTest, FailedCreationDoesNotStartAnOwnerOrGuestShutdown)
{
  auto functions = Functions();
  functions.initialize = [] { return false; };
  StandaloneSession session(std::move(functions), presentation);
  ASSERT_TRUE(WaitUntil([&] { return session.IsFinished(); }));
  EXPECT_EQ(session.GetStatus(), StandaloneSession::Status::Failed);
  EXPECT_EQ(frames, 0u);
  EXPECT_EQ(shutdowns, 0u);
}

TEST_F(BigPictureStandaloneSessionTest, PersistentSurfaceFailureDrainsTheOwnerAndReportsFailure)
{
  auto functions = Functions();
  functions.render = [&] {
    ++frames;
    return false;
  };
  StandaloneSession session(std::move(functions), presentation);
  ASSERT_TRUE(WaitUntil([&] { return session.IsFinished(); }, 5s));
  EXPECT_EQ(session.GetStatus(), StandaloneSession::Status::Failed);
  EXPECT_EQ(frames, 60u);
  EXPECT_EQ(shutdowns, 1u);
}

TEST_F(BigPictureStandaloneSessionTest, SurfaceRequestsCoalesceAndRunOnTheGraphicsOwner)
{
  auto functions = Functions();
  const auto initialize = functions.initialize;
  std::atomic<bool> release = false, entered = false;
  functions.initialize = [&] {
    entered = true;
    EXPECT_TRUE(WaitUntil([&] { return release.load(); }));
    return initialize();
  };
  std::atomic<unsigned> updates = 0;
  functions.update_surface = [&](bool changed, void* surface, bool resized, float scale,
                                 std::optional<bool> fullscreen) {
    EXPECT_EQ(owner, std::this_thread::get_id());
    EXPECT_TRUE(changed);
    EXPECT_EQ(surface, reinterpret_cast<void*>(1000));
    EXPECT_TRUE(resized);
    EXPECT_EQ(scale, 2.0f);
    EXPECT_EQ(fullscreen, false);
    ++updates;
  };
  StandaloneSession session(std::move(functions), presentation);
  ASSERT_TRUE(WaitUntil([&] { return entered.load(); }));
  for (std::uintptr_t i = 1; i <= 1000; ++i)
  {
    VideoCommon::BigPicture::ChangeStandaloneSurface(reinterpret_cast<void*>(i));
    VideoCommon::BigPicture::ResizeStandaloneSurface(2);
    VideoCommon::BigPicture::ResizeStandaloneSurface();
    VideoCommon::BigPicture::SetStandaloneFullscreen(i != 1000);
  }
  release = true;
  ASSERT_TRUE(WaitUntil([&] { return updates != 0; }));
  session.Stop();
  ASSERT_TRUE(WaitUntil([&] { return session.IsFinished(); }));
  EXPECT_EQ(updates, 1u);
}

TEST_F(BigPictureStandaloneSessionTest, InactiveSessionSleepsAndFocusWakeResumesPresentation)
{
  presentation.SetFocused(false);
  StandaloneSession session(Functions(), presentation);
  ASSERT_TRUE(WaitUntil([&] { return session.GetStatus() == StandaloneSession::Status::Ready; }));
  EXPECT_EQ(frames, 0u);
  presentation.SetFocused(true);
  VideoCommon::BigPicture::WakeStandalone();
  ASSERT_TRUE(WaitUntil([&] { return frames != 0; }));
  session.Stop();
  ASSERT_TRUE(WaitUntil([&] { return session.IsFinished(); }));
  EXPECT_EQ(shutdowns, 1u);
}
}  // namespace
