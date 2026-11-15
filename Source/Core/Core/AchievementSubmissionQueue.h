// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

// Persistent outbox for RetroAchievements submissions (achievement unlocks and leaderboard
// entries) that could not reach the server. Entries are kept on disk until the server either
// accepts or definitively rejects them, so they survive closing the game or the app.
//
// API tokens are never written to disk. Requests are rebuilt with the current token when they
// are resubmitted, along with a fresh offset that tells the server when the unlock happened.
class AchievementSubmissionQueue
{
public:
  enum class Type
  {
    Achievement,
    Leaderboard,
  };

  struct Entry
  {
    u64 id = 0;
    Type type = Type::Achievement;
    std::string username;
    // Achievement ID or leaderboard ID.
    u32 target_id = 0;
    // Achievements only.
    bool hardcore = false;
    // Leaderboards only.
    s32 score = 0;
    std::string game_hash;
    // The dorequest URL the original request was sent to.
    std::string url;
    // Unix time (seconds) of the unlock or leaderboard completion.
    s64 timestamp = 0;
  };

  enum class Outcome
  {
    // The server accepted the submission (or already had it).
    Delivered,
    // The server refused the submission and will keep refusing it; drop it.
    Rejected,
    // Network or server trouble, or a credentials problem; keep it and try again later.
    RetryLater,
  };

  // Returns an entry for requests that can be deferred (award achievement and submit leaderboard
  // entry), or nothing for every other request. `now` is the current Unix time in seconds.
  static std::optional<Entry> ParseRequest(std::string_view url, std::string_view post_data,
                                           s64 now);

  // Builds signed POST data for resubmitting an entry. Returns nothing if rcheevos refuses to
  // build the request.
  static std::optional<std::string> BuildPostData(const Entry& entry, const std::string& api_token,
                                                  s64 now);

  // Decides what to do with an entry after resubmitting it. `http_status` is 0 or negative when
  // no response was received at all.
  static Outcome ClassifyResponse(const Entry& entry, int http_status, std::string_view body);

  explicit AchievementSubmissionQueue(std::string file_path);

  // Replaces the in-memory queue with the contents of the file, if it exists.
  void Load();

  // Appends an entry, assigning it a new ID, and saves the queue. Returns the assigned ID.
  u64 Add(Entry entry);
  void Remove(u64 id);
  std::vector<Entry> GetEntries() const;
  size_t Size() const;
  bool IsEmpty() const;

private:
  void SaveLocked() const;

  const std::string m_file_path;
  mutable std::mutex m_lock;
  std::vector<Entry> m_entries;
  u64 m_next_id = 1;
};

#endif  // USE_RETRO_ACHIEVEMENTS
