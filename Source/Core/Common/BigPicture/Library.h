// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace BigPicture
{
constexpr std::uint32_t MAX_COVER_WIDTH = 512;
constexpr std::uint32_t MAX_COVER_HEIGHT = 768;
// Decoded covers retained by a library snapshot: enough for several grid pages of scrolling,
// with a memory bound for large custom artwork.
constexpr std::size_t MAX_DECODED_COVERS = 64;
constexpr std::size_t MAX_DECODED_COVER_BYTES = 64 * 1024 * 1024;

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
  ChooseBootDisc,
  Folders,
  RemoveFolder,
  Count
};

struct LibraryFile
{
  std::string path;
  std::string name;
  bool directory = false;
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
  std::vector<LibraryFile> discs;
};

struct LibraryImage
{
  std::uint64_t revision = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> rgba;
};

// Contents shared by every copy, as each published snapshot copies the library state. Copying is
// a pointer copy; the owning thread's edits copy the contents first while a published snapshot
// still holds them, so snapshots stay immutable and a large library is not copied per update.
template <typename Container>
class CopyOnWrite
{
public:
  CopyOnWrite() = default;
  CopyOnWrite(Container contents) : m_contents(std::make_shared<Container>(std::move(contents))) {}
  const Container& Get() const { return m_contents ? *m_contents : Empty(); }
  auto begin() const { return Get().begin(); }
  auto end() const { return Get().end(); }
  std::size_t size() const { return Get().size(); }
  bool empty() const { return Get().empty(); }
  void clear() { m_contents.reset(); }

protected:
  Container& Edit()
  {
    if (!m_contents)
      m_contents = std::make_shared<Container>();
    else if (m_contents.use_count() > 1)
      m_contents = std::make_shared<Container>(*m_contents);
    return *m_contents;
  }

private:
  static const Container& Empty()
  {
    static const Container empty;
    return empty;
  }
  std::shared_ptr<Container> m_contents;
};

template <typename T>
class SharedList final : public CopyOnWrite<std::vector<T>>
{
public:
  using CopyOnWrite<std::vector<T>>::CopyOnWrite;
  SharedList(std::initializer_list<T> items) : CopyOnWrite<std::vector<T>>(std::vector<T>(items)) {}
  const T& front() const { return this->Get().front(); }
  const T& operator[](std::size_t index) const { return this->Get()[index]; }
  void push_back(T item) { this->Edit().push_back(std::move(item)); }
  void pop_back() { this->Edit().pop_back(); }
};

class SharedStringSet final : public CopyOnWrite<std::set<std::string>>
{
public:
  using CopyOnWrite::CopyOnWrite;
  bool contains(const std::string& value) const { return Get().contains(value); }
  void insert(std::string value)
  {
    if (!contains(value))
      Edit().insert(std::move(value));
  }
  void erase(const std::string& value)
  {
    if (contains(value))
      Edit().erase(value);
  }
};

// Published by the Qt owner. Covers are decoded off-thread, bounded thumbnails; GPU resources
// never cross this boundary. A scan or filter change cannot mutate a retained snapshot.
struct LibrarySnapshot
{
  std::uint64_t catalog_revision = 0;
  std::uint64_t view_revision = 0;
  std::uint64_t picker_revision = 0;
  SharedList<LibraryGame> games;
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
  SharedList<LibraryFile> files;
  bool picker_loading = false;
  bool picker_folders_only = false;
  bool picker_disc = false;
  bool picker_library = false;
  bool picker_boot_disc = false;
  bool recursive = false;
  // The library's game folders, as configured.
  std::vector<std::string> folders;
  std::string picker_error;
  std::string draft;
  bool editing_path = false;
  bool native_text_input = false;
  std::map<std::string, std::shared_ptr<const LibraryImage>> covers;
  SharedStringSet missing_covers;
};
}  // namespace BigPicture
