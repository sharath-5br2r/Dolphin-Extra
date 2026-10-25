// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/InGameMenu.h"

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
  const auto label = [&](Action action) -> const std::string& {
    return text.actions[static_cast<std::size_t>(action)];
  };
  const auto reason_text = [&](UnavailableReason reason) -> const std::string& {
    return text.reasons[static_cast<std::size_t>(reason)];
  };
  const auto add = [&](std::string id, Action action, std::string detail = {}) {
    const auto reason = snapshot.availability[static_cast<std::size_t>(action)];
    const bool enabled = reason == UnavailableReason::None;
    page.items.push_back(
        {id, label(action), enabled ? std::move(detail) : reason_text(reason), enabled});
    if (enabled)
      menu.requests.emplace(std::move(id), NavigationRequest{snapshot.generation, action});
  };
  if (!snapshot.pending)
    menu.requests.emplace("back", NavigationRequest{snapshot.generation, Action::Back});

  const auto screen = snapshot.screens.empty() ? Screen::Pause : snapshot.screens.back();
  if (screen == Screen::Confirmation && snapshot.confirmation)
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
  if (screen == Screen::Error)
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
  if (screen == Screen::SaveStates || screen == Screen::LoadStates)
  {
    const bool saving = screen == Screen::SaveStates;
    const auto action = saving ? Action::SaveState : Action::LoadState;
    page.id = saving ? "save-states" : "load-states";
    page.title = label(saving ? Action::SaveStates : Action::LoadStates);
    page.description = text.state_browser_help;
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
      if (enabled)
      {
        NavigationRequest request{snapshot.generation, action};
        request.slot = static_cast<int>(slot + 1);
        menu.requests.emplace(id, std::move(request));
      }
    }
    add("refresh", Action::RefreshStates);
    add("back", Action::Back);
  }
  else if (screen == Screen::Settings)
  {
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
    page.title = text.title;
    page.description = snapshot.host.game_title;
    page.default_focus = "resume";
    add("resume", Action::Resume);
    add("save", Action::SaveStates);
    add("load", Action::LoadStates);
    add("preferences", Action::Settings);
    add("disc", Action::ChangeDisc, text.desktop_disc_help);
    add("reset", Action::Reset);
    add("screenshot", Action::Screenshot);
    add("stop", Action::Stop, text.stop_help);
    add("exit", Action::Exit);
    page.items.push_back({"close", text.close});
    menu.requests.emplace("close", NavigationRequest{snapshot.generation, Action::Back});
  }
  if (snapshot.result && (snapshot.result->status == ResultStatus::Dispatched ||
                          snapshot.result->status == ResultStatus::Unavailable))
  {
    const auto detail = snapshot.result->status == ResultStatus::Dispatched ?
                            text.requested :
                            reason_text(snapshot.result->reason);
    if (!detail.empty())
      page.description += "\n" + detail;
  }
  if (snapshot.pending)
  {
    menu.requests.clear();
    for (auto& item : page.items)
      item.enabled = false;
  }
  return menu;
}
}  // namespace VideoCommon::BigPicture
