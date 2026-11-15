// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/InGameMenu.h"
#include "VideoCommon/BigPicture/LibraryMenu.h"
#include "VideoCommon/BigPicture/SettingsMenu.h"

#include <algorithm>
#include <string_view>

#include <fmt/format.h>
#include <utility>

namespace VideoCommon::BigPicture
{
std::optional<::BigPicture::NavigationRequest> InGameMenu::Activate(const MenuResult& result) const
{
  const auto it = requests.find(result.back ? "back" : result.activated.value_or(""));
  if (it == requests.end())
    return std::nullopt;
  auto request = it->second;
  if (result.value && !result.back)
  {
    if (request.setting)
      request.setting->display_value = *result.value;
    else if (request.settings_menu)
      request.settings_menu->value = fmt::format("{}", *result.value);
  }
  return request;
}

void PendingMenuHold::Replace(const ::BigPicture::Snapshot* previous, InGameMenu& menu,
                              const ::BigPicture::Snapshot& next, Clock::time_point now)
{
  if (!next.pending || (m_idle && m_generation != next.generation))
    m_idle.reset();
  if (next.pending && previous && !previous->pending && previous->generation == next.generation)
  {
    m_idle = std::move(menu);
    m_generation = next.generation;
    m_since = now;
  }
}

const InGameMenu& PendingMenuHold::Visible(const ::BigPicture::Snapshot& snapshot,
                                           const InGameMenu& menu, Clock::time_point now) const
{
  return snapshot.pending && m_idle && m_generation == snapshot.generation &&
                 now - m_since < GRACE ?
             *m_idle :
             menu;
}

std::optional<::BigPicture::NavigationRequest>
PendingMenuHold::Activate(const ::BigPicture::Snapshot& snapshot, const InGameMenu& visible,
                          const MenuResult& result)
{
  const bool acted = result.activated || result.back;
  if (snapshot.pending)
  {
    if (acted)
      m_held = Held{snapshot.generation, visible.page.id, result};
    return std::nullopt;
  }
  if (acted)
  {
    m_held.reset();
    return visible.Activate(result);
  }
  const auto held = std::exchange(m_held, std::nullopt);
  if (!held || held->generation != snapshot.generation || held->page != visible.page.id)
    return std::nullopt;
  return visible.Activate(held->result);
}

void PendingMenuHold::Reset()
{
  m_idle.reset();
  m_held.reset();
}

InGameMenu BuildInGameMenu(const ::BigPicture::Snapshot& snapshot,
                           const ::BigPicture::NavigationText& text,
                           ::BigPicture::NavigationOptions options)
{
  using namespace ::BigPicture;
  InGameMenu menu;
  auto& page = menu.page;
  page.theme = snapshot.host.menu_theme;
  const auto label = [&](Action action) -> const std::string& {
    return text.actions[static_cast<std::size_t>(action)];
  };
  const auto reason_text = [&](UnavailableReason reason) -> const std::string& {
    return text.reasons[static_cast<std::size_t>(reason)];
  };
  const auto add = [&](std::string id, Action action, std::string detail = {},
                       bool toolbar = false) {
    const auto reason = snapshot.availability[static_cast<std::size_t>(action)];
    const bool enabled = reason == UnavailableReason::None;
    (toolbar ? page.toolbar : page.items)
        .push_back({id,
                    label(action),
                    enabled ? std::move(detail) : reason_text(reason),
                    enabled,
                    {},
                    false,
                    GetActionIcon(action)});
    if (enabled)
      menu.requests.emplace(std::move(id), NavigationRequest{snapshot.generation, action});
  };
  if (!snapshot.pending)
    menu.requests.emplace("back", NavigationRequest{snapshot.generation, Action::Back});

  const auto screen = snapshot.screens.empty() ? Screen::Pause : snapshot.screens.back();
  if (screen == Screen::About)
  {
    page.id = "about";
    page.title = text.about_title;
    page.description = text.about_description;
    page.dialog = true;
    add("back", Action::Back);
  }
  else if (screen == Screen::Home)
  {
    page.id = "home";
    page.title = "Dolphin";
    page.presentation = MenuPresentation::Landing;
    page.columns = 4;
    page.default_focus = "game-list";
    page.account_name = snapshot.host.achievements_username;
    const std::array actions{Action::ShowLibrary, Action::StartGame, Action::Settings,
                             Action::ExitMenu};
    const std::array ids{"game-list", "start-game", "preferences", "exit-menu"};
    const std::array images{"media-cdrom", "start-game", "applications-system", "exit"};
    for (std::size_t i = 0; i < actions.size(); ++i)
    {
      add(ids[i], actions[i], text.home_descriptions[i]);
      page.items.back().label = text.home_labels[i];
      page.items.back().image_key = std::string("home:") + images[i];
    }
    menu.requests["back"] = {snapshot.generation, Action::ExitMenu};
  }
  else if (screen == Screen::StartGame)
  {
    page.id = "start-game";
    page.title = text.home_labels[1];
    page.presentation = MenuPresentation::Launch;
    page.default_focus = "open-file";
    if (snapshot.host.library)
    {
      add("open-file", Action::Library);
      page.items.back().label =
          text.library_actions[static_cast<std::size_t>(LibraryOperation::OpenFile)];
      page.items.back().icon = MenuIcon::Folder;
      if (auto it = menu.requests.find("open-file"); it != menu.requests.end())
        it->second.library_operation = LibraryOperation::OpenFile;
    }
    else
      add("open-file", Action::OpenGame);
    page.items.back().label = text.start_file;
    if (page.items.back().enabled)
      page.items.back().detail = text.start_file_description;
    page.items.back().image_key = "home:start-file";
    if (!snapshot.host.launch_path.empty())
    {
      add("boot", Action::Boot, snapshot.host.launch_title);
      if (auto it = menu.requests.find("boot"); it != menu.requests.end())
        it->second.path = snapshot.host.launch_path;
      page.items.back().image_key = "home:drive-cdrom";
    }
    for (int i : {0, 1, 2, 5})
    {
      if (!snapshot.host.boot_ipl_available[i])
        continue;
      const auto id = "ipl-" + std::to_string(i);
      add(id, Action::BootIPL, i == 0 ? "NTSC-J" : i == 1 ? "NTSC-U" : i == 2 ? "PAL" : "DEV");
      page.items.back().label += std::string(" (") +
                                 (i == 0 ? "NTSC-J" :
                                  i == 1 ? "NTSC-U" :
                                  i == 2 ? "PAL" :
                                           "DEV") +
                                 ")";
      if (page.items.back().enabled)
        page.items.back().detail = text.start_ipl_description;
      page.items.back().image_key = "home:start-bios";
      if (auto it = menu.requests.find(id); it != menu.requests.end())
        it->second.slot = static_cast<int>(i);
    }
    if (snapshot.host.boot_system_menu_available)
    {
      add("system-menu", Action::BootSystemMenu);
      if (page.items.back().enabled)
        page.items.back().detail = text.start_wii_description;
      page.items.back().image_key = "home:start-bios";
    }
    add("back", Action::Back, {}, true);
  }
  else if (screen == Screen::ExitMenu)
  {
    page.id = "exit-menu";
    page.title = text.home_labels[3];
    page.dialog = true;
    page.default_focus = "desktop";
    auto home = snapshot;
    home.screens = {Screen::Home};
    home.pending.reset();
    page.background = std::make_shared<MenuPage>(BuildInGameMenu(home, text, options).page);
    add("desktop", Action::ReturnToDesktop);
    add("exit", Action::Exit);
    add("back", Action::Back);
  }
  else if (snapshot.host.library &&
           (screen == Screen::Library || screen == Screen::LibraryOptions ||
            screen == Screen::FilePicker || screen == Screen::TextInput ||
            screen == Screen::GameOptions || screen == Screen::GameFolders))
  {
    auto library_menu = BuildLibraryMenu(snapshot, text);
    if (screen == Screen::FilePicker && snapshot.screens.size() >= 2 &&
        snapshot.screens[snapshot.screens.size() - 2] == Screen::StartGame)
    {
      auto parent = snapshot;
      parent.screens.pop_back();
      parent.pending.reset();
      library_menu.page.background =
          std::make_shared<MenuPage>(BuildInGameMenu(parent, text, options).page);
    }
    return library_menu;
  }
  else if (screen == Screen::Continuation)
  {
    page.id = "continuation";
    page.title = text.continuation_title;
    page.description = snapshot.host.continuation_title + "\n" + text.continuation_description;
    page.dialog = true;
    page.default_focus = "continue";
    add("continue", Action::ContinueGame);
    page.items.back().icon = MenuIcon::Play;
    add("fresh", Action::FreshBoot);
    page.items.back().icon = MenuIcon::Reset;
    add("delete", Action::DeleteContinuation);
    page.items.back().icon = MenuIcon::Stop;
    add("back", Action::Back);
  }
  else if (screen == Screen::Confirmation && snapshot.confirmation)
  {
    const auto& command = *snapshot.confirmation;
    page.id = "confirmation";
    page.focus_revision = command.id;
    page.title = label(command.action);
    page.description = command.action == Action::Reset         ? text.confirm_reset :
                       command.action == Action::Exit          ? text.confirm_exit :
                       command.action == Action::SaveState     ? text.confirm_overwrite :
                       command.action == Action::ResetPlayTime ? text.confirm_reset_play_time :
                                                                 text.confirm_stop;
    if (command.action == Action::Library &&
        command.library_operation == LibraryOperation::RemoveFolder)
    {
      page.title = text.library_actions[static_cast<std::size_t>(LibraryOperation::RemoveFolder)];
      page.description = text.confirm_remove_folder + "\n" + command.path;
    }
    if (command.action == Action::ResetPlayTime && snapshot.host.library)
    {
      const auto& games = snapshot.host.library->games;
      if (const auto game = std::ranges::find(games, command.path, &LibraryGame::path);
          game != games.end())
        page.description += "\n" + game->title;
    }
    if (command.action == Action::Exit && snapshot.host.lifecycle == Lifecycle::Idle)
      page.description = text.confirm_exit_idle;
    if (command.action == Action::SaveState && command.slot >= 1 &&
        command.slot <= static_cast<int>(STATE_SLOT_COUNT))
      page.description += "\n" + text.slot_labels[command.slot - 1];
    page.dialog = true;
    page.default_focus = "cancel";
    if (command.action == Action::Exit && snapshot.host.lifecycle == Lifecycle::Idle)
    {
      auto home = snapshot;
      home.screens = {Screen::Home};
      home.confirmation.reset();
      home.pending.reset();
      page.background = std::make_shared<MenuPage>(BuildInGameMenu(home, text, options).page);
    }
    add("confirm", Action::Confirm);
    page.items.back().label = text.confirm;
    page.items.back().icon = GetActionIcon(command.action);
    if (auto it = menu.requests.find("confirm"); it != menu.requests.end())
      it->second.confirmation_id = command.id;
    if (IsStopAction(command.action) && snapshot.host.lifecycle != Lifecycle::Idle)
    {
      page.items.back().label = text.exit_without_saving;
      if (snapshot.host.can_save_continuation)
      {
        add("save-and-exit", Action::Confirm);
        page.items.back().label = text.exit_and_save;
        page.items.back().icon = GetActionIcon(Action::SaveState);
        if (auto it = menu.requests.find("save-and-exit"); it != menu.requests.end())
        {
          it->second.confirmation_id = command.id;
          it->second.slot = 1;
        }
      }
    }
    add("cancel", Action::Cancel);
    page.items.back().label = text.cancel;
    page.items.back().icon = MenuIcon::Back;
    return menu;
  }
  else if (screen == Screen::Error)
  {
    page.id = "error";
    page.focus_revision = snapshot.error ? snapshot.error->command.id : 0;
    page.title = text.error;
    if (snapshot.error)
      page.description = snapshot.error->message.empty() ? reason_text(snapshot.error->reason) :
                                                           snapshot.error->message;
    page.dialog = true;
    page.default_focus = "back";
    add("back", Action::Back);
    return menu;
  }
  else if (screen == Screen::Achievements)
  {
    page.id = "achievements";
    page.title = snapshot.host.game_title;
    page.presentation = MenuPresentation::Achievements;
    page.overlay = true;
    page.achievements = snapshot.host.achievements_logged_in ? snapshot.host.achievements : nullptr;
    page.empty_text = !snapshot.host.achievements_logged_in ?
                          reason_text(UnavailableReason::LoginRequired) :
                      !page.achievements ? text.achievements_loading :
                                           text.achievements_empty;
    add("back", Action::Back, {}, true);
  }
  else if (screen == Screen::SaveStates || screen == Screen::LoadStates)
  {
    const bool saving = screen == Screen::SaveStates;
    // From the library's game options, a slot boots that game from the state.
    const bool booting = !saving && snapshot.mode == Mode::Library;
    const auto action = saving  ? Action::SaveState :
                        booting ? Action::BootState :
                                  Action::LoadState;
    page.id = saving ? "save-states" : "load-states";
    page.title = label(saving ? Action::SaveStates : Action::LoadStates);
    page.description = text.state_browser_help;
    page.presentation = MenuPresentation::States;
    page.overlay = snapshot.mode == Mode::InGame;
    page.columns = 3;
    page.busy = snapshot.pending.has_value();
    const auto reason = snapshot.availability[static_cast<std::size_t>(action)];
    for (std::size_t slot = 0; slot < STATE_SLOT_COUNT; ++slot)
    {
      const auto& state = snapshot.host.state_slots[slot];
      const std::string id = "slot-" + std::to_string(slot + 1);
      const bool enabled = snapshot.host.state_slots_ready && reason == UnavailableReason::None &&
                           (saving || state.readable);
      const auto detail = reason != UnavailableReason::None ? reason_text(reason) :
                          !snapshot.host.state_slots_ready  ? text.loading_slots :
                          !state.occupied                   ? text.empty_slot :
                          !state.readable                   ? text.unknown_slot :
                                                              state.description;
      page.items.push_back({id, text.slot_labels[slot], detail, enabled});
      page.items.back().image_key = "state:" + std::to_string(slot);
      if (state.preview && state.preview->width && state.preview->height)
        page.items.back().image_aspect =
            static_cast<float>(state.preview->width) / state.preview->height;
      page.items.back().icon = GetActionIcon(action);
      if (enabled)
      {
        NavigationRequest request{snapshot.generation, action};
        request.slot = static_cast<int>(slot + 1);
        if (booting)
          request.path = snapshot.options_path;
        menu.requests.emplace(id, std::move(request));
      }
    }
    if (booting)
    {
      // Reread the slots of the same game; the controller keeps this browser on top.
      add("refresh", Action::GameStates, {}, true);
      page.toolbar.back().label = label(Action::RefreshStates);
      page.toolbar.back().icon = GetActionIcon(Action::RefreshStates);
      if (auto it = menu.requests.find("refresh"); it != menu.requests.end())
        it->second.path = snapshot.options_path;
    }
    else
      add("refresh", Action::RefreshStates, {}, true);
    add("back", Action::Back, {}, true);
    std::rotate(page.toolbar.begin(), page.toolbar.end() - 1, page.toolbar.end());
    const auto first = std::ranges::find_if(page.items, &MenuItem::enabled);
    page.default_focus = first == page.items.end() ? "refresh" : first->id;
  }
  else if (screen == Screen::Settings)
  {
    if (snapshot.host.settings_menu)
      return BuildSettingsMenu(snapshot, text, options);
    page.id = "preferences";
    page.title = text.preferences;
    page.items = {
        {"swap", text.swap, options.swap_confirm_back ? text.on : text.off},
        {"scale", text.scale, std::to_string(static_cast<int>(options.scale * 100)) + "%"},
        {"safe_area", text.safe_area,
         std::to_string(static_cast<int>(options.safe_area * 100)) + "%"}};
    for (const auto* id : {"swap", "scale", "safe_area"})
    {
      auto changed = options;
      if (std::string_view(id) == "swap")
        changed.swap_confirm_back = !changed.swap_confirm_back;
      else if (std::string_view(id) == "scale")
        changed.scale = changed.scale >= 1.5f ? 0.75f : changed.scale + 0.25f;
      else
        changed.safe_area = changed.safe_area >= 0.149f ? 0 : changed.safe_area + 0.05f;
      menu.requests.emplace(id, NavigationRequest{snapshot.generation, Action::Back, changed});
    }
    add("desktop-settings", Action::DesktopSettings, text.desktop_settings_help);
    add("back", Action::Back);
  }
  else if (snapshot.mode == Mode::Library)
  {
    page.id = "standalone";
    page.presentation = MenuPresentation::Landing;
    page.columns = 3;
    page.title = text.title;
    page.description = text.standalone_help;
    if (!snapshot.host.launch_path.empty())
    {
      add("boot", Action::Boot, snapshot.host.launch_title);
      if (auto it = menu.requests.find("boot"); it != menu.requests.end())
        it->second.path = snapshot.host.launch_path;
    }
    add("open-game", Action::OpenGame);
    add("preferences", Action::Settings);
    add("fullscreen", Action::Fullscreen);
    add("desktop", Action::ReturnToDesktop);
    add("exit", Action::Exit);
  }
  else
  {
    page.id = "root";
    page.presentation = MenuPresentation::Pause;
    page.overlay = true;
    page.title = text.title;
    page.context.label = snapshot.host.game_title;
    page.context.image_key = snapshot.host.game_path;
    page.context.detail = snapshot.host.game_details;
    page.play_time = snapshot.host.game_play_time;
    page.rich_presence = snapshot.host.game_rich_presence;
    page.achievement_progress = snapshot.host.game_achievement_progress;
    page.default_focus = "resume";
    add("resume", Action::Resume);
    // Returning to the game is an explicit resume, including after a state load or an
    // external pause invalidated the pause menu's ownership of the paused state.
    if (const auto resume = menu.requests.find("resume"); resume != menu.requests.end())
      menu.requests["back"] = resume->second;
    add("load", Action::LoadStates);
    add("save", Action::SaveStates);
    add("screenshot", Action::Screenshot);
    add("achievements", Action::Achievements);
    add("disc", Action::ChangeDisc);
    if (menu.requests.contains("disc"))
    {
      if (snapshot.host.library)
      {
        menu.requests["disc"].action = Action::Library;
        menu.requests["disc"].library_operation = LibraryOperation::OpenDisc;
      }
      else
      {
        menu.requests.erase("disc");
        page.items.back().enabled = false;
        page.items.back().detail = reason_text(UnavailableReason::Busy);
      }
    }
    add("reset", Action::Reset);
    add("preferences", Action::Settings);
    page.items.back().label = text.game_settings;
    if (auto it = menu.requests.find("preferences"); it != menu.requests.end())
      it->second.slot = 1;
    add("stop", Action::Stop);
    add("exit", Action::Exit);
  }
  if (snapshot.result && (snapshot.result->status == ResultStatus::Dispatched ||
                          snapshot.result->status == ResultStatus::Unavailable))
  {
    const auto detail =
        snapshot.result->status == ResultStatus::Dispatched ?
            (snapshot.result->command.action == Action::ChangeDisc ? text.disc_change_requested :
                                                                     text.requested) :
            reason_text(snapshot.result->reason);
    if (!detail.empty())
      page.description += "\n" + detail;
  }
  else if (snapshot.result && snapshot.result->status == ResultStatus::Succeeded &&
           (snapshot.result->command.action == Action::SaveState ||
            snapshot.result->command.action == Action::LoadState) &&
           snapshot.result->command.slot >= 1 &&
           snapshot.result->command.slot <= static_cast<int>(STATE_SLOT_COUNT))
  {
    // The core reported completion; name the slot rather than pointing at the on-screen message.
    page.description += "\n" + text.slot_labels[snapshot.result->command.slot - 1] + ": " +
                        (snapshot.result->command.action == Action::SaveState ? text.state_saved :
                                                                                text.state_loaded);
  }
  if (snapshot.pending)
  {
    menu.requests.clear();
    for (auto& item : page.items)
      item.enabled = false;
    for (auto& item : page.toolbar)
      item.enabled = false;
  }
  return menu;
}
}  // namespace VideoCommon::BigPicture
