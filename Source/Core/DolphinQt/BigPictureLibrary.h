// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <QObject>

#include "Common/BigPicture/Library.h"
#include "Common/WorkQueueThread.h"

class BigPictureLibrary final : public QObject
{
  Q_OBJECT
public:
  struct Callbacks
  {
    std::function<void()> changed;
    std::function<void(const std::string&)> add_folder;
    std::function<void()> refresh;
    std::function<std::vector<unsigned char>(const std::string&)> read_cover;
    std::function<bool()> recursive;
    std::function<void(bool)> set_recursive;
    std::function<bool()> native_text_input;
  };
  explicit BigPictureLibrary(QObject* parent, Callbacks callbacks);
  ~BigPictureLibrary() override;
  void SetGames(std::vector<BigPicture::LibraryGame> games);
  void SetScanning(bool scanning);
  void SetInitialSelection(std::string path);
  void SetDiscContext(std::string path, std::string game_id, std::string title,
                      std::vector<std::string> folders);
  std::shared_ptr<const BigPicture::LibrarySnapshot> GetSnapshot() const { return m_snapshot; }
  // Returns a translated error, or an empty string on success. All mutations run on Qt's owner.
  std::string Execute(BigPicture::LibraryOperation operation, const std::string& value,
                      std::uint64_t revision);

private:
  struct Job
  {
    bool cover = false;
    std::uint64_t revision = 0;
    std::string path;
    bool folders_only = false;
    bool disc = false;
    std::vector<unsigned char> bytes;
  };
  void RunJob(Job job);
  void Publish(bool filter = false);
  void Browse(const std::string& path);
  void RequestCover(const std::string& path, std::uint64_t revision);
  void OpenDiscLibrary();
  std::string DiscDirectory() const;
  std::string m_disc_path;
  std::string m_disc_game_id;
  std::string m_disc_title;
  std::vector<std::string> m_disc_folders;
  std::vector<BigPicture::LibraryGame> m_games;
  std::optional<std::vector<BigPicture::LibraryGame>> m_pending_games;
  BigPicture::LibrarySnapshot m_state;
  std::shared_ptr<const BigPicture::LibrarySnapshot> m_snapshot;
  Callbacks m_callbacks;
  Common::WorkQueueThread<Job> m_worker;
  std::set<std::pair<std::uint64_t, std::string>> m_pending_covers;
  std::map<std::string, std::uint64_t> m_cover_use;
  std::uint64_t m_cover_clock = 0;
  bool m_directory_pending = false;
  std::optional<Job> m_next_directory;
  unsigned m_sort = 0;
  std::atomic<bool> m_destroying = false;
};
