// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "VideoCommon/BigPicture/InGameMenu.h"
#include "VideoCommon/BigPicture/Navigation.h"
#include "VideoCommon/BigPicture/Style.h"

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
    io.ConfigNavCursorVisibleAlways = false;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_RendererHasTextures;
    io.KeyRepeatDelay = 0.3f;
    io.KeyRepeatRate = 0.1f;
    io.Fonts->AddFontDefault();
  }
  void TearDown() override { ImGui::DestroyContext(context); }

  MenuResult Frame(const MenuPage& page, ::BigPicture::NavigationOptions options = {})
  {
    ImGui::NewFrame();
    auto result = toolkit.Draw(page, options, "Select / Back");
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
    if (pressed.activated || pressed.back || pressed.fullscreen)
      return pressed;
    return released.activated || released.back || released.fullscreen ? released : settled;
  }
  ImGuiContext* context = nullptr;
  NavigationToolkit toolkit;
};

TEST_F(BigPictureNavigationUITest, GroupedDiscPickerCanSelectSecondDiscAndCancelWithGamepadOnly)
{
  using namespace ::BigPicture;
  Snapshot snapshot;
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Home, Screen::Library, Screen::FilePicker};
  auto library = std::make_shared<LibrarySnapshot>();
  library->picker_boot_disc = true;
  library->picker_revision = 12;
  library->files = {{"disc1.iso", "Disc 1"}, {"disc2.iso", "Disc 2"}};
  snapshot.host.library = library;
  const auto menu = BuildInGameMenu(snapshot, {}, {});
  Settle(menu.page);
  EXPECT_EQ(toolkit.GetMemory(menu.page.id).item, "file:disc1.iso");
  Press(menu.page, ImGuiKey_GamepadDpadDown);
  EXPECT_EQ(toolkit.GetMemory(menu.page.id).item, "file:disc2.iso");
  const auto choice = menu.Activate(Press(menu.page, ImGuiKey_GamepadFaceDown));
  ASSERT_TRUE(choice);
  EXPECT_EQ(choice->action, Action::Boot);
  EXPECT_EQ(choice->path, "disc2.iso");
  EXPECT_EQ(choice->library_revision, 12u);
  const auto cancel = menu.Activate(Press(menu.page, ImGuiKey_GamepadFaceRight));
  ASSERT_TRUE(cancel);
  EXPECT_EQ(cancel->action, Action::Back);
}

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

TEST_F(BigPictureNavigationUITest, EscapeOnPauseRequestsResumeAndSubmenuEscapeRequestsBack)
{
  ::BigPicture::Snapshot snapshot;
  snapshot.mode = ::BigPicture::Mode::InGame;
  snapshot.host.lifecycle = ::BigPicture::Lifecycle::Paused;
  snapshot.screens = {::BigPicture::Screen::Pause};
  auto menu = BuildInGameMenu(snapshot, {}, {});
  Settle(menu.page);
  const auto resume = menu.Activate(Press(menu.page, ImGuiKey_Escape));
  ASSERT_TRUE(resume);
  EXPECT_EQ(resume->action, ::BigPicture::Action::Resume);
  snapshot.screens.push_back(::BigPicture::Screen::SaveStates);
  menu = BuildInGameMenu(snapshot, {}, {});
  Settle(menu.page);
  const auto back = menu.Activate(Press(menu.page, ImGuiKey_Escape));
  ASSERT_TRUE(back);
  EXPECT_EQ(back->action, ::BigPicture::Action::Back);
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

TEST_F(BigPictureNavigationUITest, F11IsAvailableAcrossEveryPresentationAndDialog)
{
  for (const auto presentation :
       {MenuPresentation::List, MenuPresentation::Landing, MenuPresentation::Pause,
        MenuPresentation::Library, MenuPresentation::Launch, MenuPresentation::FilePicker,
        MenuPresentation::Settings})
  {
    toolkit.Reset();
    MenuPage page{presentation == MenuPresentation::Landing ? "home" : "shortcut",
                  "Title",
                  {},
                  {{"one", "One"}, {"two", "Two"}},
                  "one"};
    page.presentation = presentation;
    Settle(page);
    const auto before = toolkit.GetMemory(page.id).item;
    const auto result = Press(page, ImGuiKey_F11);
    EXPECT_TRUE(result.fullscreen) << static_cast<int>(presentation);
    EXPECT_FALSE(result.activated);
    EXPECT_FALSE(result.back);
    EXPECT_EQ(toolkit.GetMemory(page.id).item, before);
  }
  MenuPage dialog{"confirmation",
                  "Exit Dolphin",
                  "Exit Dolphin?",
                  {{"confirm", "Confirm"}, {"cancel", "Cancel"}},
                  "cancel",
                  1,
                  true};
  Settle(dialog);
  EXPECT_TRUE(Press(dialog, ImGuiKey_F11).fullscreen);
  EXPECT_EQ(toolkit.GetMemory(dialog.id).item, "cancel");
}

TEST_F(BigPictureNavigationUITest, DialogUsesCompactGeometryAndResetsOnlyForANewConfirmation)
{
  MenuPage page{"confirmation",
                "Exit Dolphin",
                "Exit Dolphin?",
                {{"confirm", "Confirm"}, {"cancel", "Cancel"}},
                "cancel",
                1,
                true};
  page.focus_revision = 41;
  Settle(page);
  const auto* dialog = ImGui::FindWindowByName("###BigPictureDialog");
  ASSERT_NE(dialog, nullptr);
  EXPECT_FLOAT_EQ(dialog->Size.x, 600);
  EXPECT_LT(dialog->Size.y, 320);
  EXPECT_GT(dialog->Pos.x, 0);
  EXPECT_GT(dialog->Pos.y, 0);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "cancel");
  Press(page, ImGuiKey_UpArrow);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "confirm");
  EXPECT_TRUE(Press(page, ImGuiKey_F11).fullscreen);
  ImGui::GetIO().DisplaySize = ImVec2(1920, 1080);
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "confirm");
  ++page.focus_revision;
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "cancel");
}

TEST_F(BigPictureNavigationUITest, DialogFitsSafeAreaWithLargeTextAndRetainsCancel)
{
  MenuPage page{"long-dialog",
                "Exit Dolphin",
                "A translated confirmation message.\n日本語の確認文。",
                {{"confirm", "Confirm"}, {"cancel", "Cancel and return to the previous screen"}},
                "cancel",
                1,
                true};
  for (const auto size : {ImVec2(1280, 720), ImVec2(3840, 2160)})
  {
    ImGui::GetIO().DisplaySize = size;
    toolkit.Reset();
    for (int i = 0; i < 6; ++i)
      Frame(page, {false, 1.5f, 0.15f});
    const auto* dialog = ImGui::FindWindowByName("###BigPictureDialog");
    ASSERT_NE(dialog, nullptr);
    EXPECT_GE(dialog->Pos.x, size.x * 0.15f);
    EXPECT_GE(dialog->Pos.y, size.y * 0.15f);
    EXPECT_LE(dialog->Pos.x + dialog->Size.x, size.x * 0.85f);
    EXPECT_LE(dialog->Pos.y + dialog->Size.y, size.y * 0.85f);
    EXPECT_EQ(toolkit.GetMemory(page.id).item, "cancel");
  }
}

TEST_F(BigPictureNavigationUITest, ListLoopsBothDirectionsWithKeyboardAndGamepad)
{
  MenuPage page{"pause", "Pause"};
  page.items.push_back({"disabled-start", "Unavailable", {}, false});
  for (int i = 0; i < 24; ++i)
    page.items.push_back({std::to_string(i), "Item " + std::to_string(i)});
  page.items.push_back({"disabled-end", "Unavailable", {}, false});
  Settle(page);
  for (const auto [up, down, confirm] :
       {std::array{ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_Enter},
        std::array{ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadFaceDown}})
  {
    EXPECT_EQ(toolkit.GetMemory("pause").item, "0");
    Press(page, up);
    Settle(page);
    EXPECT_EQ(toolkit.GetMemory("pause").item, "23");
    EXPECT_GT(toolkit.GetMemory("pause").scroll, 0);
    EXPECT_EQ(Press(page, confirm).activated, "23");
    Press(page, down);
    Settle(page);
    EXPECT_EQ(toolkit.GetMemory("pause").item, "0");
    EXPECT_EQ(Press(page, confirm).activated, "0");
    // The first enabled row follows a disabled row; it must be back in view.
    EXPECT_LT(toolkit.GetMemory("pause").scroll, 100);
  }
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
TEST_F(BigPictureNavigationUITest, LibraryToolbarAndGridAreBothControllerAccessible)
{
  MenuPage page{"library", "Library", {}, {{"one", "One"}, {"two", "Two"}}, "refresh", 2};
  page.toolbar = {{"options", "Options"}, {"refresh", "Refresh"}};
  page.artwork = true;
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("library").item, "refresh");
  EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "refresh");
  Press(page, ImGuiKey_GamepadDpadDown);
  EXPECT_TRUE(toolkit.GetMemory("library").item == "one" ||
              toolkit.GetMemory("library").item == "two");
  Press(page, ImGuiKey_GamepadDpadUp);
  EXPECT_TRUE(toolkit.GetMemory("library").item == "options" ||
              toolkit.GetMemory("library").item == "refresh");
}

TEST_F(BigPictureNavigationUITest, RemovingSelectedGameUsesSafeToolbarFocusInsteadOfAnotherGame)
{
  MenuPage page{"library", "Library", {}, {{"one", "One"}, {"two", "Two"}}, "two"};
  page.presentation = MenuPresentation::Library;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back}};
  Settle(page);
  ASSERT_EQ(toolkit.GetMemory("library").item, "two");
  page.items.pop_back();
  page.default_focus = "back";
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("library").item, "back");
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "back");
}
TEST_F(BigPictureNavigationUITest, KeyboardShoulderPagingMovesByVisibleGridRows)
{
  MenuPage page{"keyboard", "Keyboard"};
  page.columns = 6;
  page.compact = true;
  for (int i = 0; i < 80; ++i)
    page.items.push_back({std::to_string(i), "Key"});
  Settle(page);
  Press(page, ImGuiKey_GamepadR1);
  EXPECT_GE(std::stoi(toolkit.GetMemory("keyboard").item), 24);
  Press(page, ImGuiKey_GamepadL1);
  EXPECT_EQ(toolkit.GetMemory("keyboard").item, "0");
}
TEST_F(BigPictureNavigationUITest, LargeLibraryDoesNotGenerateGeometryForOffscreenCards)
{
  MenuPage page{"large-library", "Library"};
  page.default_focus = "4999";
  page.presentation = MenuPresentation::Library;
  page.columns = 4;
  page.artwork = true;
  for (int i = 0; i < 5000; ++i)
    page.items.push_back({std::to_string(i), "Library game", "GameCube | NTSC-U"});
  Settle(page);
  ASSERT_NE(ImGui::GetDrawData(), nullptr);
  EXPECT_LT(ImGui::GetDrawData()->TotalVtxCount, 10000);
  EXPECT_LT(ImGui::GetDrawData()->TotalIdxCount, 20000);
  EXPECT_EQ(toolkit.GetMemory("large-library").item, "4999");
  EXPECT_GT(toolkit.GetMemory("large-library").scroll, 0);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "4999");
}

TEST_F(BigPictureNavigationUITest, LibraryDetailsDoNotTakeFocusFromListOrToolbar)
{
  MenuPage page{"library-details",
                "Library",
                {},
                {{"one", "First game", "GameCube"}, {"two", "Second game", "Wii"}},
                "one"};
  page.presentation = MenuPresentation::Library;
  page.artwork = true;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back},
                  {"grid-view", "Grid", {}, true, {}, false, MenuIcon::Grid},
                  {"list-view", "List", {}, true, {}, true, MenuIcon::List}};
  for (const auto size : {ImVec2(1280, 720), ImVec2(3840, 2160), ImVec2(3440, 1440)})
  {
    ImGui::GetIO().DisplaySize = size;
    toolkit.Reset();
    Settle(page);
    Press(page, ImGuiKey_GamepadDpadDown);
    EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "two");
    Press(page, ImGuiKey_GamepadDpadUp);
    Press(page, ImGuiKey_GamepadDpadUp);
    EXPECT_EQ(toolkit.GetMemory(page.id).item, "back");
  }
}

TEST_F(BigPictureNavigationUITest, SettingsTabsStayFixedWhenRowsHaveNoOptions)
{
  MenuPage page{"settings-with-options", "Interface Settings", {}, {{"one", "One"}}};
  page.presentation = MenuPresentation::Settings;
  page.items[0].has_options = true;
  page.context = {"options", "Options"};
  page.toolbar = {{"back", "Back"}};
  for (int i = 0; i < 17; ++i)
    page.toolbar.push_back({"tab-" + std::to_string(i), "Category"});
  // Focus restored to the open category's tab.
  page.toolbar[4].active = true;
  toolkit.RestoreMemory(page.id, {"tab-3", 0, 0, "one"});
  Settle(page);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "tab-3");
  ASSERT_NE(GImGui->NavWindow, nullptr);
  const auto before = GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main];
  page.id = "settings-controllers";
  page.title = "Controllers";
  page.items[0].has_options = false;
  toolkit.RestoreMemory(page.id, {"tab-3", 0, 0, "one"});
  Settle(page);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "tab-3");
  const auto after = GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main];
  EXPECT_FLOAT_EQ(before.Min.x, after.Min.x);
  EXPECT_FLOAT_EQ(before.Max.x, after.Max.x);
  EXPECT_FLOAT_EQ(before.Min.y, after.Min.y);
  EXPECT_FLOAT_EQ(before.Max.y, after.Max.y);
}

TEST_F(BigPictureNavigationUITest, ReturningToTheTopOfSettingsRevealsTheFirstSectionHeading)
{
  MenuPage page{"settings-sections", "Audio Settings", {}, {}};
  page.presentation = MenuPresentation::Settings;
  page.toolbar = {{"back", "Back"}};
  for (int i = 0; i < 16; ++i)
  {
    MenuItem item{"row-" + std::to_string(i), "Row " + std::to_string(i)};
    item.detail = "Help text.";
    item.section = i < 8 ? "Audio Output" : "Advanced";
    page.items.push_back(item);
  }
  Settle(page);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "row-0");
  for (int i = 0; i < 15; ++i)
    Press(page, ImGuiKey_DownArrow);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "row-15");
  ASSERT_GT(toolkit.GetMemory(page.id).scroll, 0);
  // Moving up to the first row of a section reveals its heading above the row.
  for (int i = 0; i < 7; ++i)
    Press(page, ImGuiKey_UpArrow);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "row-8");
  Settle(page);
  const auto* rows = GImGui->NavWindow;
  ASSERT_NE(rows, nullptr);
  const float row_top = rows->Pos.y + rows->NavRectRel[ImGuiNavLayer_Main].Min.y;
  // The heading (34 px plus a 10 px gap at scale 1) sits above the row and stays visible.
  EXPECT_GE(row_top - 44, rows->InnerRect.Min.y);
  for (int i = 0; i < 8; ++i)
    Press(page, ImGuiKey_UpArrow);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "row-0");
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).scroll, 0);
}

TEST_F(BigPictureNavigationUITest, PressHeldAcrossABriefPendingCommandStillActivates)
{
  ::BigPicture::Snapshot idle;
  idle.mode = ::BigPicture::Mode::Library;
  auto pending = idle;
  pending.pending = ::BigPicture::Command{idle.generation, 1, ::BigPicture::Action::Library};
  InGameMenu menu;
  menu.page = {"list", "Title", {}, {{"one", "One"}, {"two", "Two"}}};
  menu.requests.emplace(
      "two", ::BigPicture::NavigationRequest{idle.generation, ::BigPicture::Action::Boot});
  const auto idle_menu = menu;
  Settle(menu.page);
  ASSERT_NE(GImGui->NavWindow, nullptr);
  Press(menu.page, ImGuiKey_DownArrow);
  ASSERT_EQ(toolkit.GetMemory(menu.page.id).item, "two");
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(rect.GetCenter().x, rect.GetCenter().y);
  Frame(menu.page);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  Frame(menu.page);

  // The host publishes a pending snapshot that disables every item while the button is held.
  PendingMenuHold hold;
  const auto now = PendingMenuHold::Clock::now();
  hold.Replace(&idle, menu, pending, now);
  InGameMenu disabled;
  disabled.page = {"list", "Title", {}, {{"one", "One", {}, false}, {"two", "Two", {}, false}}};
  const auto& visible = hold.Visible(pending, disabled, now);
  EXPECT_FALSE(hold.Activate(pending, visible, Frame(visible.page)));
  // The host finishes before the release.
  hold.Replace(&pending, disabled, idle, now);
  menu = idle_menu;
  Frame(menu.page);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  std::optional<::BigPicture::NavigationRequest> request;
  for (int i = 0; i < 2 && !request; ++i)
    request = hold.Activate(idle, menu, Frame(menu.page));
  ASSERT_TRUE(request);
  EXPECT_EQ(request->action, ::BigPicture::Action::Boot);
  io.AddMousePosEvent(-100, -100);
}

TEST_F(BigPictureNavigationUITest, PointerAndKeyboardShareOneHighlightAndClicksHitThePointer)
{
  MenuPage page{"settings-one-highlight", "Audio", {}, {{"one", "One"}, {"two", "Two"}}};
  page.presentation = MenuPresentation::Settings;
  page.toolbar = {{"back", "Back"}, {"tab-0", "General"}, {"tab-1", "Audio"}, {"tab-2", "Wii"}};
  Settle(page);
  const auto nav_rect = [] {
    return ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                     GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  };
  Press(page, ImGuiKey_DownArrow);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "two");
  const auto two = nav_rect();
  Press(page, ImGuiKey_UpArrow);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "one");
  // After keyboard or gamepad input, the focused row is the highlighted one.
  EXPECT_TRUE(GImGui->NavCursorVisible && GImGui->NavHighlightItemUnderNav);

  // Pointing at another row moves the single highlight to it; the focused row loses it.
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(two.GetCenter().x, two.GetCenter().y);
  Frame(page);
  Frame(page);
  EXPECT_FALSE(GImGui->NavHighlightItemUnderNav);
  EXPECT_NE(GImGui->HoveredId, 0u);
  EXPECT_NE(GImGui->HoveredId, GImGui->NavId);

  // A key press hands the highlight back to keyboard focus.
  Press(page, ImGuiKey_LeftShift);
  EXPECT_TRUE(GImGui->NavCursorVisible && GImGui->NavHighlightItemUnderNav);

  // A click whose position arrives with the button activates the row under the pointer, not the
  // row that still has keyboard focus.
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "one");
  std::string activated;
  const auto take = [&](const MenuResult& result) {
    if (result.activated && activated.empty())
      activated = *result.activated;
  };
  io.AddMousePosEvent(two.GetCenter().x, two.GetCenter().y + 1);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  take(Frame(page));
  take(Frame(page));
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  take(Frame(page));
  take(Frame(page));
  EXPECT_EQ(activated, "two");

  // The same holds in the category bar: with keyboard focus on one tab, hovering another is the
  // only tab highlight.
  MenuPage tabs = page;
  tabs.id = "settings-one-highlight-tabs";
  tabs.toolbar[2].active = true;
  toolkit.RestoreMemory(tabs.id, {"tab-1", 0, 0, "one"});
  Settle(tabs);
  Press(tabs, ImGuiKey_LeftShift);
  ASSERT_EQ(toolkit.GetMemory(tabs.id).item, "tab-1");
  EXPECT_TRUE(GImGui->NavHighlightItemUnderNav);
  const auto focused_tab = nav_rect();
  const auto focused_id = GImGui->NavId;
  io.AddMousePosEvent(focused_tab.GetCenter().x + focused_tab.GetWidth(),
                      focused_tab.GetCenter().y);
  Frame(tabs);
  Frame(tabs);
  EXPECT_FALSE(GImGui->NavHighlightItemUnderNav);
  EXPECT_NE(GImGui->HoveredId, 0u);
  EXPECT_NE(GImGui->HoveredId, focused_id);
  io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  Frame(tabs);
}

TEST_F(BigPictureNavigationUITest, SettingsTabsMarkOneTabWithAnUnderlineAndNoHoverSquare)
{
  MenuPage page{"settings-underline", "Audio", {}, {{"one", "One"}}};
  page.presentation = MenuPresentation::Settings;
  page.toolbar = {{"back", "Back"}, {"tab-0", "General"}, {"tab-1", "Audio"}, {"tab-2", "Wii"}};
  page.toolbar[1].active = true;
  const auto& palette = Style::GetPalette(page.theme);
  const float header = Style::BAR_HEIGHT;
  // Left edges of underlines, and whether any hover-coloured fill is drawn, in the header bar.
  const auto header_marks = [&] {
    const auto* window = ImGui::FindWindowByName("###BigPictureSettings");
    std::set<float> underlines;
    bool square = false;
    const auto underline = ImGui::GetColorU32(palette.header_text);
    const auto hover = ImGui::GetColorU32(palette.hover);
    for (const auto& vertex : window->DrawList->VtxBuffer)
    {
      if (vertex.pos.y == header - 2 && vertex.col == underline)
        underlines.insert(vertex.pos.x);
      square |= vertex.pos.y < header && vertex.col == hover;
    }
    EXPECT_FALSE(square);
    // One underline: a quad with two vertices on its top edge. Returns its left edge.
    EXPECT_EQ(underlines.size(), 2u);
    return underlines.empty() ? 0.0f : *underlines.begin();
  };
  Settle(page);
  // Keyboard focus is on a row: the open category carries the only underline.
  const float open = header_marks();

  // Keyboard focus on a tab moves the underline to it.
  Press(page, ImGuiKey_UpArrow);
  const auto focused = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                                 GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_NEAR(header_marks(), focused.Min.x + 10, 1);

  // So does pointing at a tab; no hover square is drawn.
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(1280 - 30, header / 2);
  Frame(page);
  Frame(page);
  ASSERT_NE(GImGui->HoveredId, 0u);
  const float pointed = header_marks();
  EXPECT_NE(pointed, open);
  EXPECT_GT(pointed, 1280 - 70);
  io.AddMousePosEvent(focused.GetCenter().x, focused.GetCenter().y);
  Frame(page);
  Frame(page);
  EXPECT_NEAR(header_marks(), focused.Min.x + 10, 1);
  io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  Frame(page);
}

TEST_F(BigPictureNavigationUITest, ACategoryClickedEarlierIsNotRestoredAsThePageFocus)
{
  const auto category = [](const std::string& id, int active) {
    MenuPage page{id, id, {}, {{id + "-row", "Row"}}};
    page.presentation = MenuPresentation::Settings;
    page.toolbar = {{"back", "Back"}, {"tab-0", "Interface"}, {"tab-1", "Audio"}};
    page.toolbar[active + 1].active = true;
    return page;
  };
  auto interface = category("interface", 0);
  auto audio = category("audio", 1);
  Settle(interface);

  // Click the Audio tab: the click focuses that tab on the Interface page before it closes.
  Press(interface, ImGuiKey_UpArrow);
  const auto tab = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                             GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  auto& io = ImGui::GetIO();
  const ImVec2 audio_tab(1280 - 8 - 56 * 0.5f, tab.GetCenter().y);
  io.AddMousePosEvent(audio_tab.x, audio_tab.y);
  Frame(interface);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  Frame(interface);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  std::string activated;
  for (int i = 0; i < 2 && activated.empty(); ++i)
    activated = Frame(interface).activated.value_or("");
  ASSERT_EQ(activated, "tab-1");
  ASSERT_EQ(toolkit.GetMemory(interface.id).item, "tab-1");
  Settle(audio);

  // Changing category with the keyboard returns to the Interface page's rows, so the only tab
  // marked is the open category, from the page's first frame on. (Refocusing the window briefly
  // restores ImGui's last focused header button, the Audio tab clicked earlier.)
  const auto underlines = [] {
    const auto* window = ImGui::FindWindowByName("###BigPictureSettings");
    const auto color = ImGui::GetColorU32(Style::GetPalette("").header_text);
    std::set<float> left;
    for (const auto& vertex : window->DrawList->VtxBuffer)
    {
      if (vertex.pos.y == Style::BAR_HEIGHT - 2 && vertex.col == color)
        left.insert(vertex.pos.x);
    }
    return left;
  };
  // Two tabs 56 px wide end 8 px from the right edge; each underline is inset by 10 px.
  const float interface_tab = 1280 - 8 - 2 * 56 + 10;
  EXPECT_EQ(Press(audio, ImGuiKey_LeftArrow).activated, "tab-0");
  for (int i = 0; i < 4; ++i)
  {
    Frame(interface);
    EXPECT_EQ(underlines(), (std::set<float>{interface_tab, interface_tab + 56 - 20})) << i;
  }
  EXPECT_EQ(toolkit.GetMemory(interface.id).item, "interface-row");
  io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

TEST_F(BigPictureNavigationUITest, SettingValuesWrapAfterSeparatorsAndKeepTheirEnd)
{
  ImGui::NewFrame();
  const auto width = [](const std::string& text) { return ImGui::CalcTextSize(text.c_str()).x; };
  const auto joined = [](const std::vector<std::string>& lines) {
    std::string text;
    for (const auto& line : lines)
      text += line;
    return text;
  };
  const std::string preset = "lola-retro/flat/consumer-tv-composite";
  const float narrow = width("consumer-tv-composite") + 1;
  auto lines = WrapSettingValue(preset, narrow);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(joined(lines), preset);
  EXPECT_TRUE(lines[0].ends_with('/') || lines[0].ends_with('-')) << lines[0];
  for (const auto& line : lines)
    EXPECT_LE(width(line), narrow) << line;

  // A value that fits stays on one line.
  EXPECT_EQ(WrapSettingValue(preset, width(preset) + 1), std::vector{preset});

  // Too long for two lines: the start is dropped and the end kept.
  const std::string path = "C:/Users/someone/Documents/Dolphin Emulator/Shaders/presets/"
                           "crt/guest-advanced/crt-guest-advanced-ntsc.slangp";
  const float small = width("crt-guest-advanced-ntsc.slangp") + 4;
  lines = WrapSettingValue(path, small);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_TRUE(lines[0].starts_with("...")) << lines[0];
  EXPECT_TRUE(lines[1].ends_with("crt-guest-advanced-ntsc.slangp")) << lines[1];
  for (const auto& line : lines)
    EXPECT_LE(width(line), small) << line;

  // A long word without separators still breaks where it must.
  lines = WrapSettingValue(std::string(40, 'w'), width(std::string(25, 'w')));
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(joined(lines), std::string(40, 'w'));
  ImGui::EndFrame();
}

TEST_F(BigPictureNavigationUITest, OpeningAPageShowsFocusOnlyAfterKeyboardOrGamepadInput)
{
  MenuPage first{"first", "First", {}, {{"one", "One"}, {"two", "Two"}}};
  Settle(first);
  EXPECT_TRUE(GImGui->NavCursorVisible && GImGui->NavHighlightItemUnderNav);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(5, 5);
  Frame(first);
  io.AddMousePosEvent(9, 9);
  Frame(first);
  MenuPage second{"second", "Second", {}, {{"three", "Three"}}};
  Settle(second);
  EXPECT_FALSE(GImGui->NavHighlightItemUnderNav);
  Press(second, ImGuiKey_GamepadDpadDown);
  EXPECT_TRUE(GImGui->NavCursorVisible && GImGui->NavHighlightItemUnderNav);
  io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

TEST_F(BigPictureNavigationUITest, MouseWheelScrollsSettingsRows)
{
  MenuPage page{"settings-wheel", "Audio", {}, {}};
  page.presentation = MenuPresentation::Settings;
  for (int i = 0; i < 30; ++i)
    page.items.push_back({"row-" + std::to_string(i), "Row " + std::to_string(i)});
  Settle(page);
  ASSERT_EQ(toolkit.GetMemory(page.id).scroll, 0);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(640, 360);
  Frame(page);
  io.AddMouseWheelEvent(0, -3);
  Frame(page);
  Frame(page);
  EXPECT_GT(toolkit.GetMemory(page.id).scroll, 0);
  io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

TEST_F(BigPictureNavigationUITest, LongSettingsHelpLeavesRoomForOtherRows)
{
  MenuPage page{"settings-long-help", "Graphics", {}, {{"one", "One"}, {"two", "Two"}}};
  page.presentation = MenuPresentation::Settings;
  page.items[0].detail = std::string(1000, 'x') + "\n" + std::string(1000, 'y');
  page.items[0].value = std::string(4096, 'z');
  Settle(page);
  ASSERT_NE(GImGui->NavWindow, nullptr);
  const auto selected = GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main];
  EXPECT_LT(selected.GetHeight(), 200);
  Press(page, ImGuiKey_DownArrow);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "two");
}

TEST(BigPictureIconsTest, SystemIconsMatchPCSX2PromptFontCodepoints)
{
  EXPECT_EQ(GetMenuIconCodepoint(MenuIcon::CPU), 0x235eu);
  EXPECT_EQ(GetMenuIconCodepoint(MenuIcon::Controller), 0x2360u);
  EXPECT_EQ(GetMenuIconCodepoint(MenuIcon::Keyboard), 0x2361u);
  EXPECT_EQ(GetMenuIconCodepoint(MenuIcon::Memory), 0x2364u);
  EXPECT_EQ(GetMenuIconCodepoint(MenuIcon::Audio), 0x2365u);
  EXPECT_EQ(GetMenuIconCodepoint(MenuIcon::Image), 0x2366u);
}

TEST(BigPictureFontsTest, RichPresenceDecodesSupplementaryUnicodeWithoutReplacement)
{
  static_assert(sizeof(ImWchar) == 4);
  unsigned int codepoint = 0;
  EXPECT_EQ(ImTextCharFromUtf8(&codepoint, "\xf0\x9f\x8c\x8a", nullptr), 4);
  EXPECT_EQ(codepoint, 0x1f30au);
}

TEST_F(BigPictureNavigationUITest, SettingsSummaryBoundsIncludeExplicitParagraphBreaks)
{
  MenuPage page{"settings-paragraphs", "Settings", {}, {{"one", "One"}, {"two", "Two"}}};
  page.presentation = MenuPresentation::Settings;
  for (int i = 0; i < 30; ++i)
    page.items[0].detail += "A short paragraph.\n\n";
  Settle(page);
  ASSERT_NE(GImGui->NavWindow, nullptr);
  EXPECT_LT(GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main].GetHeight(), 140);
  Press(page, ImGuiKey_DownArrow);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "two");
}

TEST_F(BigPictureNavigationUITest, FullscreenStylesDoNotLeakIntoDiagnosticOverlays)
{
  const auto original = ImGui::GetStyle();
  MenuPage page{"style", "Menu", {}, {{"one", "One", "Summary"}}};
  page.toolbar = {{"tab", "Current category", {}, true, {}, true}};
  Settle(page);
  const auto& after = ImGui::GetStyle();
  EXPECT_EQ(after.Alpha, original.Alpha);
  EXPECT_EQ(after.WindowPadding.x, original.WindowPadding.x);
  EXPECT_EQ(after.ItemSpacing.y, original.ItemSpacing.y);
  EXPECT_EQ(after.FrameRounding, original.FrameRounding);
  for (int i = 0; i < ImGuiCol_COUNT; ++i)
  {
    EXPECT_EQ(after.Colors[i].x, original.Colors[i].x);
    EXPECT_EQ(after.Colors[i].y, original.Colors[i].y);
    EXPECT_EQ(after.Colors[i].z, original.Colors[i].z);
    EXPECT_EQ(after.Colors[i].w, original.Colors[i].w);
  }
}

TEST_F(BigPictureNavigationUITest, LiveThemeChangesPreserveSelectionAndActivation)
{
  MenuPage page{"themes", "Appearance", {}, {{"first", "First"}, {"second", "Second"}}};
  page.items[0].icon = MenuIcon::Theme;
  page.items[1].icon = MenuIcon::Settings;
  Settle(page);
  Press(page, ImGuiKey_GamepadDpadDown);
  for (const auto& choice : ::BigPicture::THEME_CHOICES)
  {
    page.theme = choice.id;
    Settle(page);
    EXPECT_EQ(toolkit.GetMemory(page.id).item, "second");
    EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "second");
  }
  page.theme = "FutureTheme";
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "second");
}

TEST_F(BigPictureNavigationUITest, CardsRemainRevealableAtMaximumScaleAndSafeArea)
{
  MenuPage page{"overscan",       "Library",
                "Selected games", {{"game", "A long translated game title", "GameCube | NTSC-U"}},
                "game",           4};
  page.artwork = true;
  page.toolbar = {{"search", "Search"},          {"filters", "View and filters"},
                  {"view", "List view"},         {"settings", "Settings"},
                  {"folder", "Add game folder"}, {"refresh", "Refresh"}};
  for (const auto size : {ImVec2(1280, 720), ImVec2(3840, 2160)})
  {
    ImGui::GetIO().DisplaySize = size;
    toolkit.Reset();
    for (int frame = 0; frame < 6; ++frame)
      Frame(page, {false, 1.5f, 0.15f});
    ASSERT_NE(GImGui->NavWindow, nullptr);
    auto* window = GImGui->NavWindow;
    const auto selected = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
    EXPECT_LE(selected.GetHeight(), window->InnerRect.GetHeight() + 2);
    EXPECT_GE(selected.Min.y, window->InnerRect.Min.y - 2);
    EXPECT_LE(selected.Max.y, window->InnerRect.Max.y + 2);
  }
}

TEST_F(BigPictureNavigationUITest, PauseActionsFitAt720pAndRemainNavigableWithOverscan)
{
  MenuPage page{"root", "Pause"};
  page.presentation = MenuPresentation::Pause;
  page.overlay = true;
  page.context = {"game", "A very long running game title that must not cover the clock",
                  "game.rvz\nGMSE01 | Revision 0"};
  page.play_time = "This Session: 0h 12m 3s\nAll Time: 22h 0m 0s";
  for (int i = 0; i < 9; ++i)
  {
    page.items.push_back({std::to_string(i), "Action " + std::to_string(i)});
    page.items.back().icon = MenuIcon::Play;
  }
  Settle(page);
  auto* window = GImGui->NavWindow;
  ASSERT_NE(window, nullptr);
  EXPECT_EQ(window->ScrollMax.y, 0);
  for (int i = 0; i < 8; ++i)
    Press(page, ImGuiKey_DownArrow);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "8");
  EXPECT_TRUE(Press(page, ImGuiKey_Escape).back);
  for (const auto size : {ImVec2(1280, 720), ImVec2(3840, 2160), ImVec2(3440, 1440)})
  {
    ImGui::GetIO().DisplaySize = size;
    toolkit.InvalidateFocus();
    for (int frame = 0; frame < 6; ++frame)
      Frame(page, {false, 1.5f, 0.15f});
    window = GImGui->NavWindow;
    ASSERT_NE(window, nullptr);
    const auto selected = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
    EXPECT_GE(selected.Min.y, window->InnerRect.Min.y - 2);
    EXPECT_LE(selected.Max.y, window->InnerRect.Max.y + 2);
  }
}

TEST(BigPicturePromptsTest, PausePromptsUseReturnToGameAndRespectConfirmBackSwap)
{
  ::BigPicture::NavigationStatus status;
  status.device = "Xbox";
  status.device_source = "XInput";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Confirm)] = "Button A";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Back)] = "Button B";
  ::BigPicture::NavigationText text;
  text.select = "Select";
  text.return_to_game = "Return to Game";
  text.previous = "Previous Page";
  text.next = "Next Page";
  const auto normal = GetControllerPrompts(status, text, false, true);
  EXPECT_NE(normal.find("Return to Game"), std::string::npos);
  EXPECT_EQ(normal.find("Previous Page"), std::string::npos);
  EXPECT_EQ(normal.find("Next Page"), std::string::npos);
  status.options.swap_confirm_back = true;
  EXPECT_NE(GetControllerPrompts(status, text, false, true), normal);
}

TEST_F(BigPictureNavigationUITest, SharedFooterStaysThinAsSafeAreaChanges)
{
  MenuPage page{"footer", "Menu", {}, {{"resume", "Resume Game"}}};
  for (const auto presentation : {MenuPresentation::Pause, MenuPresentation::List})
  {
    page.presentation = presentation;
    for (const float safe_area : {0.0f, 0.05f, 0.15f})
    {
      for (int frame = 0; frame < 4; ++frame)
        Frame(page, {false, 1.0f, safe_area});
      const auto* footer = ImGui::GetForegroundDrawList();
      ASSERT_GE(footer->VtxBuffer.Size, 4);
      EXPECT_FLOAT_EQ(footer->VtxBuffer[0].pos.x, 0);
      EXPECT_FLOAT_EQ(footer->VtxBuffer[0].pos.y, 720 - Style::FOOTER_HEIGHT);
      EXPECT_FLOAT_EQ(footer->VtxBuffer[2].pos.x, 1280);
      EXPECT_FLOAT_EQ(footer->VtxBuffer[2].pos.y, 720);
    }
  }
}

TEST_F(BigPictureNavigationUITest, ArrivingAchievementMetadataKeepsTheSelectedPauseActionVisible)
{
  MenuPage page{"pause-metadata", "Pause"};
  page.presentation = MenuPresentation::Pause;
  for (int i = 0; i < 9; ++i)
    page.items.push_back({std::to_string(i), "Action " + std::to_string(i)});
  Settle(page);
  Press(page, ImGuiKey_UpArrow);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "8");
  page.rich_presence = "Exploring Delfino Plaza";
  page.achievement_progress = "3/50 achievements | 15/500 points";
  for (int frame = 0; frame < 6; ++frame)
    Frame(page);
  auto* window = GImGui->NavWindow;
  ASSERT_NE(window, nullptr);
  const auto selected = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_GE(selected.Min.y, window->InnerRect.Min.y - 2);
  EXPECT_LE(selected.Max.y, window->InnerRect.Max.y + 2);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "8");
}

TEST(BigPictureThemeTest, EveryThemeKeepsNormalHeaderAndSelectionTextReadable)
{
  const auto luminance = [](ImVec4 color) {
    const auto linear = [](float c) {
      return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * linear(color.x) + 0.7152f * linear(color.y) + 0.0722f * linear(color.z);
  };
  const auto contrast = [&](ImVec4 a, ImVec4 b) {
    const float x = luminance(a), y = luminance(b);
    return (std::max(x, y) + 0.05f) / (std::min(x, y) + 0.05f);
  };
  for (const auto& choice : ::BigPicture::THEME_CHOICES)
  {
    SCOPED_TRACE(choice.id);
    const auto& palette = Style::GetPalette(choice.id);
    EXPECT_GE(contrast(palette.text, palette.background), 4.5f);
    EXPECT_GE(contrast(palette.secondary, palette.background), 4.5f);
    EXPECT_GE(contrast(palette.text, palette.panel), 4.5f);
    EXPECT_GE(contrast(palette.header_text, palette.header), 4.5f);
    EXPECT_GE(contrast(palette.card_secondary, palette.header), 4.5f);
    EXPECT_GE(contrast(palette.selected_text, palette.selected), 4.5f);
  }
  EXPECT_FLOAT_EQ(Style::GetPalette("FutureTheme").background.x, Style::DARK.background.x);
}
TEST_F(BigPictureNavigationUITest, HomeNavigatesHorizontallyAndRestoresFocusAfterAChild)
{
  MenuPage home{
      "home",
      "Dolphin",
      {},
      {{"games", "Game List"}, {"start", "Start Game"}, {"settings", "Settings"}, {"exit", "Exit"}},
      "games",
      4};
  home.presentation = MenuPresentation::Landing;
  Settle(home);
  Press(home, ImGuiKey_RightArrow);
  EXPECT_EQ(toolkit.GetMemory("home").item, "start");
  EXPECT_EQ(Press(home, ImGuiKey_Enter).activated, "start");
  MenuPage child{"start", "Start Game", {}, {{"file", "Open File"}}};
  Settle(child);
  Settle(home);
  EXPECT_EQ(toolkit.GetMemory("home").item, "start");
  Press(home, ImGuiKey_GamepadDpadRight);
  EXPECT_EQ(Press(home, ImGuiKey_GamepadFaceDown).activated, "settings");
  EXPECT_TRUE(Press(home, ImGuiKey_Escape).back);
  for (int i = 0; i < 4; ++i)
    Frame(home, {false, 1.5f, 0.15f});
  EXPECT_EQ(toolkit.GetMemory("home").item, "settings");
}
TEST_F(BigPictureNavigationUITest, SwitchingDevicesReleasesHeldKeysAndKeepsTheCurrentSelection)
{
  MenuPage home{
      "home",
      "Dolphin",
      {},
      {{"games", "Game List"}, {"start", "Start Game"}, {"settings", "Settings"}, {"exit", "Exit"}},
      "games",
      4};
  home.presentation = MenuPresentation::Landing;
  NavigationInput input;
  Settle(home);
  input.KeyEvent(ImGuiKey_LeftArrow, true, true);
  Settle(home);
  input.KeyEvent(ImGuiKey_GamepadDpadRight, true, false);
  Settle(home);
  EXPECT_FALSE(ImGui::IsKeyDown(ImGuiKey_LeftArrow));
  EXPECT_FALSE(input.IsKeyboard());
  EXPECT_EQ(toolkit.GetMemory("home").item, "start");
  input.KeyEvent(ImGuiKey_RightArrow, true, true);
  Settle(home);
  EXPECT_FALSE(ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight));
  EXPECT_TRUE(input.IsKeyboard());
  EXPECT_EQ(toolkit.GetMemory("home").item, "settings");
  input.KeyEvent(ImGuiKey_GamepadDpadRight, false, false);
  Settle(home);
  EXPECT_TRUE(input.IsKeyboard());  // A late release must not steal input ownership.
  input.KeyEvent(ImGuiKey_GamepadFaceDown, true, false);
  auto result = Frame(home);
  EXPECT_EQ(result.activated, "settings");
}

TEST_F(BigPictureNavigationUITest, HomeRecoversRetainedFocusWithoutNeedingAnotherDevice)
{
  MenuPage home{"home",  "Dolphin", {}, {{"games", "Game List"}, {"start", "Start Game"}},
                "games", 4};
  home.presentation = MenuPresentation::Landing;
  Settle(home);
  Press(home, ImGuiKey_RightArrow);
  ImGui::SetWindowFocus(nullptr);
  Settle(home);
  EXPECT_EQ(Press(home, ImGuiKey_Enter).activated, "start");
}

TEST(BigPicturePromptTest, ControllerHintsUsePhysicalGlyphsAndRespectSwappedButtons)
{
  ::BigPicture::NavigationStatus status;
  status.device = "Xbox Wireless Controller";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Confirm)] = "`Button A`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Back)] = "`Button B`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Previous)] = "`Shoulder L`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Next)] = "`Shoulder R`";
  ::BigPicture::NavigationText text;
  text.select = "Select";
  text.back = "Back";
  text.navigate = "Navigate";
  const auto normal = GetControllerPrompts(status, text, false);
  EXPECT_NE(normal.find("\xe2\x87\x93 Select"), std::string::npos);
  EXPECT_NE(normal.find("\xe2\x87\x92 Back"), std::string::npos);
  EXPECT_EQ(normal.find("Button A"), std::string::npos);
  status.options.swap_confirm_back = true;
  EXPECT_NE(GetControllerPrompts(status, text, false).find("\xe2\x87\x92 Select"),
            std::string::npos);
  EXPECT_EQ(GetBindingGlyph("`Button S`", "DualSense Wireless Controller"), "\xe2\x87\xa3");
  EXPECT_EQ(GetBindingGlyph("`Button E`", "Nintendo Switch Pro Controller"), "\xe2\x87\x93");
  EXPECT_NE(GetBindingGlyph("`Custom/0/Pad:Button 7`", "").find("Button 7"), std::string::npos);
  EXPECT_FALSE(GetBindingGlyph("", "").empty());
}
TEST(BigPicturePromptTest, DirectInputDualSenseNumbersUseSonyHidLayoutAndKeepConfiguredActions)
{
  const std::string_view device = "DualSense Wireless Controller";
  EXPECT_EQ(GetBindingGlyph("`Button 0`", device, "DInput"), "\xe2\x87\xa0");
  EXPECT_EQ(GetBindingGlyph("`Button 1`", device, "DInput"), "\xe2\x87\xa3");
  EXPECT_EQ(GetBindingGlyph("`Button 2`", device, "DInput"), "\xe2\x87\xa2");
  EXPECT_EQ(GetBindingGlyph("`Button 3`", device, "DInput"), "\xe2\x87\xa1");
  EXPECT_EQ(GetBindingGlyph("`Button 4`", device, "DInput"), "\xe2\x86\xb0");
  EXPECT_EQ(GetBindingGlyph("`Button 5`", device, "DInput"), "\xe2\x86\xb1");
  ::BigPicture::NavigationStatus status;
  status.device = device;
  status.device_source = "DInput";
  // Reproduce the saved DualSense profile: Circle selects, Cross goes back, L1/R1 page.
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Confirm)] = "`Button 2`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Back)] = "`Button 1`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Previous)] = "`Button 4`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Next)] = "`Button 5`";
  ::BigPicture::NavigationText text;
  text.select = "Select";
  text.navigate = "Navigate";
  text.home_labels = {"Game List", "Start Game", "Settings", "Exit"};
  text.actions[static_cast<std::size_t>(::BigPicture::Action::Fullscreen)] = "Toggle Fullscreen";
  const auto prompts = GetControllerPrompts(status, text, true);
  EXPECT_EQ(prompts.find("Button "), std::string::npos);
  EXPECT_NE(prompts.find("\xe2\x86\xb0 Game List"), std::string::npos);
  EXPECT_NE(prompts.find("\xe2\x86\xb1 Toggle Fullscreen"), std::string::npos);
  EXPECT_NE(prompts.find("\xe2\x87\xa2 Select"), std::string::npos);
  EXPECT_NE(prompts.find("\xe2\x87\xa3 Exit"), std::string::npos);
  status.options.swap_confirm_back = true;
  const auto swapped = GetControllerPrompts(status, text, true);
  EXPECT_NE(swapped.find("\xe2\x87\xa3 Select"), std::string::npos);
  EXPECT_NE(swapped.find("\xe2\x87\xa2 Exit"), std::string::npos);
}

TEST(BigPicturePromptTest, QualifiedButtonsUseTheirOwnBackendAndCompoundsAreNotGuessed)
{
  EXPECT_EQ(GetBindingGlyph("`DInput/0/DualSense Wireless Controller:Button 4`", "Xbox Controller",
                            "XInput"),
            "\xe2\x86\xb0");
  EXPECT_EQ(GetBindingGlyph("`SDL/0/DualSense Wireless Controller:Button S`", "Xbox Controller",
                            "XInput"),
            "\xe2\x87\xa3");
  for (const auto source : {"SDL", "XInput", ""})
    EXPECT_NE(
        GetBindingGlyph("`Button 4`", "DualSense Wireless Controller", source).find("Button 4"),
        std::string::npos);
  EXPECT_NE(GetBindingGlyph("`Button 4`", "Unknown Gamepad", "DInput").find("Button 4"),
            std::string::npos);
  EXPECT_NE(
      GetBindingGlyph("`Button 999`", "DualSense Wireless Controller", "DInput").find("Button 999"),
      std::string::npos);
  EXPECT_NE(GetBindingGlyph("`Button 4` | `Button 5`", "DualSense Wireless Controller", "DInput")
                .find("Button 5"),
            std::string::npos);
}
TEST_F(BigPictureNavigationUITest, LibraryGridMatchesReferenceGeometryAndNavigatesFourColumns)
{
  MenuPage page{"library", "Game Grid", {}, {}, "back", 4};
  page.presentation = MenuPresentation::Library;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back},
                  {"grid-view", "Grid", {}, true, {}, true, MenuIcon::Grid},
                  {"list-view", "List", {}, true, {}, false, MenuIcon::List}};
  for (int i = 0; i < 24; ++i)
    page.items.push_back({"game:" + std::to_string(i), "Game title"});
  Settle(page);
  const auto* root = ImGui::FindWindowByName("###BigPictureLibrary");
  ASSERT_NE(root, nullptr);
  EXPECT_FLOAT_EQ(root->Pos.x, 0);
  EXPECT_FLOAT_EQ(root->Pos.y, 0);
  EXPECT_FLOAT_EQ(root->Size.x, 1280);
  EXPECT_FLOAT_EQ(root->Size.y, 720);
  ASSERT_NE(GImGui->NavWindow, nullptr);
  auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                        GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_NEAR(rect.Min.x, 128, 1);
  EXPECT_NEAR(rect.Min.y, 44, 1);
  EXPECT_NEAR(rect.GetWidth(), 236, 1);
  EXPECT_NEAR(rect.GetHeight(), 316, 1);
  Press(page, ImGuiKey_RightArrow);
  EXPECT_EQ(toolkit.GetMemory("library").item, "game:1");
  Press(page, ImGuiKey_GamepadDpadDown);
  EXPECT_EQ(toolkit.GetMemory("library").item, "game:5");
  rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                   GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_LE(rect.Max.y, GImGui->NavWindow->InnerRect.Max.y + 2);
  EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "game:5");
  EXPECT_EQ(Press(page, ImGuiKey_F1).activated, "view-toggle");
  EXPECT_EQ(Press(page, ImGuiKey_F2).activated, "settings");
  EXPECT_EQ(Press(page, ImGuiKey_F3).activated, "options");
  EXPECT_EQ(Press(page, ImGuiKey_F4).activated, "search");
  EXPECT_EQ(Press(page, ImGuiKey_F5).activated, "refresh");
  EXPECT_EQ(Press(page, ImGuiKey_GamepadL1).activated, "view-toggle");
  EXPECT_EQ(Press(page, ImGuiKey_GamepadR1).activated, "options");
  EXPECT_EQ(Press(page, ImGuiKey_F6).activated, "options:game:5");
  EXPECT_EQ(Press(page, GetNavigationKey(::BigPicture::NavButton::Options, false)).activated,
            "options:game:5");
}

TEST_F(BigPictureNavigationUITest, LibraryViewChangesAndChildScreensRetainAndRevealTheSelectedGame)
{
  MenuPage page{"library", "Game Grid", {}, {}, "game:18", 4};
  page.presentation = MenuPresentation::Library;
  for (int i = 0; i < 80; ++i)
    page.items.push_back({"game:" + std::to_string(i), "Game title", "GameCube"});
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("library").item, "game:18");
  EXPECT_GT(toolkit.GetMemory("library").scroll, 0);
  page.columns = 1;
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "game:18");
  MenuPage child{"settings", "Settings", {}, {{"back", "Back"}}};
  Settle(child);
  page.columns = 4;
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "game:18");
  std::rotate(page.items.begin(), page.items.begin() + 20, page.items.end());
  ++page.focus_revision;
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "game:18");
  for (int i = 0; i < 5; ++i)
    Frame(page, {false, 1.5f, 0.15f});
  auto* window = GImGui->NavWindow;
  ASSERT_NE(window, nullptr);
  const auto rect = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_GE(rect.Min.y, window->InnerRect.Min.y - 2);
  EXPECT_LE(rect.Max.y, window->InnerRect.Max.y + 2);
  EXPECT_EQ(toolkit.GetMemory("library").item, "game:18");
}

TEST_F(BigPictureNavigationUITest, LibraryArtworkKeepsItsAspectAndOnlyLoadsVisibleCovers)
{
  MenuPage page{"library", "Game Grid", {}, {}, "0", 4};
  page.presentation = MenuPresentation::Library;
  for (int i = 0; i < 1000; ++i)
  {
    MenuItem item{std::to_string(i),
                  "An intentionally long Japanese title 日本語の長いゲームタイトル",
                  {},
                  true,
                  "cover"};
    item.image_aspect = 1.5f;
    page.items.push_back(item);
  }
  int requests = 0;
  for (int i = 0; i < 5; ++i)
  {
    requests = 0;
    ImGui::NewFrame();
    toolkit.Draw(page, {}, "Select / Back", nullptr, [&](const std::string&) {
      ++requests;
      return ImTextureID{123};
    });
    ImGui::Render();
  }
  EXPECT_LT(requests, 20);
  const auto* data = ImGui::GetDrawData();
  bool found = false;
  for (const auto* list : data->CmdLists)
    for (const auto& command : list->CmdBuffer)
      if (!command.TexRef._TexData && command.TexRef._TexID == ImTextureID{123} &&
          command.ElemCount >= 6)
      {
        const auto first =
            list->VtxBuffer[command.VtxOffset + list->IdxBuffer[command.IdxOffset]].pos;
        ImRect box(first, first);
        for (unsigned i = 0; i < 6; ++i)
          box.Add(list->VtxBuffer[command.VtxOffset + list->IdxBuffer[command.IdxOffset + i]].pos);
        EXPECT_NEAR(box.GetWidth() / box.GetHeight(), 1.5f, 0.01f);
        EXPECT_LE(box.GetWidth(), 206.1f);
        EXPECT_LE(box.GetHeight(), 272.1f);
        found = true;
      }
  EXPECT_TRUE(found);
}

TEST_F(BigPictureNavigationUITest, EmptyLibraryKeepsFolderSetupAccessible)
{
  MenuPage page{"library", "Game Grid", "Add a game folder to build your library.", {}, "back", 4};
  page.presentation = MenuPresentation::Library;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back}};
  page.context = {"add-folder", "Add Game Folder"};
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_F3).activated, "options");
  Press(page, ImGuiKey_DownArrow);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "add-folder");
}

TEST_F(BigPictureNavigationUITest, LaunchTilesNavigateAndBackLivesInTheHeader)
{
  MenuPage page{"start-game",
                "Start Game",
                {},
                {{"file", "Start File"}, {"ipl", "GameCube IPL"}, {"wii", "Wii System Menu"}},
                "file"};
  page.presentation = MenuPresentation::Launch;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back}};
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "file");
  Press(page, ImGuiKey_GamepadDpadRight);
  EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "ipl");
  Press(page, ImGuiKey_UpArrow);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "back");
  ASSERT_NE(GImGui->NavWindow, nullptr);
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_LE(rect.Max.y, 44);
  EXPECT_LE(rect.Max.x, 54);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "back");
  EXPECT_TRUE(Press(page, ImGuiKey_Escape).back);
}

TEST_F(BigPictureNavigationUITest, PickerIsolatesFocusRestoresSelectionAndHandlesParentShortcut)
{
  auto launch = std::make_shared<MenuPage>(MenuPage{
      "start-game", "Start Game", {}, {{"file", "Start File"}, {"ipl", "GameCube IPL"}}, "file"});
  launch->presentation = MenuPresentation::Launch;
  launch->toolbar = {{"back", "Back"}};
  Settle(*launch);
  MenuPage picker{"file-picker",
                  "Select Game File",
                  {},
                  {{"parent", "Parent Folder"}, {"folder", "Games"}, {"game", "Game.rvz"}},
                  "folder"};
  picker.presentation = MenuPresentation::FilePicker;
  picker.focus_revision = 1;
  picker.background = launch;
  picker.toolbar = {{"path", "Enter Path"}, {"roots", "Locations"}};
  Settle(picker);
  EXPECT_EQ(Press(picker, ImGuiKey_GamepadDpadDown).focused, "game");
  EXPECT_EQ(Press(picker, ImGuiKey_GamepadFaceDown).activated, "game");
  EXPECT_EQ(toolkit.GetMemory("start-game").item, "file");
  EXPECT_EQ(Press(picker, ImGuiKey_Backspace).activated, "parent");
  EXPECT_EQ(Press(picker, ImGuiKey_GamepadL1).activated, "parent");
  MenuPage keyboard{"keyboard", "Enter Path", {}, {{"done", "Done"}}, "done"};
  Settle(keyboard);
  Settle(picker);
  EXPECT_EQ(Press(picker, ImGuiKey_Enter).activated, "game");
  ++picker.focus_revision;
  picker.items = {{"new", "New folder"}};
  picker.default_focus = "new";
  Settle(picker);
  EXPECT_EQ(Press(picker, ImGuiKey_Enter).activated, "new");
  EXPECT_EQ(Press(picker, ImGuiKey_Backspace).activated, "roots");
  EXPECT_TRUE(Press(picker, ImGuiKey_Escape).back);
  Settle(*launch);
  EXPECT_EQ(Press(*launch, ImGuiKey_Enter).activated, "file");
}

TEST_F(BigPictureNavigationUITest, LaunchFitsAllConsoleRegionsAndScaling)
{
  MenuPage page{"start-game", "Start Game"};
  page.presentation = MenuPresentation::Launch;
  page.toolbar = {{"back", "Back"}};
  for (int i = 0; i < 7; ++i)
    page.items.push_back({std::to_string(i), "Installed console menu"});
  for (const auto size : {ImVec2(1280, 720), ImVec2(1634, 977), ImVec2(3440, 1440)})
  {
    ImGui::GetIO().DisplaySize = size;
    for (int frame = 0; frame < 4; ++frame)
      Frame(page, {false, 1.5f, 0.15f});
    ASSERT_NE(GImGui->NavWindow, nullptr);
    const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                                GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
    EXPECT_GE(rect.Min.x, 0);
    EXPECT_GE(rect.Min.y, 0);
    EXPECT_LE(rect.Max.x, size.x);
    EXPECT_LE(rect.Max.y, size.y);
  }
}

TEST(BigPicturePromptTest, LaunchAndPickerHintsUseBoundButtonsAndOnlySupportedShortcuts)
{
  ::BigPicture::NavigationStatus status;
  status.device = "DualSense Wireless Controller";
  status.device_source = "DInput";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Confirm)] = "`Button 2`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Back)] = "`Button 1`";
  status.bindings[static_cast<std::size_t>(::BigPicture::NavButton::Previous)] = "`Button 4`";
  ::BigPicture::NavigationText text;
  text.navigate = "Navigate";
  text.select = "Select";
  text.back = "Back";
  text.library_cancel = "Cancel";
  text.library_parent = "Parent Folder";
  const auto picker = GetLaunchControllerPrompts(status, text, true);
  EXPECT_NE(picker.find("\xe2\x86\xb0 Parent Folder"), std::string::npos);
  EXPECT_NE(picker.find("\xe2\x87\xa2 Select"), std::string::npos);
  EXPECT_NE(picker.find("\xe2\x87\xa3 Cancel"), std::string::npos);
  EXPECT_EQ(picker.find("Button"), std::string::npos);
  const auto launch = GetLaunchControllerPrompts(status, text, false);
  EXPECT_EQ(launch.find("Parent Folder"), std::string::npos);
  EXPECT_NE(launch.find("\xe2\x87\xa3 Back"), std::string::npos);
  status.options.swap_confirm_back = true;
  EXPECT_NE(GetLaunchControllerPrompts(status, text, false).find("\xe2\x87\xa3 Select"),
            std::string::npos);
}

TEST_F(BigPictureNavigationUITest, PickerRestoresAndRevealsSelectionInLongListings)
{
  MenuPage picker{"file-picker", "Select Game File", {}, {}, "0"};
  picker.presentation = MenuPresentation::FilePicker;
  picker.focus_revision = 17;
  for (int i = 0; i < 80; ++i)
    picker.items.push_back({std::to_string(i), "A long file name 日本語.rvz"});
  Settle(picker);
  for (int i = 0; i < 26; ++i)
    Press(picker, ImGuiKey_DownArrow);
  EXPECT_EQ(toolkit.GetMemory(picker.id).item, "26");
  EXPECT_GT(toolkit.GetMemory(picker.id).scroll, 0);
  MenuPage child{"keyboard", "Enter Path", {}, {{"done", "Done"}}};
  Settle(child);
  Settle(picker);
  EXPECT_EQ(Press(picker, ImGuiKey_Enter).activated, "26");
  for (int frame = 0; frame < 5; ++frame)
    Frame(picker, {false, 1.5f, 0.15f});
  ASSERT_NE(GImGui->NavWindow, nullptr);
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_GE(rect.Min.y, GImGui->NavWindow->InnerRect.Min.y - 2);
  EXPECT_LE(rect.Max.y, GImGui->NavWindow->InnerRect.Max.y + 2);
  EXPECT_EQ(toolkit.GetMemory(picker.id).item, "26");
}

TEST_F(BigPictureNavigationUITest, SettingsSwitchesCategoriesAndKeepsRowOptionsAccessible)
{
  MenuPage page{"settings-interface",
                "Interface Settings",
                "Global settings",
                {{"theme", "Theme", "Choose the appearance of Big Picture."},
                 {"startup", "Start in Big Picture", "Open Big Picture when Dolphin starts."}},
                "theme"};
  page.presentation = MenuPresentation::Settings;
  page.category_page = true;
  page.context = {"setting-options", "Setting Options", {}, true, {}, false, MenuIcon::Settings};
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back},
                  {"Interface", "Interface", {}, true, {}, true, MenuIcon::Display},
                  {"Display", "Display", {}, true, {}, false, MenuIcon::Camera},
                  {"Audio", "Audio", {}, true, {}, false, MenuIcon::Audio},
                  {"global", "Global", {}, true, {}, true, MenuIcon::Globe}};
  page.items[0].value = "Dark";
  page.items[0].section = "Appearance";
  page.items[0].has_options = true;
  page.items[1].toggle = false;
  page.items[1].section = "Startup";
  page.items[1].has_options = true;
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "theme");
  Press(page, ImGuiKey_GamepadDpadDown);
  EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "startup");
  auto busy = page;
  busy.busy = true;
  busy.context.enabled = false;
  for (auto& item : busy.items)
    item.enabled = false;
  for (auto& item : busy.toolbar)
    item.enabled = false;
  Settle(busy);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "startup");
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "startup");
  EXPECT_EQ(Press(page, ImGuiKey_F2).activated, "edit:startup");
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(rect.GetCenter().x, rect.GetCenter().y);
  Frame(page);
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  EXPECT_EQ(Frame(page).activated, "edit:startup");
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  Frame(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "startup");
  io.AddMousePosEvent(-100, -100);
  EXPECT_EQ(Press(page, ImGuiKey_RightArrow).activated, "Display");
  EXPECT_EQ(Press(page, ImGuiKey_LeftArrow).activated, "Audio");
  EXPECT_EQ(Press(page, ImGuiKey_GamepadR1).activated, "Display");
  EXPECT_EQ(Press(page, ImGuiKey_GamepadDpadRight).activated, "edit:startup");
  MenuPage child = page;
  child.id = "startup-options";
  child.items = {{"reset", "Reset to default"}, {"inherit", "Use inherited value"}};
  child.default_focus = "reset";
  Settle(child);
  EXPECT_TRUE(Press(child, ImGuiKey_Escape).back);
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_F2).activated, "edit:startup");
  Press(page, ImGuiKey_GamepadDpadUp);
  Press(page, ImGuiKey_GamepadDpadUp);
  for (int i = 0; i < 8; ++i)
    Press(page, ImGuiKey_GamepadDpadRight);
  EXPECT_NE(toolkit.GetMemory(page.id).item, "setting-options");
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "global");

  // A number's options page steps its value with Left and Right; shoulders still change page.
  MenuPage stepper = page;
  stepper.id = "volume-options";
  stepper.category_page = false;
  stepper.items = {{"adjust--1", "Decrease by 1"}, {"adjust-1", "Increase by 1"}};
  stepper.default_focus = "adjust-1";
  stepper.decrease = "adjust--1";
  stepper.increase = "adjust-1";
  Settle(stepper);
  EXPECT_EQ(Press(stepper, ImGuiKey_LeftArrow).activated, "adjust--1");
  EXPECT_EQ(Press(stepper, ImGuiKey_GamepadDpadRight).activated, "adjust-1");
  EXPECT_EQ(Press(stepper, ImGuiKey_GamepadDpadLeft).activated, "adjust--1");
  EXPECT_EQ(Press(stepper, ImGuiKey_GamepadR1).activated, "Display");
  // Without large steps, Page Up/Down do nothing on a stepper.
  EXPECT_FALSE(Press(stepper, ImGuiKey_PageUp).activated);

  // A slider page also takes large steps from Page Up/Down.
  MenuPage slider = stepper;
  slider.id = "volume-slider";
  MenuItem value{"value", "Volume"};
  value.value = "37%";
  value.slider = MenuSlider{0, 100, 1, 37, "0%", "100%"};
  slider.items = {value};
  slider.default_focus = "value";
  slider.decrease = "value:decrease";
  slider.increase = "value:increase";
  slider.decrease_large = "value:decrease-large";
  slider.increase_large = "value:increase-large";
  Settle(slider);
  EXPECT_EQ(Press(slider, ImGuiKey_RightArrow).activated, "value:increase");
  EXPECT_EQ(Press(slider, ImGuiKey_PageUp).activated, "value:increase-large");
  EXPECT_EQ(Press(slider, ImGuiKey_PageDown).activated, "value:decrease-large");

  // Dragging sets the value under the pointer once per value, and retries a value the host
  // has not applied once the published value changes (e.g. after a stale rejection).
  const auto track = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                               GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  io.AddMousePosEvent(track.Min.x + 48, track.GetCenter().y);
  Frame(slider);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const auto first = Frame(slider);
  EXPECT_EQ(first.activated, "value:set");
  EXPECT_EQ(first.value, 0);
  EXPECT_FALSE(Frame(slider).activated);
  io.AddMousePosEvent(track.Max.x, track.GetCenter().y);
  const auto second = Frame(slider);
  EXPECT_EQ(second.activated, "value:set");
  EXPECT_EQ(second.value, 100);
  EXPECT_FALSE(Frame(slider).activated);
  slider.items.front().slider->value = 0;
  const auto retry = Frame(slider);
  EXPECT_EQ(retry.activated, "value:set");
  EXPECT_EQ(retry.value, 100);
  EXPECT_FALSE(Frame(slider).activated);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  Frame(slider);
  io.AddMousePosEvent(-100, -100);
}

TEST_F(BigPictureNavigationUITest, SettingsContentKeepsSpaceBelowHeaderAndAboveFooter)
{
  MenuPage page{"settings-spacing", "Graphics Backend", "An explanation.", {{"one", "One"}}};
  page.presentation = MenuPresentation::Settings;
  Settle(page);
  auto* window = GImGui->NavWindow;
  ASSERT_NE(window, nullptr);
  float text_top = 720;
  const auto text_color = ImGui::GetColorU32(Style::GetPalette(page.theme).text);
  for (const auto& vertex : window->DrawList->VtxBuffer)
    if (vertex.col == text_color)
      text_top = std::min(text_top, vertex.pos.y);
  EXPECT_GE(text_top, 44 + 16);

  page.id = "settings-long-spacing";
  page.category_page = true;
  page.description.clear();
  page.items.clear();
  for (int i = 0; i < 40; ++i)
    page.items.push_back({std::to_string(i), "Setting", "Description"});
  toolkit.RestoreMemory(page.id, {"39", 100000, 0, "39"});
  Settle(page);
  window = GImGui->NavWindow;
  ASSERT_NE(window, nullptr);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "39");
  const auto last = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_LE(last.Max.y, 720 - 34 - 16);
  EXPECT_LE(last.Max.y, window->InnerRect.Max.y - window->WindowPadding.y + 2);
  EXPECT_LE(window->InnerRect.Max.y, 720 - 34 - 16);
}

TEST_F(BigPictureNavigationUITest, SettingsRetainAndRevealRowsAcrossNestedPagesAndScaling)
{
  MenuPage page{"settings-display", "Display Settings", "Game settings: Example", {}, "0"};
  page.presentation = MenuPresentation::Settings;
  page.category_page = true;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back}};
  for (int i = 0; i < 40; ++i)
  {
    MenuItem item{std::to_string(i), "Long setting label 日本語",
                  "An explanation which must wrap at large scales and remain visible."};
    item.value = i % 2 ? "A long inherited choice value" : "Off";
    item.section = i < 20 ? "Presentation" : "Enhancements";
    page.items.push_back(item);
  }
  Settle(page);
  for (int i = 0; i < 24; ++i)
    Press(page, ImGuiKey_DownArrow);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "24");
  EXPECT_GT(toolkit.GetMemory(page.id).scroll, 0);
  MenuPage child{"choice", "Choose Value", {}, {{"one", "One"}}};
  Settle(child);
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "24");
  for (int frame = 0; frame < 6; ++frame)
    Frame(page, {false, 1.5f, 0.15f});
  ASSERT_NE(GImGui->NavWindow, nullptr);
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_GE(rect.Min.y, GImGui->NavWindow->InnerRect.Min.y - 2);
  EXPECT_LE(rect.Max.y, GImGui->NavWindow->InnerRect.Max.y + 2);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "24");
  page.items.erase(page.items.begin() + 24);
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "0");
}
TEST_F(BigPictureNavigationUITest, StateGridNavigatesInThreeColumnsAndRestoresSelection)
{
  MenuPage page{"save-states", "Save State", {}, {}, "slot-1"};
  page.presentation = MenuPresentation::States;
  page.toolbar = {{"back", "Back", {}, true, {}, false, MenuIcon::Back},
                  {"refresh", "Refresh", {}, true, {}, false, MenuIcon::Reset}};
  for (int i = 1; i <= 10; ++i)
    page.items.push_back({"slot-" + std::to_string(i), "Save Slot " + std::to_string(i)});
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_RightArrow).focused, "slot-2");
  EXPECT_EQ(Press(page, ImGuiKey_DownArrow).focused, "slot-5");
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "slot-5");
  EXPECT_EQ(Press(page, ImGuiKey_F1).activated, "refresh");
  EXPECT_EQ(Press(page, ImGuiKey_GamepadL1).activated, "refresh");
  MenuPage parent{"root", "Pause", {}, {{"save", "Save State"}}};
  Settle(parent);
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "slot-5");
  EXPECT_TRUE(Press(page, ImGuiKey_Escape).back);
}

TEST_F(BigPictureNavigationUITest, StateGridSkipsEmptyLoadsAndRevealsSelectionAfterScaling)
{
  MenuPage page{"load-states", "Load State", {}, {}, "slot-1"};
  page.presentation = MenuPresentation::States;
  for (int i = 1; i <= 10; ++i)
    page.items.push_back(
        {"slot-" + std::to_string(i), "Load Slot " + std::to_string(i), {}, i != 2});
  Settle(page);
  EXPECT_EQ(Press(page, ImGuiKey_RightArrow).focused, "slot-3");
  toolkit.RestoreMemory(page.id, {"slot-10", 100000});
  for (int frame = 0; frame < 6; ++frame)
    Frame(page, {false, 1.5f, 0.15f});
  ASSERT_NE(GImGui->NavWindow, nullptr);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "slot-10");
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_GE(rect.Min.y, GImGui->NavWindow->InnerRect.Min.y - 2);
  EXPECT_LE(rect.Max.y, GImGui->NavWindow->InnerRect.Max.y + 2);
}
TEST_F(BigPictureNavigationUITest, StateSlotTextStaysInsideThePaddedSelectionBox)
{
  MenuPage page{
      "state-padding", "Save State", {}, {{"slot-1", "Slot 1", "10/27/2026 18:54:02"}}, "slot-1"};
  page.presentation = MenuPresentation::States;
  Settle(page);
  ASSERT_NE(GImGui->NavWindow, nullptr);
  auto* window = GImGui->NavWindow;
  const auto selection = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
  const auto text_color = ImGui::GetColorU32(Style::GetPalette(page.theme).text);
  bool found_text = false;
  for (const auto& vertex : window->DrawList->VtxBuffer)
  {
    if (vertex.col != text_color)
      continue;
    found_text = true;
    EXPECT_GE(vertex.pos.x, selection.Min.x + 10);
    EXPECT_LE(vertex.pos.x, selection.Max.x - 10);
    EXPECT_LE(vertex.pos.y, selection.Max.y - 10);
  }
  EXPECT_TRUE(found_text);
}
MenuPage AchievementsFixture()
{
  auto data = std::make_shared<::BigPicture::AchievementsSnapshot>();
  ::BigPicture::AchievementSubset subset;
  subset.id = 1;
  subset.title = "Example Game";
  subset.summary = "You have unlocked 1 of 3 achievements, earning 5 of 20 possible points.";
  subset.progress = 1.0f / 3;
  subset.progress_label = "33%";
  ::BigPicture::Achievement earned;
  earned.id = 11;
  earned.title = "First Victory";
  earned.description = "Complete the first battle.";
  earned.points = "5 points";
  earned.unlocked = true;
  earned.unlocked_at = "Unlocked: 10/27/2026 18:54";
  earned.type = "Progression";
  subset.groups.push_back({2, "Unlocked", {earned}});
  earned.id = 12;
  earned.title = "Almost There";
  earned.unlocked = false;
  earned.unlocked_at.clear();
  earned.measured = "18/20";
  earned.progress = 0.9f;
  subset.groups.push_back({7, "Almost There", {earned}});
  earned.id = 13;
  earned.progress = 0.4f;
  earned.measured = "2/5";
  subset.groups.push_back({1, "Locked", {earned}});
  data->subsets.push_back(subset);
  subset.id = 2;
  subset.title = "Bonus Challenge";
  subset.groups.front().achievements.front().id = 21;
  data->subsets.push_back(subset);
  MenuPage page;
  page.id = "achievements";
  page.presentation = MenuPresentation::Achievements;
  page.achievements = data;
  return page;
}

TEST_F(BigPictureNavigationUITest, AchievementsNavigateSubsetsAndCollapseGroups)
{
  auto page = AchievementsFixture();
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).selected, "subset:1");
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "subset:1");
  Press(page, ImGuiKey_Enter);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "group:1:2");
  Press(page, ImGuiKey_Enter);
  EXPECT_EQ(Press(page, ImGuiKey_DownArrow).focused, "group:1:7");
  Press(page, ImGuiKey_UpArrow);
  Press(page, ImGuiKey_Enter);
  EXPECT_EQ(Press(page, ImGuiKey_DownArrow).focused, "achievement:11");
  Press(page, ImGuiKey_GamepadR1);
  EXPECT_EQ(toolkit.GetMemory(page.id).selected, "subset:2");
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "subset:2");
  Press(page, ImGuiKey_GamepadL1);
  EXPECT_EQ(toolkit.GetMemory(page.id).selected, "subset:1");
  Press(page, ImGuiKey_PageDown);
  EXPECT_EQ(toolkit.GetMemory(page.id).selected, "subset:2");
  EXPECT_TRUE(Press(page, ImGuiKey_Escape).back);
}

TEST_F(BigPictureNavigationUITest, AchievementRefreshAndReturnKeepTheSelectedRow)
{
  auto page = AchievementsFixture();
  Settle(page);
  Press(page, ImGuiKey_Enter);
  Press(page, ImGuiKey_DownArrow);
  ASSERT_EQ(toolkit.GetMemory(page.id).item, "achievement:11");
  auto changed = std::make_shared<::BigPicture::AchievementsSnapshot>(*page.achievements);
  changed->subsets.front().groups.front().achievements.front().description = "Updated description.";
  changed->subsets.front().progress = 0.66f;
  page.achievements = changed;
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "achievement:11");
  MenuPage parent{"pause", "Pause", {}, {{"achievements", "Achievements"}}};
  Settle(parent);
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "achievement:11");
  EXPECT_TRUE(Press(page, ImGuiKey_GamepadFaceRight).back);
}

TEST_F(BigPictureNavigationUITest, AchievementSelectionStaysVisibleAtMaximumScale)
{
  auto page = AchievementsFixture();
  auto data = std::make_shared<::BigPicture::AchievementsSnapshot>(*page.achievements);
  auto& rows = data->subsets.front().groups.front().achievements;
  const auto first = rows.front();
  for (int i = 0; i < 25; ++i)
  {
    auto row = first;
    row.id = 100 + i;
    rows.push_back(row);
  }
  page.achievements = data;
  Settle(page);
  Press(page, ImGuiKey_Enter);
  for (int i = 0; i < 20; ++i)
    Press(page, ImGuiKey_DownArrow);
  const auto focused = toolkit.GetMemory(page.id).item;
  for (int frame = 0; frame < 8; ++frame)
    Frame(page, {false, 1.5f, 0.15f});
  ASSERT_NE(GImGui->NavWindow, nullptr);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, focused);
  const auto rect = ImGui::WindowRectRelToAbs(GImGui->NavWindow,
                                              GImGui->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
  EXPECT_GE(rect.Min.y, GImGui->NavWindow->InnerRect.Min.y - 2);
  EXPECT_LE(rect.Max.y, GImGui->NavWindow->InnerRect.Max.y + 2);
}

TEST_F(BigPictureNavigationUITest, AchievementPointsAlignWithDescriptionForWrappedTitles)
{
  auto page = AchievementsFixture();
  for (const auto* title :
       {"Short Title", "A long achievement title that wraps across multiple lines and still "
                       "keeps the point value aligned with the description below the title"})
  {
    toolkit.Reset();
    auto data = std::make_shared<::BigPicture::AchievementsSnapshot>(*page.achievements);
    data->subsets.resize(1);
    data->subsets.front().groups.resize(1);
    auto& row = data->subsets.front().groups.front().achievements.front();
    row.title = title;
    row.type.clear();
    row.description = "MMMM";
    row.points = "MMMM";
    row.unlocked_at.clear();
    page.achievements = data;
    Settle(page);
    Press(page, ImGuiKey_Enter);
    Press(page, ImGuiKey_DownArrow);
    ASSERT_EQ(toolkit.GetMemory(page.id).item, "achievement:11");
    auto* window = GImGui->NavWindow;
    ASSERT_NE(window, nullptr);
    const auto rect = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
    float description_top = 10000;
    float points_top = 10000;
    const auto* glyph = ImGui::GetFont()->GetFontBaked(14)->FindGlyphNoFallback('M');
    ASSERT_NE(glyph, nullptr);
    const auto text_color = ImGui::GetColorU32(Style::GetPalette(page.theme).text);
    for (const auto& vertex : window->DrawList->VtxBuffer)
    {
      if (vertex.col != text_color || vertex.uv.x != glyph->U0 || vertex.uv.y != glyph->V0)
        continue;
      if (vertex.pos.x >= rect.Min.x + 66 && vertex.pos.x < rect.Min.x + 170)
        description_top = std::min(description_top, vertex.pos.y);
      if (vertex.pos.x > rect.Max.x - 100)
        points_top = std::min(points_top, vertex.pos.y);
    }
    EXPECT_LT(description_top, 10000);
    EXPECT_LT(points_top, 10000);
    EXPECT_FLOAT_EQ(description_top, points_top);
  }
}

TEST_F(BigPictureNavigationUITest, CachedPercentageWithoutCurrentMeasurementDoesNotDrawABar)
{
  auto page = AchievementsFixture();
  auto data = std::make_shared<::BigPicture::AchievementsSnapshot>(*page.achievements);
  data->subsets.resize(1);
  data->subsets.front().groups.resize(1);
  auto& row = data->subsets.front().groups.front().achievements.front();
  row.unlocked = false;
  row.unlocked_at.clear();
  row.progress = 0.75f;
  row.measured.clear();
  page.achievements = data;
  Settle(page);
  Press(page, ImGuiKey_Enter);
  Press(page, ImGuiKey_DownArrow);
  const auto has_progress_fill = [&] {
    const auto fill = ImGui::GetColorU32(Style::GetPalette(page.theme).selected);
    return std::ranges::any_of(GImGui->NavWindow->DrawList->VtxBuffer,
                               [&](const auto& vertex) { return vertex.col == fill; });
  };
  ASSERT_NE(GImGui->NavWindow, nullptr);
  EXPECT_FALSE(has_progress_fill());
  auto refreshed = std::make_shared<::BigPicture::AchievementsSnapshot>(*data);
  refreshed->subsets.front().groups.front().achievements.front().measured = "3/4";
  page.achievements = refreshed;
  Settle(page);
  EXPECT_TRUE(has_progress_fill());
}

TEST_F(BigPictureNavigationUITest, EmptyAchievementViewRemainsDismissibleAndRestoresFontStyles)
{
  auto page = AchievementsFixture();
  page.achievements.reset();
  page.empty_text = "Log in to RetroAchievements to view achievements.";
  const auto style = ImGui::GetStyle();
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory(page.id).item, "back");
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "back");
  EXPECT_TRUE(Press(page, ImGuiKey_GamepadFaceRight).back);
  EXPECT_FLOAT_EQ(ImGui::GetStyle().WindowPadding.x, style.WindowPadding.x);
  EXPECT_FLOAT_EQ(ImGui::GetStyle().WindowPadding.y, style.WindowPadding.y);
  EXPECT_EQ(ImGui::GetStyle().Colors[ImGuiCol_NavCursor].w, style.Colors[ImGuiCol_NavCursor].w);
}

TEST_F(BigPictureNavigationUITest, SharedViewStylesRestoreCallerColorsAndSpacing)
{
  auto& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(9, 17);
  style.ItemSpacing = ImVec2(3, 11);
  style.ScrollbarSize = 19;
  style.FontScaleMain = 1.25f;
  const auto original = style;
  for (const auto* theme : {"Dark", "Light"})
  {
    for (const auto presentation :
         {MenuPresentation::States, MenuPresentation::Settings, MenuPresentation::Achievements})
    {
      MenuPage page{"style-test", "Settings", {}, {{"entry", "Entry"}}};
      page.presentation = presentation;
      page.theme = theme;
      const int colors = GImGui->ColorStack.Size;
      const int variables = GImGui->StyleVarStack.Size;
      Settle(page);
      EXPECT_EQ(GImGui->ColorStack.Size, colors);
      EXPECT_EQ(GImGui->StyleVarStack.Size, variables);
      EXPECT_FLOAT_EQ(style.WindowPadding.x, original.WindowPadding.x);
      EXPECT_FLOAT_EQ(style.WindowPadding.y, original.WindowPadding.y);
      EXPECT_FLOAT_EQ(style.WindowBorderSize, original.WindowBorderSize);
      EXPECT_FLOAT_EQ(style.ChildBorderSize, original.ChildBorderSize);
      EXPECT_FLOAT_EQ(style.ItemSpacing.x, original.ItemSpacing.x);
      EXPECT_FLOAT_EQ(style.ItemSpacing.y, original.ItemSpacing.y);
      EXPECT_FLOAT_EQ(style.ScrollbarSize, original.ScrollbarSize);
      EXPECT_FLOAT_EQ(style.FontScaleMain, original.FontScaleMain);
      for (int color = 0; color < ImGuiCol_COUNT; ++color)
      {
        EXPECT_FLOAT_EQ(style.Colors[color].x, original.Colors[color].x);
        EXPECT_FLOAT_EQ(style.Colors[color].y, original.Colors[color].y);
        EXPECT_FLOAT_EQ(style.Colors[color].z, original.Colors[color].z);
        EXPECT_FLOAT_EQ(style.Colors[color].w, original.Colors[color].w);
      }
    }
  }
}
}  // namespace
