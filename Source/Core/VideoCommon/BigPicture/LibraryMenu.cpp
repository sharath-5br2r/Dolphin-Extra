// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/LibraryMenu.h"

#include <algorithm>

namespace VideoCommon::BigPicture
{
InGameMenu BuildLibraryMenu(const ::BigPicture::Snapshot& snapshot,
                            const ::BigPicture::NavigationText& text)
{
  using namespace ::BigPicture;
  using O = LibraryOperation;
  InGameMenu menu;
  auto& page = menu.page;
  page.theme = snapshot.host.menu_theme;
  const auto& library = *snapshot.host.library;
  const auto tail = [](const std::string& value) {
    if (value.size() <= 180)
      return value;
    std::size_t start = value.size() - 180;
    while (start < value.size() && (static_cast<unsigned char>(value[start]) & 0xc0) == 0x80)
      ++start;
    return std::string("…") + value.substr(start);
  };
  const auto screen = snapshot.screens.empty() ? Screen::Library : snapshot.screens.back();
  const auto add = [&](std::string id, O operation, std::string value = {}, std::string detail = {},
                       bool toolbar = false) {
    MenuItem item{id, text.library_actions[static_cast<std::size_t>(operation)], detail};
    item.icon = GetLibraryIcon(operation);
    (toolbar ? page.toolbar : page.items).push_back(std::move(item));
    NavigationRequest request{snapshot.generation, Action::Library};
    request.library_operation = operation;
    request.path = std::move(value);
    request.library_revision = library.picker_revision;
    menu.requests.emplace(std::move(id), std::move(request));
  };
  const auto action = [&](std::string id, Action a, bool enabled = true, int slot = 0,
                          bool toolbar = false) -> NavigationRequest* {
    const auto reason = snapshot.availability[static_cast<std::size_t>(a)];
    enabled = enabled && reason == UnavailableReason::None;
    (toolbar ? page.toolbar : page.items)
        .push_back({id,
                    text.actions[static_cast<std::size_t>(a)],
                    enabled ? std::string{} : text.reasons[static_cast<std::size_t>(reason)],
                    enabled,
                    {},
                    false,
                    GetActionIcon(a)});
    if (enabled)
    {
      NavigationRequest request{snapshot.generation, a};
      request.slot = slot;
      const auto it = menu.requests.emplace(std::move(id), std::move(request)).first;
      return &it->second;
    }
    return nullptr;
  };
  page.title = text.title;
  menu.requests.emplace("back", NavigationRequest{snapshot.generation, Action::Back});
  if (screen == Screen::LibraryOptions)
  {
    page.id = "library-options";
    page.title = text.library_actions[static_cast<std::size_t>(O::Options)];
    add("view", O::View, {}, library.grid ? text.library_grid : text.library_list);
    add("sort", O::Sort, {}, library.sort);
    add("platform", O::Platform, {},
        library.platform.empty() ? text.library_all : library.platform);
    add("region", O::Region, {}, library.region.empty() ? text.library_all : library.region);
    add("tag", O::Tag, {}, library.tag.empty() ? text.library_all : library.tag);
    add("clear", O::ClearFilters);
    add("file", O::OpenFile);
    action("fullscreen", Action::Fullscreen);
    for (int region : {0, 1, 2, 5})
    {
      if (!snapshot.host.boot_ipl_available[region])
        continue;
      action("ipl-" + std::to_string(region), Action::BootIPL, true, region);
      if (page.items.back().enabled)
        page.items.back().detail = region == 0 ? "NTSC-J" :
                                   region == 1 ? "NTSC-U" :
                                   region == 2 ? "PAL" :
                                                 "DEV";
    }
    if (snapshot.host.boot_system_menu_available)
      action("system-menu", Action::BootSystemMenu);
    action("desktop", Action::ReturnToDesktop);
    action("exit", Action::Exit);
    page.items.push_back({"back", text.back});
  }
  else if (screen == Screen::FilePicker)
  {
    page.id = "file-picker";
    page.reset_on_revision = true;
    page.focus_revision = library.picker_revision;
    page.title =
        text.library_actions[static_cast<std::size_t>(library.picker_folders_only ? O::AddFolder :
                                                      library.picker_disc         ? O::OpenDisc :
                                                                                    O::OpenFile)];
    page.description = library.picker_library ? text.library_disc_games : tail(library.directory);
    page.description += "\n" + library.picker_error;
    if (library.picker_loading)
      page.description += "\n" + text.library_scanning;
    if (library.picker_folders_only && !library.directory.empty() && !library.picker_loading)
    {
      add("choose-folder", O::ChooseFolder, library.directory, {}, true);
      page.toolbar.back().label = text.library_choose_folder;
    }
    if (library.picker_library)
      add("browse-discs", O::BrowseDiscs, {}, {}, true);
    else
    {
      add("roots", O::EnterDirectory, {}, {}, true);
      page.toolbar.back().label = text.library_roots;
    }
    if (!library.parent_directory.empty())
    {
      add("parent", O::EnterDirectory, library.parent_directory, {}, true);
      page.toolbar.back().label = text.library_parent;
    }
    add("path", O::EditPath, {}, {}, true);
    if (library.picker_folders_only)
      add("recursive", O::Recursive, {}, library.recursive ? text.on : text.off);
    for (const auto& file : library.files)
    {
      const auto id = "file:" + file.path;
      if (file.directory)
      {
        add(id, O::EnterDirectory, file.path);
        page.items.back().label = file.name + "/";
      }
      else
      {
        auto* request = action(id, library.picker_disc ? Action::ChangeDisc : Action::Boot);
        page.items.back().label = file.name;
        if (request)
        {
          request->path = file.path;
          request->library_revision = library.picker_revision;
        }
      }
    }
    page.default_focus = "roots";
    if (!library.files.empty())
      page.default_focus = "file:" + library.files.front().path;
  }
  else if (screen == Screen::TextInput)
  {
    page.id = "text-input";
    page.title = text.library_keyboard;
    page.description = tail(library.draft) + "_";
    if (library.native_text_input)
    {
      page.items.push_back({"back", text.library_cancel});
      page.default_focus = "back";
    }
    else
    {
      add("done", O::TextDone, {}, {}, true);
      add("erase", O::TextErase, {}, {}, true);
      add("clear-text", O::TextClear, {}, {}, true);
      add("space", O::TextKey, " ", {}, true);
      page.toolbar.back().label = text.library_space;
      page.toolbar.push_back({"back", text.library_cancel});
      page.columns = 6;
      page.compact = true;
      const std::string characters =
          "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/\\:._-@+~()[]{}!#$%&=,;'";
      for (std::size_t i = 0; i < characters.size(); ++i)
      {
        add("key-" + std::to_string(i), O::TextKey, std::string(1, characters[i]));
        page.items.back().label = std::string(1, characters[i]);
      }
      page.default_focus = "key-0";
    }
  }
  else
  {
    page.id = "library";
    page.presentation = MenuPresentation::Library;
    page.focus_revision = library.view_revision;
    page.description = library.scanning         ? text.library_scanning :
                       library.total_games == 0 ? text.library_empty :
                       library.games.empty()    ? text.library_no_matches :
                                                  std::string{};
    if (!library.query.empty())
      page.description += "\n" + tail(library.query);
    add("search", O::Search, {}, {}, true);
    if (!library.query.empty())
      add("clear-search", O::ClearSearch, {}, {}, true);
    add("options", O::Options, {}, {}, true);
    add("view-toggle", O::View, {}, {}, true);
    page.toolbar.back().label = library.grid ? text.library_list : text.library_grid;
    page.toolbar.back().icon = library.grid ? MenuIcon::List : MenuIcon::Grid;
    action("settings", Action::Settings, true, 0, true);
    add("add-folder", O::AddFolder, {}, {}, true);
    add("refresh", O::Refresh, {}, {}, true);
    page.columns = library.grid ? 4 : 1;
    page.artwork = true;
    page.default_focus = "refresh";
    for (const auto& game : library.games)
    {
      const auto id = "game:" + game.path;
      auto* request = action(id, Action::Boot);
      page.items.back().label = game.title;
      if (page.items.back().enabled)
        page.items.back().detail = game.description;
      page.items.back().image_key = game.path;
      page.items.back().icon = MenuIcon::None;
      page.items.back().preview_detail = game.details;
      if (request)
        request->path = game.path;
      if (game.path == library.selected_path && page.items.back().enabled)
        page.default_focus = id;
    }
    if (!snapshot.host.launch_path.empty() && library.total_games == 0)
    {
      auto* request = action("default-game", Action::Boot);
      page.items.back().label = snapshot.host.launch_title;
      if (request)
        request->path = snapshot.host.launch_path;
    }
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
