// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <imgui.h>

#include "VideoCommon/BigPicture/Navigation.h"

namespace
{
using namespace VideoCommon::BigPicture;

class BigPictureNavigationUITest : public testing::Test
{
protected:
  void SetUp() override
  {
    context = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigNavCursorVisibleAlways = true;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_RendererHasTextures;
    io.KeyRepeatDelay = 0.3f;
    io.KeyRepeatRate = 0.1f;
    io.Fonts->AddFontDefault();
  }
  void TearDown() override { ImGui::DestroyContext(context); }

  MenuResult Frame(const MenuPage& page)
  {
    ImGui::NewFrame();
    auto result = toolkit.Draw(page, {}, "Select / Back");
    ImGui::Render();
    return result;
  }
  void Settle(const MenuPage& page)
  {
    for (int i = 0; i < 4; ++i)
      Frame(page);
  }
  MenuResult Press(const MenuPage& page, ImGuiKey key)
  {
    ImGui::GetIO().AddKeyEvent(key, true);
    auto pressed = Frame(page);
    ImGui::GetIO().AddKeyEvent(key, false);
    auto released = Frame(page);
    auto settled = Frame(page);
    if (pressed.activated || pressed.back)
      return pressed;
    return released.activated || released.back ? released : settled;
  }
  ImGuiContext* context = nullptr;
  NavigationToolkit toolkit;
};

TEST_F(BigPictureNavigationUITest, KeyboardAndControllerNavigateHierarchyAndRestoreParentFocus)
{
  const MenuPage root{"root", "Menu", {}, {{"settings", "Settings"}, {"close", "Close"}}};
  const MenuPage child{"settings", "Settings", {}, {{"one", "One"}, {"two", "Two"}}};
  Settle(root);
  EXPECT_EQ(toolkit.GetMemory("root").item, "settings");
  EXPECT_EQ(Press(root, ImGuiKey_Enter).activated, "settings");
  Settle(child);
  Press(child, ImGuiKey_DownArrow);
  EXPECT_EQ(toolkit.GetMemory("settings").item, "two");
  EXPECT_TRUE(Press(child, ImGuiKey_GamepadFaceRight).back);
  Settle(root);
  EXPECT_EQ(toolkit.GetMemory("root").item, "settings");
  Press(root, ImGuiKey_GamepadDpadDown);
  EXPECT_EQ(toolkit.GetMemory("root").item, "close");
  EXPECT_EQ(Press(root, ImGuiKey_GamepadFaceDown).activated, "close");
  Settle(child);
  EXPECT_EQ(toolkit.GetMemory("settings").item, "two");
}

TEST_F(BigPictureNavigationUITest, DialogDefaultsToCancelAndSwapPreservesLogicalActions)
{
  const MenuPage dialog{"dialog",
                        "Confirm",
                        "A long confirmation message.",
                        {{"confirm", "Confirm"}, {"cancel", "Cancel"}},
                        "cancel",
                        1,
                        true};
  Settle(dialog);
  EXPECT_EQ(toolkit.GetMemory("dialog").item, "cancel");
  const auto swapped_confirm = GetNavigationKey(::BigPicture::NavButton::Back, true);
  const auto swapped_back = GetNavigationKey(::BigPicture::NavButton::Confirm, true);
  EXPECT_EQ(Press(dialog, swapped_confirm).activated, "cancel");
  EXPECT_TRUE(Press(dialog, swapped_back).back);
  EXPECT_TRUE(Press(dialog, ImGuiKey_Escape).back);
}

TEST_F(BigPictureNavigationUITest, DisabledAndRemovedItemsDoNotRetainUnusableFocus)
{
  MenuPage page{
      "page",
      "Menu",
      {},
      {{"disabled", "Disabled", "Unavailable", false}, {"first", "First"}, {"second", "Second"}}};
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("page").item, "first");
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "first");
  Press(page, ImGuiKey_DownArrow);
  EXPECT_EQ(toolkit.GetMemory("page").item, "second");
  page.items.pop_back();
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("page").item, "first");
  page.items[1].enabled = false;
  Settle(page);
  EXPECT_FALSE(Press(page, ImGuiKey_Enter).activated);
}

TEST_F(BigPictureNavigationUITest, HeldDirectionUsesBoundedRepeatAndScrollRestoresAfterBack)
{
  MenuPage page{"list", "List"};
  for (int i = 0; i < 24; ++i)
    page.items.push_back({std::to_string(i), "Item " + std::to_string(i)});
  Settle(page);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadDpadDown, true);
  Settle(page);
  const auto first = toolkit.GetMemory("list").item;
  for (int i = 0; i < 8; ++i)
    Frame(page);
  EXPECT_EQ(toolkit.GetMemory("list").item, first);
  for (int i = 0; i < 75; ++i)
    Frame(page);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadDpadDown, false);
  Settle(page);
  const auto memory = toolkit.GetMemory("list");
  EXPECT_NE(memory.item, first);
  EXPECT_GT(memory.scroll, 0);
  const MenuPage child{"child", "Child", {}, {{"back", "Back"}}};
  Settle(child);
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("list").item, memory.item);
  EXPECT_NEAR(toolkit.GetMemory("list").scroll, memory.scroll, 2.0f);
}

TEST_F(BigPictureNavigationUITest, CardsAndLongUtf8LabelsRenderAtSupportedSizes)
{
  const MenuPage page{"cards",
                      "Menu",
                      {},
                      {{"first", "Very long translated label that wraps over several lines",
                        "Details remain inside the card"},
                       {"utf8", "\xc3\x89\xc3\xa9 \xd0\x9c\xd0\xb5\xd0\xbd\xd1\x8e"}},
                      {},
                      2};
  for (const auto size :
       {ImVec2(1280, 720), ImVec2(1920, 1080), ImVec2(3840, 2160), ImVec2(3440, 1440)})
  {
    ImGui::GetIO().DisplaySize = size;
    toolkit.InvalidateFocus();
    Settle(page);
    EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
    Press(page, ImGuiKey_RightArrow);
    EXPECT_EQ(toolkit.GetMemory("cards").item, "utf8");
    const auto layout = ComputeMenuLayout(size, {false, 1.5f, 0.15f});
    EXPECT_GE(layout.position.x, size.x * 0.15f - 0.01f);
    EXPECT_GE(layout.position.y, size.y * 0.15f - 0.01f);
    EXPECT_LE(layout.position.x + layout.size.x, size.x * 0.85f + 0.01f);
    EXPECT_LE(layout.position.y + layout.size.y, size.y * 0.85f + 0.01f);
  }
}

TEST_F(BigPictureNavigationUITest, ShoulderPageMovementAndFocusResetKeepSelectionUsable)
{
  MenuPage page{"list", "List"};
  for (int i = 0; i < 30; ++i)
    page.items.push_back({std::to_string(i), "Item " + std::to_string(i)});
  Settle(page);
  Press(page, ImGuiKey_GamepadR1);
  EXPECT_GT(std::stoi(toolkit.GetMemory("list").item), 0);
  Press(page, ImGuiKey_GamepadL1);
  EXPECT_EQ(toolkit.GetMemory("list").item, "0");
  ImGui::GetIO().ClearInputKeys();
  ImGui::SetWindowFocus(nullptr);
  toolkit.InvalidateFocus();
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "0");
}

TEST_F(BigPictureNavigationUITest, DiagnosticFontScalingDoesNotScaleMenuTextAgain)
{
  const MenuPage page{"scale", "Menu", {}, {{"item", "A readable label"}}};
  Settle(page);
  std::vector<ImVec2> vertices;
  for (const auto* commands : ImGui::GetDrawData()->CmdLists)
    for (const auto& vertex : commands->VtxBuffer)
      vertices.push_back(vertex.pos);
  ImGui::GetStyle().FontScaleMain = 2.0f;
  ImGui::GetStyle().FontScaleDpi = 1.5f;
  Settle(page);
  ASSERT_EQ(ImGui::GetDrawData()->TotalVtxCount, static_cast<int>(vertices.size()));
  std::size_t i = 0;
  for (const auto* commands : ImGui::GetDrawData()->CmdLists)
  {
    for (const auto& vertex : commands->VtxBuffer)
    {
      EXPECT_NEAR(vertex.pos.x, vertices[i].x, 0.5f);
      EXPECT_NEAR(vertex.pos.y, vertices[i].y, 0.5f);
      ++i;
    }
  }
}

TEST_F(BigPictureNavigationUITest, ResizingListRestoresSelectionAndLogicalScroll)
{
  MenuPage page{"resize", "Menu"};
  for (int i = 0; i < 30; ++i)
    page.items.push_back({std::to_string(i), "Item " + std::to_string(i)});
  Settle(page);
  Press(page, ImGuiKey_GamepadR1);
  Press(page, ImGuiKey_GamepadR1);
  Settle(page);
  const auto before = toolkit.GetMemory("resize");
  ASSERT_GT(before.scroll, 0);
  ImGui::GetIO().DisplaySize = ImVec2(3840, 2160);
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("resize").item, before.item);
  EXPECT_NEAR(toolkit.GetMemory("resize").scroll, before.scroll, 2.0f);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, before.item);
}
}  // namespace
