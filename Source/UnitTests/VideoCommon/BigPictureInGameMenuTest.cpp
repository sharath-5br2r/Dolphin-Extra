// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <imgui.h>

#include "UICommon/BigPicture/Controller.h"
#include "VideoCommon/BigPicture/InGameMenu.h"

namespace
{
using namespace BigPicture;
using VideoCommon::BigPicture::BuildInGameMenu;
using VideoCommon::BigPicture::MenuResult;

class BigPictureInGameMenuTest : public testing::Test
{
protected:
  void SetUp() override
  {
    snapshot.mode = Mode::InGame;
    snapshot.host.lifecycle = Lifecycle::Paused;
    snapshot.screens = {Screen::Pause};
    for (std::size_t i = 0; i < text.actions.size(); ++i)
      text.actions[i] = "Action " + std::to_string(i);
    for (std::size_t i = 0; i < text.slot_labels.size(); ++i)
      text.slot_labels[i] = "Slot " + std::to_string(i + 1);
    text.reasons[static_cast<std::size_t>(UnavailableReason::Hardcore)] = "Hardcore";
    text.reasons[static_cast<std::size_t>(UnavailableReason::DiscUnavailable)] = "Not a disc game";
    text.empty_slot = "Empty";
    text.unknown_slot = "Unreadable";
    text.requested = "Requested";
  }
  auto Build()
  {
    for (std::size_t i = 0; i < snapshot.availability.size(); ++i)
      snapshot.availability[i] = GetAvailability(static_cast<Action>(i), snapshot.host);
    return BuildInGameMenu(snapshot, text, {});
  }
  Snapshot snapshot;
  NavigationText text;
};

TEST_F(BigPictureInGameMenuTest, RootRoutesExplicitResumeAndImplicitCloseSeparately)
{
  const auto menu = Build();
  EXPECT_EQ(menu.page.default_focus, "resume");
  EXPECT_EQ(menu.Activate({"resume"})->action, Action::Resume);
  EXPECT_EQ(menu.Activate({"close"})->action, Action::Back);
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::Back);
  EXPECT_EQ(menu.Activate({"save"})->action, Action::SaveStates);
  EXPECT_EQ(menu.Activate({"load"})->action, Action::LoadStates);
  EXPECT_EQ(menu.Activate({"screenshot"})->action, Action::Screenshot);
  EXPECT_FALSE(menu.Activate({"disc"}));
  EXPECT_EQ(menu.page.items[4].detail, "Not a disc game");
}

TEST_F(BigPictureInGameMenuTest, TenSlotsCarryOneBasedNumbersAndDisableEmptyUnreadableLoads)
{
  snapshot.screens.push_back(Screen::LoadStates);
  snapshot.host.state_slots_ready = true;
  snapshot.host.state_slots[1] = {true, false, {}};
  snapshot.host.state_slots[9] = {true, true, "Saved date"};
  auto menu = Build();
  EXPECT_EQ(menu.page.items.size(), 12u);
  EXPECT_FALSE(menu.Activate({"slot-1"}));
  EXPECT_FALSE(menu.Activate({"slot-2"}));
  EXPECT_EQ(menu.page.items[0].detail, "Empty");
  EXPECT_EQ(menu.page.items[1].detail, "Unreadable");
  ASSERT_TRUE(menu.Activate({"slot-10"}));
  EXPECT_EQ(menu.Activate({"slot-10"})->slot, 10);
  EXPECT_EQ(menu.Activate({"slot-10"})->action, Action::LoadState);
  snapshot.screens.back() = Screen::SaveStates;
  menu = Build();
  EXPECT_EQ(menu.Activate({"slot-1"})->slot, 1);
  EXPECT_EQ(menu.Activate({"slot-1"})->action, Action::SaveState);
}

TEST_F(BigPictureInGameMenuTest, PolicyChangesRemovePreviouslyAvailableLoadCommands)
{
  snapshot.screens.push_back(Screen::LoadStates);
  snapshot.host.state_slots_ready = true;
  snapshot.host.state_slots[0] = {true, true, "Date"};
  EXPECT_TRUE(Build().Activate({"slot-1"}));
  snapshot.host.hardcore = true;
  const auto menu = Build();
  EXPECT_FALSE(menu.Activate({"slot-1"}));
  EXPECT_EQ(menu.page.items[0].detail, "Hardcore");
  EXPECT_TRUE(menu.Activate({"back"}));
}

TEST_F(BigPictureInGameMenuTest, EveryConfirmationHasFreshCancelFocusAndCarriesItsRequestId)
{
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1280, 720);
  io.DeltaTime = 1.0f / 60.0f;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_HasGamepad;
  io.Fonts->AddFontDefault();
  VideoCommon::BigPicture::NavigationToolkit toolkit;
  std::uint64_t id = 0;
  snapshot.screens.push_back(Screen::Confirmation);
  for (const auto action : {Action::Reset, Action::Stop, Action::Exit, Action::SaveState})
  {
    snapshot.confirmation = Command{snapshot.generation, ++id, action, {}, 1};
    const auto menu = Build();
    ASSERT_TRUE(menu.Activate({"confirm"}));
    EXPECT_EQ(menu.Activate({"confirm"})->confirmation_id, id);
    EXPECT_TRUE(menu.page.dialog);
    EXPECT_EQ(menu.page.default_focus, "cancel");
    for (int i = 0; i < 4; ++i)
    {
      ImGui::NewFrame();
      toolkit.Draw(menu.page, {}, "Select / Back");
      ImGui::Render();
    }
    EXPECT_EQ(toolkit.GetMemory(menu.page.id).item, "cancel");
    io.AddKeyEvent(ImGuiKey_UpArrow, true);
    ImGui::NewFrame();
    toolkit.Draw(menu.page, {}, "Select / Back");
    ImGui::Render();
    io.AddKeyEvent(ImGuiKey_UpArrow, false);
    // A fresh dialog must not inherit Confirm focus from the previous destructive action.
  }
  ImGui::DestroyContext();
}

TEST_F(BigPictureInGameMenuTest, FailedAndDispatchedResultsHaveDistinctPresentations)
{
  snapshot.error = Result{{snapshot.generation, 1, Action::SaveState},
                          ResultStatus::Failed,
                          UnavailableReason::None,
                          "Disk full"};
  snapshot.screens.push_back(Screen::Error);
  auto menu = Build();
  EXPECT_EQ(menu.page.description, "Disk full");
  EXPECT_TRUE(menu.Activate({"back"}));
  snapshot.error.reset();
  snapshot.screens.pop_back();
  snapshot.result = Result{{snapshot.generation, 1, Action::SaveState}, ResultStatus::Dispatched};
  menu = Build();
  EXPECT_NE(menu.page.description.find("Requested"), std::string::npos);
}
TEST_F(BigPictureInGameMenuTest, IdleLandingOffersBootAndNativeHandoffsWithoutGameActions)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Library};
  snapshot.host.launch_path = "selected-game.iso";
  snapshot.host.launch_title = "Selected title";
  const auto menu = Build();
  ASSERT_TRUE(menu.Activate({"boot"}));
  EXPECT_EQ(menu.Activate({"boot"})->path, "selected-game.iso");
  EXPECT_EQ(menu.Activate({"boot"})->action, Action::Boot);
  EXPECT_EQ(menu.Activate({"desktop"})->action, Action::ReturnToDesktop);
  EXPECT_EQ(menu.Activate({"open-game"})->action, Action::OpenGame);
  EXPECT_EQ(menu.Activate({"exit"})->action, Action::Exit);
  EXPECT_FALSE(menu.Activate({"save"}));
  EXPECT_FALSE(menu.Activate({"resume"}));
}
}  // namespace
