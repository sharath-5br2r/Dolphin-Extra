// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/InGameMenu.h"
#include "VideoCommon/BigPicture/LibraryMenu.h"
#include "VideoCommon/BigPicture/SettingsMenu.h"

#include <string_view>
#include <utility>

namespace VideoCommon::BigPicture
{
std::optional<::BigPicture::NavigationRequest> InGameMenu::Activate(const MenuResult& result) const
{
  const auto it = requests.find(result.back ? "back" : result.activated.value_or(""));
  return it == requests.end() ? std::nullopt : std::optional(it->second);
}

InGameMenu BuildInGameMenu(const ::BigPicture::Snapshot& snapshot,
                           const ::BigPicture::NavigationText& text,
                           ::BigPicture::NavigationOptions options)
{
  using namespace ::BigPicture;
  InGameMenu menu;
  auto& page = menu.page;
  page.theme = snapshot.host.menu_theme;
  page.overlay = snapshot.mode == Mode::InGame;
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
    if (!snapshot.host.launch_path.empty())
    {
      add("boot", Action::Boot, snapshot.host.launch_title);
      if (auto it = menu.requests.find("boot"); it != menu.requests.end())
        it->second.path = snapshot.host.launch_path;
    }
    for (int i : {0, 1, 2, 5})
    {
      if (!snapshot.host.boot_ipl_available[i])
        continue;
      const auto id = "ipl-" + std::to_string(i);
      add(id, Action::BootIPL, i == 0 ? "NTSC-J" : i == 1 ? "NTSC-U" : i == 2 ? "PAL" : "DEV");
      if (auto it = menu.requests.find(id); it != menu.requests.end())
        it->second.slot = static_cast<int>(i);
    }
    if (snapshot.host.boot_system_menu_available)
      add("system-menu", Action::BootSystemMenu);
    add("back", Action::Back);
  }
  else if (screen == Screen::ExitMenu)
  {
    page.id = "exit-menu";
    page.title = text.home_labels[3];
    add("desktop", Action::ReturnToDesktop);
    add("exit", Action::Exit);
    add("back", Action::Back);
  }
  else if (snapshot.host.library &&
           (screen == Screen::Library || screen == Screen::LibraryOptions ||
            screen == Screen::FilePicker || screen == Screen::TextInput))
    return BuildLibraryMenu(snapshot, text);
  else if (screen == Screen::Confirmation && snapshot.confirmation)
  {
    const auto& command = *snapshot.confirmation;
    page.id = "confirmation";
    page.focus_revision = command.id;
    page.title = label(command.action);
    page.description = command.action == Action::Reset     ? text.confirm_reset :
                       command.action == Action::Exit      ? text.confirm_exit :
                       command.action == Action::SaveState ? text.confirm_overwrite :
                                                             text.confirm_stop;
    if (command.action == Action::Exit && snapshot.host.lifecycle == Lifecycle::Idle)
      page.description = text.confirm_exit_idle;
    if (command.action == Action::SaveState && command.slot >= 1 &&
        command.slot <= static_cast<int>(STATE_SLOT_COUNT))
      page.description += "\n" + text.slot_labels[command.slot - 1];
    page.dialog = true;
    page.default_focus = "cancel";
    add("confirm", Action::Confirm);
    page.items.back().label = text.confirm;
    if (auto it = menu.requests.find("confirm"); it != menu.requests.end())
      it->second.confirmation_id = command.id;
    add("cancel", Action::Cancel);
    page.items.back().label = text.cancel;
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
  else if (screen == Screen::SaveStates || screen == Screen::LoadStates)
  {
    const bool saving = screen == Screen::SaveStates;
    const auto action = saving ? Action::SaveState : Action::LoadState;
    page.id = saving ? "save-states" : "load-states";
    page.title = label(saving ? Action::SaveStates : Action::LoadStates);
    page.description = text.state_browser_help;
    page.columns = 4;
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
      page.items.back().icon = GetActionIcon(action);
      if (enabled)
      {
        NavigationRequest request{snapshot.generation, action};
        request.slot = static_cast<int>(slot + 1);
        menu.requests.emplace(id, std::move(request));
      }
    }
    add("refresh", Action::RefreshStates, {}, true);
    add("back", Action::Back, {}, true);
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
    page.title = text.title;
    page.context.label = snapshot.host.game_title;
    page.context.image_key = snapshot.host.game_path;
    page.default_focus = "resume";
    add("resume", Action::Resume);
    add("save", Action::SaveStates);
    add("load", Action::LoadStates);
    add("preferences", Action::Settings);
    add("disc", Action::ChangeDisc, text.desktop_disc_help);
    if (snapshot.host.library && menu.requests.contains("disc"))
    {
      menu.requests["disc"].action = Action::Library;
      menu.requests["disc"].library_operation = LibraryOperation::OpenDisc;
      page.items.back().detail.clear();
    }
    add("reset", Action::Reset);
    add("screenshot", Action::Screenshot);
    add("stop", Action::Stop, text.stop_help);
    add("exit", Action::Exit);
    page.items.push_back({"close", text.close});
    page.items.back().icon = MenuIcon::Back;
    menu.requests.emplace("close", NavigationRequest{snapshot.generation, Action::Back});
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
