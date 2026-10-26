// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

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

TEST_F(BigPictureInGameMenuTest, SharedSettingsRowsCarryTypedEditsAndNestedBackRequests)
{
  snapshot.screens.push_back(Screen::Settings);
  auto view = std::make_shared<SettingsMenuSnapshot>();
  view->id = "settings-test";
  view->title = "Audio";
  view->back = SettingsRequest{22, SettingsOperation::Back};
  SettingsRow row{"volume", "37%"};
  row.edit = Common::Settings::Request{45, "audio.volume", Common::Settings::Operation::Set, 37};
  view->rows.push_back(row);
  row.id = "disabled";
  row.enabled = false;
  view->rows.push_back(row);
  snapshot.host.settings_menu = view;
  const auto menu = Build();
  ASSERT_TRUE(menu.Activate({"volume"}));
  EXPECT_EQ(menu.Activate({"volume"})->action, Action::EditSetting);
  EXPECT_EQ(menu.Activate({"volume"})->setting->revision, 45u);
  EXPECT_EQ(menu.Activate({"volume"})->setting->value, Common::Settings::Value(37));
  EXPECT_FALSE(menu.Activate({"disabled"}));
  ASSERT_TRUE(menu.Activate({{}, true})->settings_menu);
  EXPECT_EQ(menu.Activate({{}, true})->settings_menu->operation, SettingsOperation::Back);
  snapshot.pending = Command{snapshot.generation, 123, Action::EditSetting};
  EXPECT_TRUE(Build().requests.empty());
}

TEST_F(BigPictureInGameMenuTest, SharedSettingsRootClosesWhileInterfaceRetainsMenuPreferences)
{
  snapshot.screens.push_back(Screen::Settings);
  auto view = std::make_shared<SettingsMenuSnapshot>();
  view->id = "settings-interface";
  view->preferences = true;
  snapshot.host.settings_menu = view;
  const auto menu = Build();
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::Back);
  ASSERT_TRUE(menu.Activate({"swap"})->options);
  EXPECT_TRUE(menu.Activate({"swap"})->options->swap_confirm_back);
}

TEST_F(BigPictureInGameMenuTest, TenSlotsCarryOneBasedNumbersAndDisableEmptyUnreadableLoads)
{
  snapshot.screens.push_back(Screen::LoadStates);
  snapshot.host.state_slots_ready = true;
  snapshot.host.state_slots[1] = {true, false, {}};
  snapshot.host.state_slots[9] = {true, true, "Saved date"};
  auto menu = Build();
  EXPECT_EQ(menu.page.items.size(), 10u);
  EXPECT_EQ(menu.page.toolbar.size(), 2u);
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

TEST_F(BigPictureInGameMenuTest, GlobalThemeReachesPauseLibraryAndSettingsPages)
{
  snapshot.host.menu_theme = "Light";
  EXPECT_EQ(Build().page.theme, "Light");
  snapshot.screens = {Screen::LoadStates};
  EXPECT_EQ(Build().page.theme, "Light");
  snapshot.host.settings_menu = std::make_shared<SettingsMenuSnapshot>();
  snapshot.screens = {Screen::Settings};
  EXPECT_EQ(Build().page.theme, "Light");
  snapshot.host.library = std::make_shared<LibrarySnapshot>();
  snapshot.screens = {Screen::Library};
  snapshot.mode = Mode::Library;
  EXPECT_EQ(Build().page.theme, "Light");
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
TEST_F(BigPictureInGameMenuTest, LibraryCardsCarryPathsAndRetainSafeFocusWhenSelectionDisappears)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Library};
  auto library = std::make_shared<LibrarySnapshot>();
  library->games = {{"one.iso", "Same title"}, {"two.iso", "Same title"}};
  library->total_games = 2;
  library->selected_path = "two.iso";
  snapshot.host.library = library;
  auto menu = Build();
  EXPECT_EQ(menu.page.default_focus, "game:two.iso");
  EXPECT_EQ(menu.Activate({"game:two.iso"})->path, "two.iso");
  EXPECT_EQ(menu.Activate({"add-folder"})->library_operation, LibraryOperation::AddFolder);
  EXPECT_EQ(menu.page.toolbar.size(), 6u);
  ASSERT_TRUE(menu.Activate({"view-toggle"}));
  EXPECT_EQ(menu.Activate({"view-toggle"})->library_operation, LibraryOperation::View);
  ASSERT_TRUE(menu.Activate({"settings"}));
  EXPECT_EQ(menu.Activate({"settings"})->action, Action::Settings);
  library->selected_path.clear();
  library->games.pop_back();
  menu = Build();
  EXPECT_EQ(menu.page.default_focus, "refresh");
  EXPECT_FALSE(menu.Activate({"game:two.iso"}));
  snapshot.screens.push_back(Screen::LibraryOptions);
  menu = Build();
  EXPECT_TRUE(std::ranges::none_of(
      menu.requests, [](const auto& entry) { return entry.second.action == Action::Settings; }));
}

TEST_F(BigPictureInGameMenuTest, PickerUsesRevisionAndDistinguishesFolderNavigationFromDiscChanges)
{
  auto library = std::make_shared<LibrarySnapshot>();
  library->picker_revision = 9;
  library->picker_disc = true;
  library->files = {{"dir", "Folder", true}, {"other.iso", "Other Disc", false}};
  snapshot.host.library = library;
  snapshot.host.can_change_disc = true;
  snapshot.screens.push_back(Screen::FilePicker);
  auto menu = Build();
  EXPECT_EQ(menu.Activate({"file:dir"})->library_operation, LibraryOperation::EnterDirectory);
  ASSERT_TRUE(menu.Activate({"file:other.iso"}));
  EXPECT_EQ(menu.Activate({"file:other.iso"})->action, Action::ChangeDisc);
  EXPECT_EQ(menu.Activate({"file:other.iso"})->library_revision, 9u);
  library->picker_disc = false;
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  ASSERT_TRUE(Build().Activate({"file:other.iso"}));
  EXPECT_EQ(Build().Activate({"file:other.iso"})->action, Action::Boot);
}

TEST_F(BigPictureInGameMenuTest, InGameDiscActionOpensControllerPickerWhenLibraryIsAvailable)
{
  snapshot.host.library = std::make_shared<LibrarySnapshot>();
  snapshot.host.can_change_disc = true;
  const auto menu = Build();
  EXPECT_EQ(menu.Activate({"disc"})->action, Action::Library);
  EXPECT_EQ(menu.Activate({"disc"})->library_operation, LibraryOperation::OpenDisc);
}
TEST_F(BigPictureInGameMenuTest, LibraryPolicyDisablesCardsWithoutCreatingFallbackRequests)
{
  snapshot.mode = Mode::Library;
  snapshot.screens = {Screen::Library};
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.host.netplay = true;
  text.reasons[static_cast<std::size_t>(UnavailableReason::NetPlay)] = "NetPlay";
  auto library = std::make_shared<LibrarySnapshot>();
  library->games = {{"game.iso", "Game"}};
  library->selected_path = "game.iso";
  snapshot.host.library = library;
  const auto menu = Build();
  ASSERT_EQ(menu.page.items.size(), 1u);
  EXPECT_FALSE(menu.page.items.front().enabled);
  EXPECT_EQ(menu.page.items.front().detail, "NetPlay");
  EXPECT_EQ(menu.page.default_focus, "refresh");
  EXPECT_FALSE(menu.Activate({"game:game.iso"}));
}

TEST_F(BigPictureInGameMenuTest,
       SearchCanBeClearedDirectlyAndNativeTextEntryDoesNotShowControllerKeys)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Library};
  auto library = std::make_shared<LibrarySnapshot>();
  library->query = "Search";
  snapshot.host.library = library;
  auto menu = Build();
  ASSERT_TRUE(menu.Activate({"clear-search"}));
  EXPECT_EQ(menu.Activate({"clear-search"})->library_operation, LibraryOperation::ClearSearch);
  snapshot.screens.push_back(Screen::TextInput);
  library->native_text_input = true;
  menu = Build();
  EXPECT_FALSE(menu.Activate({"key-0"}));
  EXPECT_TRUE(menu.page.toolbar.empty());
  library->native_text_input = false;
  menu = Build();
  EXPECT_TRUE(menu.Activate({"key-0"}));
  EXPECT_EQ(menu.Activate({"clear-text"})->library_operation, LibraryOperation::TextClear);
}
TEST_F(BigPictureInGameMenuTest, HomeShowsFourActionsEvenWhenTheLibraryIsPopulated)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Home};
  snapshot.host.library = std::make_shared<LibrarySnapshot>();
  snapshot.host.achievements_username = "PlayerOne";
  text.home_labels = {"Game List", "Start Game", "Settings", "Exit"};
  const auto menu = Build();
  EXPECT_EQ(menu.page.id, "home");
  EXPECT_EQ(menu.page.account_name, "PlayerOne");
  ASSERT_EQ(menu.page.items.size(), 4u);
  EXPECT_EQ(menu.page.items.front().image_key, "home:media-cdrom");
  EXPECT_EQ(menu.Activate({"game-list"})->action, Action::ShowLibrary);
  EXPECT_EQ(menu.Activate({"start-game"})->action, Action::StartGame);
  EXPECT_EQ(menu.Activate({"preferences"})->action, Action::Settings);
  EXPECT_EQ(menu.Activate({"exit-menu"})->action, Action::ExitMenu);
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::ExitMenu);
  snapshot.pending = Command{snapshot.generation, 1, Action::Settings};
  EXPECT_TRUE(Build().requests.empty());
}

TEST_F(BigPictureInGameMenuTest, StartGameOffersOnlyInstalledConsoleMenusAndControllerFilePicker)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Home, Screen::StartGame};
  snapshot.host.library = std::make_shared<LibrarySnapshot>();
  snapshot.host.boot_ipl_available[1] = true;
  snapshot.host.boot_ipl_available[5] = true;
  snapshot.host.boot_system_menu_available = true;
  const auto menu = Build();
  EXPECT_EQ(menu.Activate({"open-file"})->library_operation, LibraryOperation::OpenFile);
  EXPECT_EQ(menu.Activate({"ipl-1"})->slot, 1);
  EXPECT_EQ(menu.Activate({"ipl-5"})->slot, 5);
  EXPECT_FALSE(menu.Activate({"ipl-0"}));
  EXPECT_EQ(menu.Activate({"system-menu"})->action, Action::BootSystemMenu);
}
}  // namespace
