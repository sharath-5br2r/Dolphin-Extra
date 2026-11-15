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
    add("search", O::Search);
    if (!library.query.empty())
      add("clear-search", O::ClearSearch);
    add("add-folder", O::AddFolder);
    add("folders", O::Folders, {}, std::to_string(library.folders.size()));
    add("refresh", O::Refresh);
    action("settings", Action::Settings);
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
  else if (screen == Screen::GameFolders)
  {
    page.id = "game-folders";
    page.title = text.library_actions[static_cast<std::size_t>(O::Folders)];
    page.description =
        library.folders.empty() ? text.library_no_folders : text.library_folders_help;
    if (library.scanning)
      page.description += "\n" + text.library_scanning;
    add("add-folder", O::AddFolder);
    add("recursive", O::Recursive, {}, library.recursive ? text.on : text.off);
    add("refresh", O::Refresh);
    for (const auto& folder : library.folders)
    {
      // Selecting a folder asks to remove it from the library.
      add("folder:" + folder, O::RemoveFolder, folder);
      page.items.back().label = folder;
    }
    page.items.push_back({"back", text.back, {}, true, {}, false, MenuIcon::Back});
    page.default_focus =
        library.folders.empty() ? "add-folder" : "folder:" + library.folders.front();
  }
  else if (screen == Screen::GameOptions)
  {
    page.id = "game-options";
    page.title = text.game_options;
    page.default_focus = "start";
    const auto game = std::ranges::find(library.games, snapshot.options_path, &LibraryGame::path);
    if (game != library.games.end())
    {
      page.title = game->title;
      page.description = game->details;
      const auto target = [&](std::string id, Action a, bool enabled = true) {
        if (auto* request = action(std::move(id), a, enabled))
          request->path = game->path;
      };
      target("start", Action::Boot);
      target("load-state", Action::GameStates);
      page.items.back().label = text.actions[static_cast<std::size_t>(Action::LoadState)];
      // Discs and Wii titles have IDs; settings for bare executables cannot be stored per game.
      target("properties", Action::GameProperties, !game->game_id.empty());
      target("reset-play-time", Action::ResetPlayTime, game->played_ms != 0);
    }
    page.items.push_back({"back", text.back, {}, true, {}, false, MenuIcon::Back});
  }
  else if (screen == Screen::FilePicker)
  {
    page.id = "file-picker";
    page.presentation = MenuPresentation::FilePicker;
    page.reset_on_revision = true;
    page.focus_revision = library.picker_revision;
    page.title =
        text.library_actions[static_cast<std::size_t>(library.picker_folders_only ? O::AddFolder :
                                                      library.picker_disc         ? O::OpenDisc :
                                                                                    O::OpenFile)];
    if (library.picker_boot_disc)
      page.title = text.library_actions[static_cast<std::size_t>(O::ChooseBootDisc)];
    else if (!library.picker_folders_only && !library.picker_disc)
      page.title = text.picker_title;
    page.context.label = library.picker_library ? text.library_disc_games : library.directory;
    page.description = library.picker_error;
    if (library.picker_loading)
      page.description += "\n" + text.library_scanning;
    if (library.picker_folders_only && !library.directory.empty() && !library.picker_loading)
    {
      add("choose-folder", O::ChooseFolder, library.directory, {}, true);
      page.toolbar.back().label = text.library_choose_folder;
    }
    if (library.picker_library)
      add("browse-discs", O::BrowseDiscs, {}, {}, true);
    else if (!library.picker_boot_disc)
    {
      add("roots", O::EnterDirectory, {}, {}, true);
      page.toolbar.back().label = text.library_roots;
    }
    if (!library.parent_directory.empty())
    {
      add("parent", O::EnterDirectory, library.parent_directory);
      page.items.back().label = text.library_parent;
      page.items.back().icon = MenuIcon::Folder;
    }
    if (!library.picker_boot_disc)
      add("path", O::EditPath, {}, {}, true);
    if (library.picker_folders_only)
      add("recursive", O::Recursive, {}, library.recursive ? text.on : text.off);
    for (const auto& file : library.files)
    {
      const auto id = "file:" + file.path;
      if (file.directory)
      {
        add(id, O::EnterDirectory, file.path);
        page.items.back().label = file.name;
        page.items.back().icon = MenuIcon::FolderSolid;
      }
      else
      {
        auto* request = action(id, library.picker_disc ? Action::ChangeDisc : Action::Boot);
        page.items.back().label = file.name;
        page.items.back().icon = library.picker_boot_disc ? MenuIcon::Disc : MenuIcon::File;
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
    page.title = library.grid ? text.game_grid : text.game_list;
    page.toolbar = {{"back", text.back, {}, true, {}, false, MenuIcon::Back},
                    {"grid-view", text.game_grid, {}, true, {}, library.grid, MenuIcon::Grid},
                    {"list-view", text.game_list, {}, true, {}, !library.grid, MenuIcon::List}};
    const auto shortcut = [&](const std::string& id, O operation) {
      NavigationRequest request{snapshot.generation, Action::Library};
      request.library_operation = operation;
      menu.requests.emplace(id, std::move(request));
    };
    shortcut(library.grid ? "list-view" : "grid-view", O::View);
    menu.requests.at(library.grid ? "list-view" : "grid-view").path =
        library.grid ? "list" : "grid";
    shortcut("view-toggle", O::View);
    shortcut("options", O::Options);
    shortcut("search", O::Search);
    shortcut("clear-search", O::ClearSearch);
    shortcut("add-folder", O::AddFolder);
    shortcut("refresh", O::Refresh);
    shortcut("clear-filters", O::ClearFilters);
    if (snapshot.availability[static_cast<std::size_t>(Action::Settings)] ==
        UnavailableReason::None)
      menu.requests.emplace("settings", NavigationRequest{snapshot.generation, Action::Settings});
    page.columns = library.grid ? 4 : 1;
    page.artwork = true;
    page.default_focus = "back";
    if (library.total_games == 0)
      page.context = {"add-folder",
                      text.library_actions[static_cast<std::size_t>(O::AddFolder)],
                      {},
                      true,
                      {},
                      false,
                      MenuIcon::Folder};
    else if (library.games.empty())
      page.context = {"clear-filters",
                      text.library_actions[static_cast<std::size_t>(O::ClearFilters)],
                      {},
                      true,
                      {},
                      false,
                      MenuIcon::Filter};
    const bool game_options =
        snapshot.availability[static_cast<std::size_t>(Action::GameOptions)] ==
        UnavailableReason::None;
    for (const auto& game : library.games)
    {
      const auto id = "game:" + game.path;
      if (game_options)
      {
        NavigationRequest options{snapshot.generation, Action::GameOptions};
        options.path = game.path;
        menu.requests.emplace("options:" + id, std::move(options));
      }
      auto* request = action(id, Action::Boot);
      page.items.back().label = game.title;
      if (page.items.back().enabled)
        page.items.back().detail = game.description;
      page.items.back().image_key = game.path;
      if (const auto cover = library.covers.find(game.path);
          cover != library.covers.end() && cover->second->width && cover->second->height)
        page.items.back().image_aspect =
            static_cast<float>(cover->second->width) / cover->second->height;
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
    page.context.enabled = false;
    for (auto& item : page.items)
      item.enabled = false;
    for (auto& item : page.toolbar)
      item.enabled = false;
  }
  return menu;
}
}  // namespace VideoCommon::BigPicture
