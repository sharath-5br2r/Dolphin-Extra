// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cmath>

#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

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
    io.ConfigNavCursorVisibleAlways = true;
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
  page.toolbar = {{"refresh", "Refresh"}};
  Settle(page);
  ASSERT_EQ(toolkit.GetMemory("library").item, "two");
  page.items.pop_back();
  page.default_focus = "refresh";
  Settle(page);
  EXPECT_EQ(toolkit.GetMemory("library").item, "refresh");
  EXPECT_EQ(Press(page, ImGuiKey_Enter).activated, "refresh");
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
  page.toolbar = {{"search", "Search"}, {"refresh", "Refresh"}};
  for (const auto size : {ImVec2(1280, 720), ImVec2(3840, 2160), ImVec2(3440, 1440)})
  {
    ImGui::GetIO().DisplaySize = size;
    toolkit.Reset();
    Settle(page);
    Press(page, ImGuiKey_GamepadDpadDown);
    EXPECT_EQ(Press(page, ImGuiKey_GamepadFaceDown).activated, "two");
    Press(page, ImGuiKey_GamepadDpadUp);
    Press(page, ImGuiKey_GamepadDpadUp);
    EXPECT_TRUE(toolkit.GetMemory(page.id).item == "search" ||
                toolkit.GetMemory(page.id).item == "refresh");
  }
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
}  // namespace
