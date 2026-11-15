// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>

#include <gtest/gtest.h>
#include <imgui.h>

#include "Common/BigPicture/PresentationState.h"
#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"

TEST(NetPlayChatUITest, PublishedSnapshotSurvivesWorkerClearingTheUI)
{
  SetNetPlayChatUI(std::make_shared<NetPlayChatUI>(nullptr));
  auto snapshot = GetNetPlayChatUI();
  std::weak_ptr<NetPlayChatUI> lifetime = snapshot;
  std::thread worker([] { SetNetPlayChatUI(nullptr); });
  worker.join();

  EXPECT_EQ(GetNetPlayChatUI(), nullptr);
  EXPECT_FALSE(lifetime.expired());
  snapshot.reset();
  EXPECT_TRUE(lifetime.expired());
}

TEST(NetPlayGolfUITest, PublishedSnapshotSurvivesWorkerClearingTheUI)
{
  SetNetPlayGolfUI(std::make_shared<NetPlayGolfUI>(std::weak_ptr<NetPlay::NetPlayClient>{}));
  auto snapshot = GetNetPlayGolfUI();
  std::weak_ptr<NetPlayGolfUI> lifetime = snapshot;
  std::thread worker([] { SetNetPlayGolfUI(nullptr); });
  worker.join();

  EXPECT_EQ(GetNetPlayGolfUI(), nullptr);
  EXPECT_FALSE(lifetime.expired());
  snapshot.reset();
  EXPECT_TRUE(lifetime.expired());
}

TEST(NetPlayChatUITest, WorkerActivationRequiresNoImGuiContext)
{
  ASSERT_EQ(ImGui::GetCurrentContext(), nullptr);
  NetPlayChatUI chat({});
  std::thread worker([&] { chat.Activate(); });
  worker.join();
  BigPicture::GetPresentationState().SetRendererReady(false);
}

TEST(NetPlayChatUITest, RenderOwnerAppliesQueuedActivationAndFocusToggle)
{
  auto* context = ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1280, 720);
  io.DeltaTime = 1.0f / 60.0f;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  io.Fonts->AddFontDefault();
  {
    NetPlayChatUI chat({});
    std::thread worker([&] {
      chat.AppendChat("A queued message", {1.0f, 1.0f, 1.0f});
      chat.Activate();
    });
    worker.join();
    const auto frames = [&] {
      for (int i = 0; i < 3; ++i)
      {
        ImGui::NewFrame();
        chat.Display();
        ImGui::Render();
      }
    };
    frames();
    EXPECT_TRUE(io.WantCaptureKeyboard);
    std::thread close([&] { chat.Activate(); });
    close.join();
    frames();
    EXPECT_FALSE(io.WantCaptureKeyboard);
  }
  ImGui::DestroyContext(context);
  BigPicture::GetPresentationState().SetRendererReady(false);
}
