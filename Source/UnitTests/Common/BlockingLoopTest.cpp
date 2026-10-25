// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "Common/BlockingLoop.h"

TEST(BlockingLoop, DynamicTimeoutSleepsWhenHiddenAndWakesForVisibleUI)
{
  using namespace std::chrono_literals;
  Common::BlockingLoop loop;
  std::atomic<bool> visible{false};
  std::atomic<unsigned> calls{0};
  loop.Prepare();
  std::thread worker([&] {
    loop.RunWithTimeout(
        [&] {
          ++calls;
          loop.AllowSleep();
        },
        [&] { return visible ? 5 : 0; });
  });
  loop.Wait();
  const auto sleeping = calls.load();
  std::this_thread::sleep_for(25ms);
  EXPECT_EQ(calls, sleeping);
  visible = true;
  loop.Wakeup();
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (calls < sleeping + 4 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);
  EXPECT_GE(calls.load(), sleeping + 4);
  visible = false;
  loop.Wakeup();
  loop.Wait();
  const auto hidden = calls.load();
  std::this_thread::sleep_for(25ms);
  EXPECT_EQ(calls, hidden);
  loop.Stop();
  worker.join();
}

TEST(BlockingLoop, MultiThreaded)
{
  Common::BlockingLoop loop;
  std::atomic signaled_a(0);
  std::atomic received_a(0);
  std::atomic signaled_b(0);
  std::atomic received_b(0);
  for (int i = 0; i < 100; i++)
  {
    // Invalidate the current state.
    received_a.store(signaled_a.load() + 1);
    received_b.store(signaled_b.load() + 123);

    // Must not block as the loop is stopped.
    loop.Wait();

    std::thread loop_thread([&] {
      loop.Run([&] {
        received_a.store(signaled_a.load());
        received_b.store(signaled_b.load());
      });
    });

    // Now Wait must block.
    loop.Prepare();

    // The payload must run at least once on startup.
    loop.Wait();
    EXPECT_EQ(signaled_a.load(), received_a.load());
    EXPECT_EQ(signaled_b.load(), received_b.load());

    std::thread run_a_thread([&] {
      for (int j = 0; j < 100; j++)
      {
        for (int k = 0; k < 100; k++)
        {
          signaled_a++;
          loop.Wakeup();
        }

        loop.Wait();
        EXPECT_EQ(signaled_a.load(), received_a.load());
      }
    });
    std::thread run_b_thread([&] {
      for (int j = 0; j < 100; j++)
      {
        for (int k = 0; k < 100; k++)
        {
          signaled_b++;
          loop.Wakeup();
        }

        loop.Wait();
        EXPECT_EQ(signaled_b.load(), received_b.load());
      }
    });

    run_a_thread.join();
    run_b_thread.join();

    loop.Stop();

    // Must not block
    loop.Wait();

    loop_thread.join();
  }
}
