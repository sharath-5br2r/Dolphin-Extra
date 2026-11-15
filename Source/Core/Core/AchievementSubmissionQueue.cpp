// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Core/AchievementSubmissionQueue.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <utility>

#include <picojson.h>

#include <rcheevos/include/rc_api_request.h>
#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_error.h>

#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"

namespace
{
constexpr double FILE_VERSION = 1;
constexpr std::string_view TYPE_ACHIEVEMENT = "achievement";
constexpr std::string_view TYPE_LEADERBOARD = "leaderboard";

int HexValue(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

std::string UrlDecode(std::string_view encoded)
{
  std::string result;
  result.reserve(encoded.size());
  for (size_t i = 0; i < encoded.size(); ++i)
  {
    const char c = encoded[i];
    if (c == '+')
    {
      result.push_back(' ');
    }
    else if (c == '%' && i + 2 < encoded.size() && HexValue(encoded[i + 1]) >= 0 &&
             HexValue(encoded[i + 2]) >= 0)
    {
      result.push_back(static_cast<char>(HexValue(encoded[i + 1]) * 16 + HexValue(encoded[i + 2])));
      i += 2;
    }
    else
    {
      result.push_back(c);
    }
  }
  return result;
}

std::map<std::string, std::string, std::less<>> ParseFormData(std::string_view data)
{
  std::map<std::string, std::string, std::less<>> fields;
  while (!data.empty())
  {
    const size_t amp = data.find('&');
    const std::string_view pair = data.substr(0, amp);
    data = amp == std::string_view::npos ? std::string_view{} : data.substr(amp + 1);

    const size_t eq = pair.find('=');
    if (eq == std::string_view::npos)
      fields.emplace(UrlDecode(pair), std::string{});
    else
      fields.emplace(UrlDecode(pair.substr(0, eq)), UrlDecode(pair.substr(eq + 1)));
  }
  return fields;
}

template <typename T>
std::optional<T> ParseNumber(const std::map<std::string, std::string, std::less<>>& fields,
                             std::string_view key)
{
  const auto it = fields.find(key);
  if (it == fields.end())
    return std::nullopt;
  const std::string& text = it->second;
  T value{};
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || ptr != text.data() + text.size())
    return std::nullopt;
  return value;
}

std::string GetField(const std::map<std::string, std::string, std::less<>>& fields,
                     std::string_view key)
{
  const auto it = fields.find(key);
  return it == fields.end() ? std::string{} : it->second;
}

u32 SecondsSince(s64 timestamp, s64 now)
{
  if (now <= timestamp)
    return 0;
  return static_cast<u32>(std::min<s64>(now - timestamp, std::numeric_limits<u32>::max()));
}

bool IsRetryableStatus(int http_status)
{
  switch (http_status)
  {
  case 401:  // Unauthorized: the token is stale. Keep the entry until the user logs in again.
  case 403:
  case 408:  // Request Timeout
  case 429:  // Too Many Requests
    return true;
  default:
    // 5xx covers maintenance mode, gateway errors and Cloudflare's 52x codes.
    return http_status >= 500;
  }
}

picojson::value EntryToJson(const AchievementSubmissionQueue::Entry& entry)
{
  picojson::object obj;
  const bool is_achievement = entry.type == AchievementSubmissionQueue::Type::Achievement;
  obj.emplace("type", std::string(is_achievement ? TYPE_ACHIEVEMENT : TYPE_LEADERBOARD));
  obj.emplace("username", entry.username);
  obj.emplace("target_id", static_cast<double>(entry.target_id));
  obj.emplace("hardcore", entry.hardcore);
  obj.emplace("score", static_cast<double>(entry.score));
  obj.emplace("game_hash", entry.game_hash);
  obj.emplace("url", entry.url);
  obj.emplace("timestamp", static_cast<double>(entry.timestamp));
  return picojson::value(std::move(obj));
}

std::optional<AchievementSubmissionQueue::Entry> EntryFromJson(const picojson::value& value)
{
  if (!value.is<picojson::object>())
    return std::nullopt;
  const picojson::object& obj = value.get<picojson::object>();

  AchievementSubmissionQueue::Entry entry;
  const auto type = ReadStringFromJson(obj, "type");
  if (type == TYPE_ACHIEVEMENT)
    entry.type = AchievementSubmissionQueue::Type::Achievement;
  else if (type == TYPE_LEADERBOARD)
    entry.type = AchievementSubmissionQueue::Type::Leaderboard;
  else
    return std::nullopt;

  const auto username = ReadStringFromJson(obj, "username");
  const auto target_id = ReadNumericFromJson<u32>(obj, "target_id");
  const auto url = ReadStringFromJson(obj, "url");
  const auto timestamp = ReadNumericFromJson<s64>(obj, "timestamp");
  if (!username || username->empty() || !target_id || *target_id == 0 || !url || url->empty() ||
      !timestamp)
  {
    return std::nullopt;
  }

  entry.username = *username;
  entry.target_id = *target_id;
  entry.url = *url;
  entry.timestamp = *timestamp;
  entry.hardcore = ReadBoolFromJson(obj, "hardcore").value_or(false);
  entry.score = ReadNumericFromJson<s32>(obj, "score").value_or(0);
  entry.game_hash = ReadStringFromJson(obj, "game_hash").value_or("");
  return entry;
}
}  // namespace

std::optional<AchievementSubmissionQueue::Entry>
AchievementSubmissionQueue::ParseRequest(std::string_view url, std::string_view post_data, s64 now)
{
  if (url.empty() || post_data.empty())
    return std::nullopt;

  const auto fields = ParseFormData(post_data);
  const std::string request_type = GetField(fields, "r");

  Entry entry;
  std::optional<u32> target_id;
  if (request_type == "awardachievement")
  {
    entry.type = Type::Achievement;
    target_id = ParseNumber<u32>(fields, "a");
    entry.hardcore = ParseNumber<u32>(fields, "h").value_or(0) != 0;
  }
  else if (request_type == "submitlbentry")
  {
    entry.type = Type::Leaderboard;
    target_id = ParseNumber<u32>(fields, "i");
    const auto score = ParseNumber<s32>(fields, "s");
    if (!score)
      return std::nullopt;
    entry.score = *score;
  }
  else
  {
    return std::nullopt;
  }

  entry.username = GetField(fields, "u");
  if (!target_id || *target_id == 0 || entry.username.empty())
    return std::nullopt;

  entry.target_id = *target_id;
  entry.game_hash = GetField(fields, "m");
  entry.url = std::string(url);
  // rcheevos adds an offset ("o") when it is itself retrying; keep the original unlock time.
  entry.timestamp = now - ParseNumber<u32>(fields, "o").value_or(0);
  return entry;
}

std::optional<std::string>
AchievementSubmissionQueue::BuildPostData(const Entry& entry, const std::string& api_token, s64 now)
{
  rc_api_request_t request{};
  int result;
  // Only the POST data is used; the URL stays the one the original request went to.
  const rc_api_host_t host{};

  if (entry.type == Type::Achievement)
  {
    rc_api_award_achievement_request_t params{};
    params.username = entry.username.c_str();
    params.api_token = api_token.c_str();
    params.achievement_id = entry.target_id;
    params.hardcore = entry.hardcore ? 1 : 0;
    params.game_hash = entry.game_hash.empty() ? nullptr : entry.game_hash.c_str();
    params.seconds_since_unlock = SecondsSince(entry.timestamp, now);
    result = rc_api_init_award_achievement_request_hosted(&request, &params, &host);
  }
  else
  {
    rc_api_submit_lboard_entry_request_t params{};
    params.username = entry.username.c_str();
    params.api_token = api_token.c_str();
    params.leaderboard_id = entry.target_id;
    params.score = entry.score;
    params.game_hash = entry.game_hash.empty() ? nullptr : entry.game_hash.c_str();
    params.seconds_since_completion = SecondsSince(entry.timestamp, now);
    result = rc_api_init_submit_lboard_entry_request_hosted(&request, &params, &host);
  }

  std::optional<std::string> post_data;
  if (result == RC_OK && request.post_data)
    post_data = request.post_data;
  rc_api_destroy_request(&request);
  return post_data;
}

AchievementSubmissionQueue::Outcome
AchievementSubmissionQueue::ClassifyResponse(const Entry& entry, int http_status,
                                             std::string_view body)
{
  if (http_status <= 0 || body.empty() || IsRetryableStatus(http_status))
    return Outcome::RetryLater;

  picojson::value root;
  const std::string error = picojson::parse(root, std::string(body));
  if (!error.empty() || !root.is<picojson::object>())
  {
    // An HTML page (captive portal, proxy error) rather than an API response.
    return Outcome::RetryLater;
  }

  const picojson::object& obj = root.get<picojson::object>();
  if (ReadBoolFromJson(obj, "Success").value_or(false))
    return Outcome::Delivered;

  const std::string message = ReadStringFromJson(obj, "Error").value_or("");
  // The server reports achievements the user already has as a failure; they are delivered.
  if (entry.type == Type::Achievement && message.starts_with("User already has"))
    return Outcome::Delivered;

  const std::string code = ReadStringFromJson(obj, "Code").value_or("");
  if (code == "invalid_credentials" || code == "expired_token" || code == "access_denied")
    return Outcome::RetryLater;

  return Outcome::Rejected;
}

AchievementSubmissionQueue::AchievementSubmissionQueue(std::string file_path)
    : m_file_path(std::move(file_path))
{
}

void AchievementSubmissionQueue::Load()
{
  std::lock_guard lg{m_lock};
  m_entries.clear();
  m_next_id = 1;

  if (!File::Exists(m_file_path))
    return;

  picojson::value root;
  std::string error;
  if (!JsonFromFile(m_file_path, &root, &error) || !root.is<picojson::object>())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to read deferred submissions from {}: {}", m_file_path,
                  error);
    return;
  }

  const picojson::object& obj = root.get<picojson::object>();
  const auto entries = obj.find("entries");
  if (entries == obj.end() || !entries->second.is<picojson::array>())
    return;

  for (const picojson::value& value : entries->second.get<picojson::array>())
  {
    auto entry = EntryFromJson(value);
    if (!entry)
    {
      WARN_LOG_FMT(ACHIEVEMENTS, "Skipping malformed deferred submission in {}", m_file_path);
      continue;
    }
    entry->id = m_next_id++;
    m_entries.push_back(std::move(*entry));
  }

  if (!m_entries.empty())
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Loaded {} deferred RetroAchievements submission(s).",
                 m_entries.size());
  }
}

u64 AchievementSubmissionQueue::Add(Entry entry)
{
  std::lock_guard lg{m_lock};
  entry.id = m_next_id++;
  const u64 id = entry.id;
  m_entries.push_back(std::move(entry));
  SaveLocked();
  return id;
}

void AchievementSubmissionQueue::Remove(u64 id)
{
  std::lock_guard lg{m_lock};
  const auto it = std::ranges::find(m_entries, id, &Entry::id);
  if (it == m_entries.end())
    return;
  m_entries.erase(it);
  SaveLocked();
}

std::vector<AchievementSubmissionQueue::Entry> AchievementSubmissionQueue::GetEntries() const
{
  std::lock_guard lg{m_lock};
  return m_entries;
}

size_t AchievementSubmissionQueue::Size() const
{
  std::lock_guard lg{m_lock};
  return m_entries.size();
}

bool AchievementSubmissionQueue::IsEmpty() const
{
  std::lock_guard lg{m_lock};
  return m_entries.empty();
}

void AchievementSubmissionQueue::SaveLocked() const
{
  if (m_entries.empty())
  {
    if (File::Exists(m_file_path) && !File::Delete(m_file_path))
      ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to delete {}", m_file_path);
    return;
  }

  picojson::array entries;
  entries.reserve(m_entries.size());
  for (const Entry& entry : m_entries)
    entries.push_back(EntryToJson(entry));

  picojson::object root;
  root.emplace("version", FILE_VERSION);
  root.emplace("entries", std::move(entries));

  // Write to a temporary file first so a crash mid-write can't destroy the existing queue.
  const std::string temp_path = m_file_path + ".tmp";
  if (!JsonToFile(temp_path, picojson::value(std::move(root)), true) ||
      !File::RenameSync(temp_path, m_file_path))
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to save deferred submissions to {}", m_file_path);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
