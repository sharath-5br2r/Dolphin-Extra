// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include <string>

#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Core/AchievementSubmissionQueue.h"

using Queue = AchievementSubmissionQueue;

namespace
{
constexpr std::string_view URL = "https://retroachievements.org/dorequest.php";
constexpr s64 NOW = 1'800'000'000;

Queue::Entry MakeAchievement(u32 id, std::string username = "Player")
{
  Queue::Entry entry;
  entry.type = Queue::Type::Achievement;
  entry.username = std::move(username);
  entry.target_id = id;
  entry.hardcore = true;
  entry.game_hash = "0123456789abcdef0123456789abcdef";
  entry.url = std::string(URL);
  entry.timestamp = NOW - 60;
  return entry;
}

class AchievementSubmissionQueueFile : public testing::Test
{
protected:
  void SetUp() override
  {
    m_dir = File::CreateTempDir();
    ASSERT_FALSE(m_dir.empty());
    m_path = m_dir + "/queue.json";
  }
  void TearDown() override { File::DeleteDirRecursively(m_dir); }

  std::string m_dir;
  std::string m_path;
};
}  // namespace

TEST(AchievementSubmissionQueue, ParsesAwardAchievement)
{
  const auto entry = Queue::ParseRequest(
      URL, "r=awardachievement&u=Some%20User&t=secret&a=1234&h=1&m=abcdef&v=sig", NOW);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->type, Queue::Type::Achievement);
  EXPECT_EQ(entry->username, "Some User");
  EXPECT_EQ(entry->target_id, 1234u);
  EXPECT_TRUE(entry->hardcore);
  EXPECT_EQ(entry->game_hash, "abcdef");
  EXPECT_EQ(entry->url, URL);
  EXPECT_EQ(entry->timestamp, NOW);
}

TEST(AchievementSubmissionQueue, KeepsOriginalUnlockTimeFromOffset)
{
  const auto entry = Queue::ParseRequest(URL, "r=awardachievement&u=p&t=x&a=5&h=0&o=90&v=sig", NOW);
  ASSERT_TRUE(entry.has_value());
  EXPECT_FALSE(entry->hardcore);
  EXPECT_EQ(entry->timestamp, NOW - 90);
}

TEST(AchievementSubmissionQueue, ParsesLeaderboardEntry)
{
  const auto entry =
      Queue::ParseRequest(URL, "r=submitlbentry&u=p&t=x&i=77&s=-250&m=hash&v=sig", NOW);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->type, Queue::Type::Leaderboard);
  EXPECT_EQ(entry->target_id, 77u);
  EXPECT_EQ(entry->score, -250);
}

TEST(AchievementSubmissionQueue, IgnoresOtherRequests)
{
  EXPECT_FALSE(Queue::ParseRequest(URL, "r=ping&u=p&t=x&g=1", NOW).has_value());
  EXPECT_FALSE(Queue::ParseRequest(URL, "r=login2&u=p&t=x", NOW).has_value());
  EXPECT_FALSE(Queue::ParseRequest(URL, "", NOW).has_value());
  // Missing or malformed required fields.
  EXPECT_FALSE(Queue::ParseRequest(URL, "r=awardachievement&u=p&t=x&h=1", NOW).has_value());
  EXPECT_FALSE(Queue::ParseRequest(URL, "r=awardachievement&t=x&a=1", NOW).has_value());
  EXPECT_FALSE(Queue::ParseRequest(URL, "r=awardachievement&u=p&a=12x", NOW).has_value());
  EXPECT_FALSE(Queue::ParseRequest(URL, "r=submitlbentry&u=p&i=3", NOW).has_value());
}

TEST(AchievementSubmissionQueue, RebuiltRequestUsesCurrentTokenAndOffset)
{
  const Queue::Entry entry = MakeAchievement(42);
  const auto post_data = Queue::BuildPostData(entry, "new-token", NOW);
  ASSERT_TRUE(post_data.has_value());
  EXPECT_NE(post_data->find("t=new-token"), std::string::npos);
  EXPECT_NE(post_data->find("o=60"), std::string::npos);
  EXPECT_NE(post_data->find("v="), std::string::npos);

  // The rebuilt request describes the same unlock.
  const auto reparsed = Queue::ParseRequest(entry.url, *post_data, NOW);
  ASSERT_TRUE(reparsed.has_value());
  EXPECT_EQ(reparsed->target_id, entry.target_id);
  EXPECT_EQ(reparsed->hardcore, entry.hardcore);
  EXPECT_EQ(reparsed->game_hash, entry.game_hash);
  EXPECT_EQ(reparsed->timestamp, entry.timestamp);
}

TEST(AchievementSubmissionQueue, RebuildsLeaderboardRequest)
{
  Queue::Entry entry = MakeAchievement(9);
  entry.type = Queue::Type::Leaderboard;
  entry.score = 31337;
  const auto post_data = Queue::BuildPostData(entry, "token", NOW);
  ASSERT_TRUE(post_data.has_value());
  const auto reparsed = Queue::ParseRequest(entry.url, *post_data, NOW);
  ASSERT_TRUE(reparsed.has_value());
  EXPECT_EQ(reparsed->type, Queue::Type::Leaderboard);
  EXPECT_EQ(reparsed->score, 31337);
  EXPECT_EQ(reparsed->timestamp, entry.timestamp);
}

TEST(AchievementSubmissionQueue, ClassifiesResponses)
{
  const Queue::Entry achievement = MakeAchievement(1);
  Queue::Entry leaderboard = achievement;
  leaderboard.type = Queue::Type::Leaderboard;

  using Outcome = Queue::Outcome;
  EXPECT_EQ(Queue::ClassifyResponse(achievement, 200, R"({"Success":true,"Score":10})"),
            Outcome::Delivered);
  EXPECT_EQ(
      Queue::ClassifyResponse(
          achievement, 200,
          R"({"Success":false,"Error":"User already has hardcore and regular achievements awarded."})"),
      Outcome::Delivered);
  EXPECT_EQ(Queue::ClassifyResponse(leaderboard, 200,
                                    R"({"Success":false,"Error":"User already has this entry"})"),
            Outcome::Rejected);
  EXPECT_EQ(Queue::ClassifyResponse(achievement, 404,
                                    R"({"Success":false,"Error":"Achievement not found"})"),
            Outcome::Rejected);

  // No connection, server trouble, credentials, or a captive portal page: try again later.
  EXPECT_EQ(Queue::ClassifyResponse(achievement, 0, ""), Outcome::RetryLater);
  EXPECT_EQ(Queue::ClassifyResponse(achievement, 503, "maintenance"), Outcome::RetryLater);
  EXPECT_EQ(Queue::ClassifyResponse(achievement, 429, R"({"Success":false})"), Outcome::RetryLater);
  EXPECT_EQ(
      Queue::ClassifyResponse(achievement, 401, R"({"Success":false,"Error":"Invalid token"})"),
      Outcome::RetryLater);
  EXPECT_EQ(Queue::ClassifyResponse(
                achievement, 200,
                R"({"Success":false,"Code":"expired_token","Error":"The token has expired."})"),
            Outcome::RetryLater);
  EXPECT_EQ(Queue::ClassifyResponse(achievement, 200, "<html>Sign in to Wi-Fi</html>"),
            Outcome::RetryLater);
}

TEST_F(AchievementSubmissionQueueFile, PersistsEntriesAcrossInstances)
{
  {
    Queue queue(m_path);
    queue.Load();
    EXPECT_TRUE(queue.IsEmpty());
    queue.Add(MakeAchievement(1));
    Queue::Entry leaderboard = MakeAchievement(2);
    leaderboard.type = Queue::Type::Leaderboard;
    leaderboard.score = -5;
    leaderboard.hardcore = false;
    queue.Add(leaderboard);
    EXPECT_EQ(queue.Size(), 2u);
  }

  Queue reloaded(m_path);
  reloaded.Load();
  const auto entries = reloaded.GetEntries();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].type, Queue::Type::Achievement);
  EXPECT_EQ(entries[0].target_id, 1u);
  EXPECT_TRUE(entries[0].hardcore);
  EXPECT_EQ(entries[0].username, "Player");
  EXPECT_EQ(entries[0].timestamp, NOW - 60);
  EXPECT_EQ(entries[0].url, URL);
  EXPECT_EQ(entries[1].type, Queue::Type::Leaderboard);
  EXPECT_EQ(entries[1].score, -5);
  EXPECT_NE(entries[0].id, entries[1].id);
}

TEST_F(AchievementSubmissionQueueFile, RemovingLastEntryDeletesFile)
{
  Queue queue(m_path);
  const u64 first = queue.Add(MakeAchievement(1));
  const u64 second = queue.Add(MakeAchievement(2));
  EXPECT_TRUE(File::Exists(m_path));

  queue.Remove(first);
  ASSERT_EQ(queue.Size(), 1u);
  EXPECT_EQ(queue.GetEntries()[0].id, second);

  queue.Remove(second);
  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_FALSE(File::Exists(m_path));
}

TEST_F(AchievementSubmissionQueueFile, SkipsMalformedEntriesAndNeverStoresTokens)
{
  ASSERT_TRUE(File::WriteStringToFile(
      m_path, R"({"version":1,"entries":[{"type":"bogus"},)"
              R"({"type":"achievement","username":"P","target_id":3,"url":"u","timestamp":5}]})"));
  Queue queue(m_path);
  queue.Load();
  ASSERT_EQ(queue.Size(), 1u);
  EXPECT_EQ(queue.GetEntries()[0].target_id, 3u);

  queue.Add(MakeAchievement(4));
  std::string contents;
  ASSERT_TRUE(File::ReadFileToString(m_path, contents));
  EXPECT_EQ(contents.find("token"), std::string::npos);
}

#endif  // USE_RETRO_ACHIEVEMENTS
