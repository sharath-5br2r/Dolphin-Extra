// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <set>

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

TEST_F(BigPictureInGameMenuTest, GroupedDiscPickerUsesNormalNavigationAndOffersOnlyItsDiscChoices)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Home, Screen::Library, Screen::FilePicker};
  auto library = std::make_shared<LibrarySnapshot>();
  library->picker_boot_disc = true;
  library->picker_revision = 7;
  library->files = {{"disc1.iso", "Disc 1"}, {"disc2.iso", "Disc 2"}};
  snapshot.host.library = library;
  text.library_actions[static_cast<std::size_t>(LibraryOperation::ChooseBootDisc)] = "Select Disc";
  const auto menu = Build();
  EXPECT_EQ(menu.page.title, "Select Disc");
  EXPECT_EQ(menu.page.default_focus, "file:disc1.iso");
  EXPECT_TRUE(menu.page.toolbar.empty());
  EXPECT_EQ(menu.page.items.size(), 2u);
  const auto choice = menu.Activate({"file:disc2.iso"});
  ASSERT_TRUE(choice);
  EXPECT_EQ(choice->action, Action::Boot);
  EXPECT_EQ(choice->path, "disc2.iso");
  EXPECT_EQ(choice->library_revision, 7u);
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::Back);
}

TEST_F(BigPictureInGameMenuTest, RootBackExplicitlyResumesAndSubmenuBackNavigates)
{
  const auto menu = Build();
  EXPECT_EQ(menu.page.default_focus, "resume");
  EXPECT_EQ(menu.Activate({"resume"})->action, Action::Resume);
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::Resume);
  EXPECT_EQ(menu.Activate({"save"})->action, Action::SaveStates);
  EXPECT_EQ(menu.Activate({"load"})->action, Action::LoadStates);
  EXPECT_EQ(menu.Activate({"screenshot"})->action, Action::Screenshot);
  EXPECT_FALSE(menu.Activate({"disc"}));
  const auto disc =
      std::ranges::find(menu.page.items, "disc", &VideoCommon::BigPicture::MenuItem::id);
  ASSERT_NE(disc, menu.page.items.end());
  EXPECT_EQ(disc->detail, "Not a disc game");
  snapshot.screens.push_back(Screen::SaveStates);
  EXPECT_EQ(Build().Activate({{}, true})->action, Action::Back);
}

TEST_F(BigPictureInGameMenuTest, PauseCarriesRunningGameMetadataAndUsesReferenceActionOrder)
{
  snapshot.host.game_title = "Super Mario Sunshine";
  snapshot.host.game_path = "sunshine.rvz";
  snapshot.host.game_details = "sunshine.rvz\nGMSE01 | Revision 0";
  snapshot.host.game_play_time = "This Session: 0h 12m 3s\nAll Time: 22h 0m 0s";
  snapshot.host.game_rich_presence = "Exploring Delfino Plaza";
  snapshot.host.game_achievement_progress = "3/50 achievements | 15/500 points";
  const auto menu = Build();
  EXPECT_EQ(menu.page.presentation, VideoCommon::BigPicture::MenuPresentation::Pause);
  EXPECT_EQ(menu.page.context.label, snapshot.host.game_title);
  EXPECT_EQ(menu.page.context.image_key, snapshot.host.game_path);
  EXPECT_EQ(menu.page.context.detail, snapshot.host.game_details);
  EXPECT_EQ(menu.page.play_time, snapshot.host.game_play_time);
  EXPECT_EQ(menu.page.rich_presence, snapshot.host.game_rich_presence);
  EXPECT_EQ(menu.page.achievement_progress, snapshot.host.game_achievement_progress);
  std::vector<std::string> ids;
  for (const auto& item : menu.page.items)
    ids.push_back(item.id);
  EXPECT_EQ(ids, (std::vector<std::string>{"resume", "load", "save", "screenshot", "achievements",
                                           "disc", "reset", "preferences", "stop", "exit"}));
  EXPECT_EQ(menu.Activate({"stop"})->action, Action::Stop);
  EXPECT_EQ(menu.Activate({"exit"})->action, Action::Exit);
  EXPECT_FALSE(menu.requests.contains("global-preferences"));
  EXPECT_NE(VideoCommon::BigPicture::GetMenuIconCodepoint(
                VideoCommon::BigPicture::GetActionIcon(Action::Stop)),
            VideoCommon::BigPicture::GetMenuIconCodepoint(
                VideoCommon::BigPicture::GetActionIcon(Action::Exit)));
  snapshot.pending = Command{snapshot.generation, 1, Action::Screenshot};
  EXPECT_TRUE(Build().requests.empty());
}

TEST_F(BigPictureInGameMenuTest, AchievementsAreDisabledUntilLoggedInAndUseTheInGameView)
{
  auto menu = Build();
  const auto entry =
      std::ranges::find(menu.page.items, "achievements", &VideoCommon::BigPicture::MenuItem::id);
  ASSERT_NE(entry, menu.page.items.end());
  EXPECT_FALSE(entry->enabled);
  EXPECT_FALSE(menu.Activate({"achievements"}));
  snapshot.host.achievements_logged_in = true;
  ASSERT_TRUE(Build().Activate({"achievements"}));
  EXPECT_EQ(Build().Activate({"achievements"})->action, Action::Achievements);
  snapshot.host.achievements = std::make_shared<AchievementsSnapshot>();
  snapshot.screens.push_back(Screen::Achievements);
  menu = Build();
  EXPECT_EQ(menu.page.presentation, VideoCommon::BigPicture::MenuPresentation::Achievements);
  EXPECT_EQ(menu.page.achievements, snapshot.host.achievements);
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::Back);
  snapshot.host.achievements_logged_in = false;
  EXPECT_FALSE(Build().page.achievements);
}

TEST_F(BigPictureInGameMenuTest, StopAndExitOfferSeparateContinuationChoices)
{
  for (const auto action : {Action::Stop, Action::Exit})
  {
    snapshot.screens = {Screen::Pause, Screen::Confirmation};
    snapshot.confirmation = Command{snapshot.generation, 123, action};
    snapshot.host.can_save_continuation = true;
    auto menu = Build();
    EXPECT_EQ(menu.page.default_focus, "cancel");
    const auto save = menu.Activate({"save-and-exit"});
    ASSERT_TRUE(save);
    EXPECT_EQ(save->action, Action::Confirm);
    EXPECT_EQ(save->slot, 1);
    EXPECT_EQ(save->confirmation_id, 123u);
    EXPECT_EQ(menu.Activate({"confirm"})->slot, 0);
    snapshot.host.can_save_continuation = false;
    EXPECT_FALSE(Build().Activate({"save-and-exit"}));
    EXPECT_TRUE(Build().Activate({"confirm"}));
  }
}

TEST_F(BigPictureInGameMenuTest, ContinuationPromptOffersResumeFreshDeleteAndCancel)
{
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.host.continuation_pending = true;
  snapshot.host.can_load_continuation = true;
  snapshot.screens = {Screen::Continuation};
  auto menu = Build();
  EXPECT_TRUE(menu.page.dialog);
  EXPECT_EQ(menu.Activate({"continue"})->action, Action::ContinueGame);
  EXPECT_EQ(menu.Activate({"fresh"})->action, Action::FreshBoot);
  EXPECT_EQ(menu.Activate({"delete"})->action, Action::DeleteContinuation);
  EXPECT_EQ(menu.Activate({{}, true})->action, Action::Back);
  snapshot.host.can_load_continuation = false;
  menu = Build();
  EXPECT_FALSE(menu.Activate({"continue"}));
  EXPECT_TRUE(menu.Activate({"fresh"}));
  EXPECT_TRUE(menu.Activate({"delete"}));
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

TEST_F(BigPictureInGameMenuTest, MappingRowsOfferOptionsAndCaptureBlocksEveryRequest)
{
  snapshot.screens.push_back(Screen::Settings);
  auto view = std::make_shared<SettingsMenuSnapshot>();
  view->id = "settings-mapping";
  view->back = SettingsRequest{3, SettingsOperation::Back};
  SettingsRow row{"map:Buttons/A", "A"};
  row.navigation = SettingsRequest{3, SettingsOperation::MapBind, "Buttons/A"};
  row.options = SettingsRequest{3, SettingsOperation::MapOptions, "Buttons/A"};
  view->rows.push_back(row);
  snapshot.host.settings_menu = view;
  auto menu = Build();
  ASSERT_EQ(menu.page.items.front().id, "map:Buttons/A");
  EXPECT_TRUE(menu.page.items.front().has_options);
  EXPECT_EQ(menu.Activate({"map:Buttons/A"})->settings_menu->operation, SettingsOperation::MapBind);
  ASSERT_TRUE(menu.Activate({"edit:map:Buttons/A"}));
  EXPECT_EQ(menu.Activate({"edit:map:Buttons/A"})->settings_menu->operation,
            SettingsOperation::MapOptions);

  // While an input is captured, no press (including Back) may act in the menu.
  view->rows.clear();
  view->capturing = true;
  menu = Build();
  EXPECT_TRUE(menu.requests.empty());
  EXPECT_FALSE(menu.Activate({{}, true}));
  EXPECT_TRUE(menu.page.busy);
}

TEST_F(BigPictureInGameMenuTest, SliderRowsStepAndSetTheirValueFromTheMenu)
{
  snapshot.screens.push_back(Screen::Settings);
  auto view = std::make_shared<SettingsMenuSnapshot>();
  view->id = "settings-volume";
  view->back = SettingsRequest{9, SettingsOperation::Back};
  SettingsRow row{"value", "Volume"};
  SettingsSlider slider{0, 100, 1, 37, "0%", "100%"};
  Common::Settings::Request edit{45, "audio.volume", Common::Settings::Operation::SetDisplay};
  edit.display_value = 36;
  slider.decrease = SettingsAction{{}, edit};
  edit.display_value = 27;
  slider.decrease_large = SettingsAction{{}, edit};
  slider.set.edit =
      Common::Settings::Request{45, "audio.volume", Common::Settings::Operation::SetDisplay};
  row.slider = slider;
  view->rows.push_back(row);
  snapshot.host.settings_menu = view;
  auto menu = Build();
  ASSERT_TRUE(menu.page.items.front().slider);
  EXPECT_EQ(menu.page.items.front().slider->value, 37);
  EXPECT_EQ(menu.page.items.front().slider->maximum_label, "100%");
  EXPECT_EQ(menu.page.decrease, "value:decrease");
  EXPECT_EQ(menu.page.increase_large, "value:increase-large");
  EXPECT_EQ(menu.Activate({"value:decrease"})->setting->display_value, 36);
  EXPECT_EQ(menu.Activate({"value:decrease-large"})->setting->display_value, 27);
  // Absent steps (the value is at that end) and the row itself request nothing.
  EXPECT_FALSE(menu.Activate({"value:increase"}));
  EXPECT_FALSE(menu.Activate({"value"}));
  MenuResult set{"value:set"};
  set.value = 64;
  ASSERT_TRUE(menu.Activate(set));
  EXPECT_EQ(menu.Activate(set)->action, Action::EditSetting);
  EXPECT_EQ(menu.Activate(set)->setting->display_value, 64);

  // A host slider receives the value as text.
  slider = {0, 50, 1, 10, "0 %", "50 %"};
  slider.set.navigation = SettingsRequest{9, SettingsOperation::MapSettingSet};
  view->rows.front().slider = slider;
  menu = Build();
  set.value = 12.5;
  ASSERT_TRUE(menu.Activate(set));
  EXPECT_EQ(menu.Activate(set)->settings_menu->operation, SettingsOperation::MapSettingSet);
  EXPECT_EQ(menu.Activate(set)->settings_menu->value, "12.5");
  view->rows.front().enabled = false;
  EXPECT_FALSE(Build().Activate(set));
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

TEST_F(BigPictureInGameMenuTest, SettingsRowsCarrySwitchStateAndRetainTheirOptionsEditor)
{
  snapshot.screens = {Screen::Settings};
  auto view = std::make_shared<SettingsMenuSnapshot>();
  view->id = "settings-interface";
  view->category_page = true;
  SettingsRow row{"setting", "Toggle"};
  row.navigation = SettingsRequest{31, SettingsOperation::Edit, "display.vsync"};
  row.edit =
      Common::Settings::Request{45, "display.vsync", Common::Settings::Operation::SetDisplay, true};
  row.toggle = false;
  row.value = "Off";
  row.section = "Presentation";
  row.help = "Synchronize presentation with the display.";
  view->rows.push_back(row);
  snapshot.host.settings_menu = view;
  const auto menu = Build();
  EXPECT_EQ(menu.page.presentation, VideoCommon::BigPicture::MenuPresentation::Settings);
  EXPECT_EQ(menu.page.items.front().toggle, false);
  EXPECT_EQ(menu.page.items.front().value, "Off");
  EXPECT_EQ(menu.page.items.front().detail, row.help);
  EXPECT_EQ(menu.page.items.front().section, "Presentation");
  EXPECT_EQ(menu.Activate({"setting"})->action, Action::EditSetting);
  EXPECT_FALSE(menu.Activate({"setting"})->settings_menu);
  EXPECT_EQ(menu.Activate({"setting"})->setting->value, Common::Settings::Value(true));
  ASSERT_TRUE(menu.Activate({"edit:setting"})->settings_menu);
  EXPECT_EQ(menu.Activate({"edit:setting"})->settings_menu->operation, SettingsOperation::Edit);
  EXPECT_EQ(menu.page.toolbar.front().id, "back");
  EXPECT_EQ(menu.Activate({"back"})->action, Action::Back);
  snapshot.pending = Command{snapshot.generation, 1, Action::EditSetting};
  EXPECT_TRUE(Build().requests.empty());
}

TEST_F(BigPictureInGameMenuTest, EverySettingsCategoryHasADistinctGlyphAndNoExtraHeaderControl)
{
  snapshot.screens = {Screen::Settings};
  auto view = std::make_shared<SettingsMenuSnapshot>();
  for (const auto* id :
       {"Interface", "General", "Display", "Enhancements", "GraphicsHacks", "GraphicsAdvanced",
        "PostProcessing", "OSD", "Audio", "GameCube", "Wii", "Paths", "MemoryCards", "Achievements",
        "Advanced", "Triforce", "controllers"})
  {
    SettingsRow row{id, id};
    row.navigation = SettingsRequest{1, SettingsOperation::Category, id};
    view->toolbar.push_back(row);
  }
  snapshot.host.settings_menu = view;
  const auto menu = Build();
  EXPECT_TRUE(menu.page.context.id.empty());
  ASSERT_EQ(menu.page.toolbar.size(), view->toolbar.size() + 1);
  EXPECT_EQ(menu.page.toolbar.back().id, "controllers");
  std::set<ImWchar> glyphs;
  for (const auto& item : menu.page.toolbar)
  {
    if (item.id == "back")
      continue;
    const auto glyph = VideoCommon::BigPicture::GetMenuIconCodepoint(item.icon);
    EXPECT_NE(glyph, 0u) << item.id;
    EXPECT_TRUE(glyphs.insert(glyph).second) << item.id;
  }
}

TEST_F(BigPictureInGameMenuTest, TenSlotsCarryOneBasedNumbersAndDisableEmptyUnreadableLoads)
{
  snapshot.screens.push_back(Screen::LoadStates);
  snapshot.host.state_slots_ready = true;
  snapshot.host.state_slots[1] = {true, false, {}};
  snapshot.host.state_slots[9] = {true, true, "Saved date"};
  auto menu = Build();
  EXPECT_EQ(menu.page.items.size(), 10u);
  EXPECT_EQ(menu.page.columns, 3);
  EXPECT_TRUE(menu.page.overlay);
  EXPECT_EQ(menu.page.toolbar.size(), 2u);
  EXPECT_FALSE(menu.Activate({"slot-1"}));
  EXPECT_FALSE(menu.Activate({"slot-2"}));
  EXPECT_EQ(menu.page.items[0].detail, "Empty");
  EXPECT_EQ(menu.page.items[1].detail, "Unreadable");
  ASSERT_TRUE(menu.Activate({"slot-10"}));
  EXPECT_EQ(menu.Activate({"slot-10"})->slot, 10);
  EXPECT_EQ(menu.Activate({"slot-10"})->action, Action::LoadState);
  EXPECT_EQ(menu.page.default_focus, "slot-10");
  snapshot.screens.back() = Screen::SaveStates;
  menu = Build();
  EXPECT_EQ(menu.Activate({"slot-1"})->slot, 1);
  EXPECT_EQ(menu.Activate({"slot-1"})->action, Action::SaveState);
}

TEST_F(BigPictureInGameMenuTest, AnEmptyLoadBrowserFocusesRefreshInsteadOfADisabledSlot)
{
  snapshot.screens.push_back(Screen::LoadStates);
  EXPECT_EQ(Build().page.default_focus, "refresh");
  snapshot.host.state_slots_ready = true;
  EXPECT_EQ(Build().page.default_focus, "refresh");
  snapshot.host.state_slots[4] = {true, true, "Saved"};
  EXPECT_EQ(Build().page.default_focus, "slot-5");
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
  EXPECT_EQ(menu.page.toolbar.size(), 3u);
  EXPECT_EQ(menu.Activate({"list-view"})->path, "list");
  EXPECT_FALSE(menu.Activate({"grid-view"}));
  ASSERT_TRUE(menu.Activate({"view-toggle"}));
  EXPECT_EQ(menu.Activate({"view-toggle"})->library_operation, LibraryOperation::View);
  ASSERT_TRUE(menu.Activate({"settings"}));
  EXPECT_EQ(menu.Activate({"settings"})->action, Action::Settings);
  library->selected_path.clear();
  library->games.pop_back();
  menu = Build();
  EXPECT_EQ(menu.page.default_focus, "back");
  EXPECT_FALSE(menu.Activate({"game:two.iso"}));
  snapshot.screens.push_back(Screen::LibraryOptions);
  menu = Build();
  EXPECT_TRUE(menu.Activate({"settings"}));
}

TEST_F(BigPictureInGameMenuTest, CompletedStateOperationsNameTheirSlot)
{
  snapshot.screens.push_back(Screen::SaveStates);
  text.state_saved = "Saved";
  text.state_loaded = "Loaded";
  snapshot.result =
      Result{Command{snapshot.generation, 1, Action::SaveState, {}, 3}, ResultStatus::Succeeded};
  EXPECT_NE(Build().page.description.find("Slot 3: Saved"), std::string::npos);
  snapshot.screens.back() = Screen::LoadStates;
  snapshot.result->command.action = Action::LoadState;
  EXPECT_NE(Build().page.description.find("Slot 3: Loaded"), std::string::npos);
  // While the core is still working, nothing claims completion.
  snapshot.result->status = ResultStatus::Pending;
  snapshot.pending = snapshot.result->command;
  EXPECT_EQ(Build().page.description.find("Loaded"), std::string::npos);
}

TEST_F(BigPictureInGameMenuTest, GameFoldersListFoldersAndConfirmRemovalByPath)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Library, Screen::LibraryOptions};
  auto library = std::make_shared<LibrarySnapshot>();
  library->folders = {"C:/Games", "D:/More Games"};
  snapshot.host.library = library;
  text.library_actions[static_cast<std::size_t>(LibraryOperation::Folders)] = "Game Folders";
  text.library_actions[static_cast<std::size_t>(LibraryOperation::RemoveFolder)] = "Remove Folder";
  text.confirm_remove_folder = "Remove?";
  auto menu = Build();
  ASSERT_TRUE(menu.Activate({"folders"}));
  EXPECT_EQ(menu.Activate({"folders"})->library_operation, LibraryOperation::Folders);

  snapshot.screens.push_back(Screen::GameFolders);
  menu = Build();
  EXPECT_EQ(menu.page.title, "Game Folders");
  EXPECT_EQ(menu.page.default_focus, "folder:C:/Games");
  ASSERT_TRUE(menu.Activate({"folder:D:/More Games"}));
  EXPECT_EQ(menu.Activate({"folder:D:/More Games"})->library_operation,
            LibraryOperation::RemoveFolder);
  EXPECT_EQ(menu.Activate({"folder:D:/More Games"})->path, "D:/More Games");
  for (const auto* id : {"add-folder", "recursive", "refresh"})
    EXPECT_TRUE(menu.Activate({id})) << id;

  library->folders.clear();
  menu = Build();
  EXPECT_EQ(menu.page.default_focus, "add-folder");

  Command remove{snapshot.generation, 1, Action::Library, "C:/Games"};
  remove.library_operation = LibraryOperation::RemoveFolder;
  snapshot.confirmation = remove;
  snapshot.screens.push_back(Screen::Confirmation);
  menu = Build();
  EXPECT_EQ(menu.page.title, "Remove Folder");
  EXPECT_EQ(menu.page.description, "Remove?\nC:/Games");
  EXPECT_EQ(menu.page.default_focus, "cancel");
  EXPECT_EQ(menu.Activate({"confirm"})->confirmation_id, 1u);
}

TEST_F(BigPictureInGameMenuTest, GameOptionsActOnTheSelectedGameAndBootItsStates)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Library};
  auto library = std::make_shared<LibrarySnapshot>();
  LibraryGame game;
  game.path = "game.iso";
  game.title = "Game";
  game.game_id = "GTEST1";
  game.details = "Details";
  LibraryGame executable;
  executable.path = "app.dol";
  executable.title = "App";
  executable.played_ms = 1000;
  library->games = {game, executable};
  snapshot.host.library = library;
  auto menu = Build();
  ASSERT_TRUE(menu.Activate({"options:game:game.iso"}));
  EXPECT_EQ(menu.Activate({"options:game:game.iso"})->action, Action::GameOptions);
  EXPECT_EQ(menu.Activate({"options:game:game.iso"})->path, "game.iso");

  snapshot.screens.push_back(Screen::GameOptions);
  snapshot.options_path = "game.iso";
  menu = Build();
  EXPECT_EQ(menu.page.title, "Game");
  EXPECT_EQ(menu.page.description, "Details");
  EXPECT_EQ(menu.page.default_focus, "start");
  for (const auto* id : {"start", "load-state", "properties"})
  {
    ASSERT_TRUE(menu.Activate({id})) << id;
    EXPECT_EQ(menu.Activate({id})->path, "game.iso") << id;
  }
  EXPECT_EQ(menu.Activate({"start"})->action, Action::Boot);
  EXPECT_EQ(menu.Activate({"load-state"})->action, Action::GameStates);
  EXPECT_EQ(menu.Activate({"properties"})->action, Action::GameProperties);
  // No play time to reset yet.
  EXPECT_FALSE(menu.Activate({"reset-play-time"}));

  // Executables have no game ID for per-game settings.
  snapshot.options_path = "app.dol";
  menu = Build();
  EXPECT_FALSE(menu.Activate({"properties"}));
  ASSERT_TRUE(menu.Activate({"reset-play-time"}));
  EXPECT_EQ(menu.Activate({"reset-play-time"})->action, Action::ResetPlayTime);

  snapshot.options_path = "game.iso";
  snapshot.screens.push_back(Screen::LoadStates);
  snapshot.host.state_slots_ready = true;
  snapshot.host.state_slots[1] = {true, true, "Saved"};
  menu = Build();
  ASSERT_TRUE(menu.Activate({"slot-2"}));
  EXPECT_EQ(menu.Activate({"slot-2"})->action, Action::BootState);
  EXPECT_EQ(menu.Activate({"slot-2"})->slot, 2);
  EXPECT_EQ(menu.Activate({"slot-2"})->path, "game.iso");
  EXPECT_FALSE(menu.Activate({"slot-1"}));
  ASSERT_TRUE(menu.Activate({"refresh"}));
  EXPECT_EQ(menu.Activate({"refresh"})->action, Action::GameStates);
  EXPECT_EQ(menu.Activate({"refresh"})->path, "game.iso");

  snapshot.host.hardcore = true;
  menu = Build();
  EXPECT_FALSE(menu.Activate({"slot-2"}));
  snapshot.host.hardcore = false;
  snapshot.host.lifecycle = Lifecycle::Running;
  snapshot.screens = {Screen::Library};
  menu = Build();
  EXPECT_FALSE(menu.Activate({"options:game:game.iso"}));
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

TEST_F(BigPictureInGameMenuTest, MissingDiscPickerNeverCreatesADesktopFallback)
{
  snapshot.host.can_change_disc = true;
  const auto menu = Build();
  EXPECT_FALSE(menu.Activate({"disc"}));
  const auto disc =
      std::ranges::find(menu.page.items, "disc", &VideoCommon::BigPicture::MenuItem::id);
  ASSERT_NE(disc, menu.page.items.end());
  EXPECT_FALSE(disc->enabled);
}

TEST_F(BigPictureInGameMenuTest, GameAndGlobalSettingsUseTheSameSettingsRenderer)
{
  snapshot.screens = {Screen::Settings};
  snapshot.host.settings_menu = std::make_shared<SettingsMenuSnapshot>();
  const auto game = Build().page;
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  const auto global = Build().page;
  EXPECT_TRUE(game.overlay);
  EXPECT_FALSE(global.overlay);
  EXPECT_EQ(game.theme, global.theme);
  EXPECT_EQ(game.presentation, global.presentation);
  EXPECT_EQ(game.columns, global.columns);
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
  EXPECT_EQ(menu.page.default_focus, "back");
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
  EXPECT_EQ(menu.page.presentation, VideoCommon::BigPicture::MenuPresentation::Launch);
  EXPECT_EQ(menu.page.default_focus, "open-file");
  EXPECT_FALSE(
      std::ranges::any_of(menu.page.items, [](const auto& item) { return item.id == "back"; }));
  ASSERT_EQ(menu.page.toolbar.size(), 1u);
  EXPECT_EQ(menu.page.toolbar.front().id, "back");
  EXPECT_EQ(menu.page.items.front().image_key, "home:start-file");
  EXPECT_EQ(menu.Activate({"back"})->action, Action::Back);
}

TEST_F(BigPictureInGameMenuTest, LaunchPickerRetainsParentTilesAndDirectoryRequests)
{
  snapshot.mode = Mode::Library;
  snapshot.host.lifecycle = Lifecycle::Idle;
  snapshot.screens = {Screen::Home, Screen::StartGame, Screen::FilePicker};
  auto library = std::make_shared<LibrarySnapshot>();
  library->directory = "C:/Games/日本語";
  library->parent_directory = "C:/Games";
  library->picker_revision = 42;
  library->files = {{"game.rvz", "Game.rvz", false}};
  snapshot.host.library = library;
  auto menu = Build();
  EXPECT_EQ(menu.page.presentation, VideoCommon::BigPicture::MenuPresentation::FilePicker);
  ASSERT_NE(menu.page.background, nullptr);
  EXPECT_EQ(menu.page.background->id, "start-game");
  EXPECT_EQ(menu.page.context.label, library->directory);
  EXPECT_EQ(menu.Activate({"parent"})->path, library->parent_directory);
  EXPECT_EQ(menu.Activate({"parent"})->library_revision, 42u);
  EXPECT_EQ(menu.Activate({"file:game.rvz"})->action, Action::Boot);
  EXPECT_EQ(menu.Activate({"file:game.rvz"})->path, "game.rvz");
  EXPECT_EQ(menu.Activate({"path"})->library_operation, LibraryOperation::EditPath);
  snapshot.pending = Command{};
  menu = Build();
  EXPECT_TRUE(menu.requests.empty());
  EXPECT_FALSE(menu.page.context.enabled);
  EXPECT_TRUE(std::ranges::none_of(menu.page.items, &VideoCommon::BigPicture::MenuItem::enabled));
}

TEST_F(BigPictureInGameMenuTest, BriefPendingCommandsKeepTheIdleMenuAndHoldPresses)
{
  using Hold = VideoCommon::BigPicture::PendingMenuHold;
  Hold hold;
  const auto start = Hold::Clock::now();
  auto idle_snapshot = snapshot;
  auto menu = Build();
  ASSERT_FALSE(menu.requests.empty());
  const auto id = menu.page.id;
  const auto item = menu.requests.begin()->first;

  // A pending snapshot disables the menu, but the idle one stays on screen during the grace.
  snapshot.pending = Command{snapshot.generation, 1, Action::Screenshot};
  hold.Replace(&idle_snapshot, menu, snapshot, start);
  menu = Build();
  EXPECT_TRUE(menu.requests.empty());
  const auto* visible = &hold.Visible(snapshot, menu, start + std::chrono::milliseconds(100));
  EXPECT_NE(visible, &menu);
  EXPECT_FALSE(visible->requests.empty());
  EXPECT_EQ(&hold.Visible(snapshot, menu, start + Hold::GRACE), &menu);

  // A press made meanwhile runs nothing, then applies to the idle page that follows.
  EXPECT_FALSE(hold.Activate(snapshot, *visible, {item}));
  auto pending_snapshot = snapshot;
  snapshot.pending.reset();
  hold.Replace(&pending_snapshot, menu, snapshot, start + std::chrono::milliseconds(120));
  menu = Build();
  EXPECT_EQ(&hold.Visible(snapshot, menu, start), &menu);
  ASSERT_EQ(menu.page.id, id);
  const auto request = hold.Activate(snapshot, menu, {});
  ASSERT_TRUE(request);
  EXPECT_EQ(request->action, menu.requests.at(item).action);
  // It applies once.
  EXPECT_FALSE(hold.Activate(snapshot, menu, {}));

  // A held press is dropped when a different page follows, and a new press replaces it.
  EXPECT_FALSE(hold.Activate(pending_snapshot, menu, {item}));
  auto other = menu;
  other.page.id = "elsewhere";
  EXPECT_FALSE(hold.Activate(snapshot, other, {}));
  EXPECT_FALSE(hold.Activate(pending_snapshot, menu, {item}));
  EXPECT_FALSE(hold.Activate(snapshot, menu, {"missing"}));
  EXPECT_FALSE(hold.Activate(snapshot, menu, {}));
  // An input reset (e.g. losing focus) discards a held press.
  EXPECT_FALSE(hold.Activate(pending_snapshot, menu, {item}));
  hold.DiscardHeld();
  EXPECT_FALSE(hold.Activate(snapshot, menu, {}));
}
}  // namespace
