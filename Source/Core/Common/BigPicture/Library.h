// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace BigPicture
{
enum class LibraryOperation
{
  None,
  Select,
  Options,
  Search,
  AddFolder,
  OpenFile,
  OpenDisc,
  EnterDirectory,
  ChooseFolder,
  Refresh,
  View,
  Sort,
  Platform,
  Region,
  Tag,
  ClearFilters,
  TextKey,
  TextErase,
  TextDone,
  EditPath,
  Cover,
  Recursive,
  ClearSearch,
  TextClear,
  TextReplace,
  BrowseDiscs,
  ControllerKeyboard,
  Count
};

struct LibraryGame
{
  std::string path;
  std::string title;
  std::string game_id;
  std::string platform;
  std::string region;
  std::string description;
  std::vector<std::string> tags;
  std::uint64_t played_ms = 0;
  std::uint64_t size = 0;
  std::string details;
};

struct LibraryImage
{
  std::uint64_t revision = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> rgba;
};

struct LibraryFile
{
  std::string path;
  std::string name;
  bool directory = false;
};

// Published by the Qt owner. Covers are decoded off-thread, bounded thumbnails; GPU resources
// never cross this boundary. A scan or filter change cannot mutate a retained snapshot.
struct LibrarySnapshot
{
  std::uint64_t catalog_revision = 0;
  std::uint64_t view_revision = 0;
  std::uint64_t picker_revision = 0;
  std::vector<LibraryGame> games;
  std::string selected_path;
  std::size_t total_games = 0;
  bool scanning = false;
  bool grid = true;
  std::string query;
  std::string sort;
  std::string platform;
  std::string region;
  std::string tag;
  std::string directory;
  std::string parent_directory;
  std::vector<LibraryFile> files;
  bool picker_loading = false;
  bool picker_folders_only = false;
  bool picker_disc = false;
  bool picker_library = false;
  bool recursive = false;
  std::string picker_error;
  std::string draft;
  bool editing_path = false;
  bool native_text_input = false;
  std::map<std::string, std::shared_ptr<const LibraryImage>> covers;
  std::set<std::string> missing_covers;
};
}  // namespace BigPicture
