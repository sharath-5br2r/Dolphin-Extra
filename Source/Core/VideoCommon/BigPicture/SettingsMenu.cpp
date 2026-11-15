// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/SettingsMenu.h"

#include <string_view>

namespace VideoCommon::BigPicture
{
InGameMenu BuildSettingsMenu(const ::BigPicture::Snapshot& snapshot,
                             const ::BigPicture::NavigationText& text,
                             ::BigPicture::NavigationOptions options)
{
  using namespace ::BigPicture;
  const auto& view = *snapshot.host.settings_menu;
  InGameMenu menu;
  menu.page.theme = snapshot.host.menu_theme;
  menu.page.presentation = MenuPresentation::Settings;
  menu.page.id = view.id;
  menu.page.title = view.title;
  menu.page.description = view.description;
  menu.page.default_focus = view.default_focus;
  menu.page.overlay = snapshot.mode == Mode::InGame;
  menu.page.category_page = view.category_page;
  menu.page.busy = snapshot.pending.has_value() || view.capturing;
  if (view.text_input)
  {
    menu.page.presentation = MenuPresentation::List;
    menu.page.settings_text = true;
    menu.page.compact = true;
    menu.page.columns = 6;
    auto visible_draft = view.draft;
    if (visible_draft.size() > 120)
    {
      std::size_t start = visible_draft.size() - 120;
      while (start < visible_draft.size() &&
             (static_cast<unsigned char>(visible_draft[start]) & 0xc0) == 0x80)
        ++start;
      visible_draft = "..." + visible_draft.substr(start);
    }
    menu.page.description += "\n" + visible_draft + "_";
  }
  const auto add = [&](const SettingsRow& row, bool toolbar) {
    auto& item = (toolbar ? menu.page.toolbar : menu.page.items)
                     .emplace_back(row.id, row.label, row.detail, row.enabled && !snapshot.pending,
                                   std::string{}, row.active);
    item.value = row.value;
    if (!row.help.empty())
      item.detail = row.help;
    item.section = row.section;
    item.toggle = row.toggle;
    item.value_enabled = row.value_enabled;
    item.has_options = row.options || (view.category_page && row.navigation &&
                                       row.navigation->operation == SettingsOperation::Edit);
    const auto category = row.category.empty() ? row.id : row.category;
    if (row.id == "interface.big_picture_theme")
      item.icon = MenuIcon::Theme;
    else if (category == "General")
      item.icon = MenuIcon::CPU;
    else if (category == "GraphicsHacks")
      item.icon = MenuIcon::Wrench;
    else if (category == "GraphicsAdvanced")
      item.icon = MenuIcon::Sliders;
    else if (category == "Enhancements")
      item.icon = MenuIcon::Image;
    else if (category == "OSD")
      item.icon = MenuIcon::Speed;
    else if (category == "Advanced")
      item.icon = MenuIcon::Warning;
    else if (category == "Triforce")
      item.icon = MenuIcon::Puzzle;
    else if (category == "GameCube")
      item.icon = MenuIcon::Cube;
    else if (category == "MemoryCards")
      item.icon = MenuIcon::Memory;
    else if (category == "Wii")
      item.icon = MenuIcon::Remote;
    else if (category == "controllers")
      item.icon = MenuIcon::Controller;
    else if (category == "Paths")
      item.icon = MenuIcon::Folder;
    else if (category == "Achievements")
      item.icon = MenuIcon::Achievements;
    else if (row.id == "Audio" || row.id.starts_with("audio.") || category == "Audio")
      item.icon = MenuIcon::Audio;
    else if (category == "Interface")
      item.icon = MenuIcon::Window;
    else if (category == "Display" || row.id.starts_with("interface.start"))
      item.icon = MenuIcon::Display;
    else if (row.id == "global")
      item.icon = MenuIcon::Globe;
    else if (row.id == "Display" || row.id == "display.internal_resolution")
      item.icon = MenuIcon::Image;
    else if (row.id.starts_with("display."))
      item.icon = MenuIcon::Display;
    else if (row.id.starts_with("interface."))
      item.icon = MenuIcon::Theme;
    else if (row.id == "PostProcessing")
      item.icon = MenuIcon::Filter;
    else if (row.id == "reset")
      item.icon = MenuIcon::Reset;
    else if (row.id == "inherit")
      item.icon = MenuIcon::Back;
    else if (row.navigation && (row.navigation->operation == SettingsOperation::PresetFolder ||
                                row.navigation->operation == SettingsOperation::BrowseStart ||
                                row.navigation->operation == SettingsOperation::BrowseEnter))
      item.icon = MenuIcon::Folder;
    else if (row.navigation && row.navigation->operation == SettingsOperation::BrowseChoose)
      item.icon = row.id == "browse-use" ? MenuIcon::Check : MenuIcon::File;
    else if (row.navigation && row.navigation->operation == SettingsOperation::TextStart)
      item.icon = MenuIcon::Keyboard;
    else if (row.id == "game")
      item.icon = MenuIcon::Disc;
    else if (row.navigation && row.navigation->operation >= SettingsOperation::Controllers &&
             row.navigation->operation <= SettingsOperation::ClearProfile)
      item.icon = MenuIcon::Controller;
    if (row.id.starts_with("native."))
    {
      if (row.id.find("cursor") != std::string::npos || row.id.find("mouse") != std::string::npos)
        item.icon = MenuIcon::Mouse;
      else if (row.id.find("hotkey") != std::string::npos ||
               row.id.find("keyboard") != std::string::npos)
        item.icon = MenuIcon::Keyboard;
      else if (row.id.find("clock") != std::string::npos || row.id.find("rtc") != std::string::npos)
        item.icon = MenuIcon::Clock;
      else if (row.id.find("language") != std::string::npos ||
               row.id.find("region") != std::string::npos)
        item.icon = MenuIcon::Globe;
      else if (row.id.find("texture") != std::string::npos ||
               row.id.find("texfmt") != std::string::npos)
        item.icon = MenuIcon::Image;
      else if (row.id.find("path") != std::string::npos ||
               row.id.find("folder") != std::string::npos)
        item.icon = MenuIcon::Folder;
    }
    if (row.slider)
    {
      const auto& slider = *row.slider;
      item.slider = MenuSlider{slider.minimum, slider.maximum,       slider.step,
                               slider.value,   slider.minimum_label, slider.maximum_label};
      // Left/Right and the larger steps act on the page's slider.
      menu.page.decrease = row.id + ":decrease";
      menu.page.increase = row.id + ":increase";
      menu.page.decrease_large = row.id + ":decrease-large";
      menu.page.increase_large = row.id + ":increase-large";
    }
    if (!row.enabled || snapshot.pending)
      return;
    const auto request_for = [&](const std::optional<SettingsRequest>& navigation,
                                 const std::optional<Common::Settings::Request>& edit) {
      NavigationRequest request{snapshot.generation, Action::SettingsMenu};
      request.settings_menu = navigation;
      if (edit)
      {
        request.action = Action::EditSetting;
        request.setting = edit;
        request.settings_menu.reset();
      }
      return request;
    };
    if (row.slider)
    {
      const auto& slider = *row.slider;
      const std::pair<const char*, const std::optional<SettingsAction>*> steps[] = {
          {":decrease", &slider.decrease},
          {":increase", &slider.increase},
          {":decrease-large", &slider.decrease_large},
          {":increase-large", &slider.increase_large}};
      for (const auto& [suffix, action] : steps)
      {
        if (*action)
          menu.requests.emplace(row.id + suffix,
                                request_for((*action)->navigation, (*action)->edit));
      }
      if (slider.set.navigation || slider.set.edit)
        menu.requests.emplace(row.id + ":set", request_for(slider.set.navigation, slider.set.edit));
    }
    if (!row.navigation && !row.edit)
      return;
    if (item.has_options)
    {
      NavigationRequest options_request{snapshot.generation, Action::SettingsMenu};
      options_request.settings_menu = row.options ? row.options : row.navigation;
      menu.requests.emplace("edit:" + row.id, std::move(options_request));
    }
    menu.requests.emplace(row.id, request_for(row.navigation, row.edit));
  };
  for (const auto& row : view.toolbar)
    add(row, true);
  for (const auto& row : view.rows)
    add(row, false);
  if (view.preferences && !snapshot.pending)
  {
    menu.page.items.push_back({"swap", text.swap, options.swap_confirm_back ? text.on : text.off});
    menu.page.items.push_back(
        {"scale", text.scale, std::to_string(static_cast<int>(options.scale * 100)) + "%"});
    menu.page.items.push_back({"safe_area", text.safe_area,
                               std::to_string(static_cast<int>(options.safe_area * 100)) + "%"});
    const auto start = menu.page.items.size() - 3;
    for (std::size_t i = 0; i < 3; ++i)
    {
      auto& item = menu.page.items[start + i];
      item.value = item.detail;
      item.detail = text.settings_navigation_help[i];
      item.section = text.settings_navigation;
      item.icon = i == 0 ? MenuIcon::Controller : MenuIcon::Display;
    }
    menu.page.items[start].toggle = options.swap_confirm_back;
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
  }
  NavigationRequest back{snapshot.generation, Action::Back};
  if (view.back)
  {
    back.action = Action::SettingsMenu;
    back.settings_menu = view.back;
  }
  if (!snapshot.pending)
  {
    menu.page.toolbar.insert(menu.page.toolbar.begin(),
                             {"back", text.back, {}, true, {}, false, MenuIcon::Back});
    menu.requests.emplace("back", std::move(back));
    if (!view.text_input)
    {
      menu.page.items.push_back({"desktop-settings",
                                 text.actions[static_cast<size_t>(Action::DesktopSettings)],
                                 text.desktop_settings_help});
      menu.page.items.back().icon = MenuIcon::Settings;
      menu.requests.emplace("desktop-settings",
                            NavigationRequest{snapshot.generation, Action::DesktopSettings});
    }
  }
  if (snapshot.pending)
    menu.page.toolbar.insert(menu.page.toolbar.begin(),
                             {"back", text.back, {}, false, {}, false, MenuIcon::Back});
  if (view.capturing)
  {
    // The press being captured, including Back, must not also act in the menu.
    menu.requests.clear();
    for (auto& item : menu.page.items)
      item.enabled = false;
    for (auto& item : menu.page.toolbar)
      item.enabled = false;
  }
  return menu;
}
}  // namespace VideoCommon::BigPicture
