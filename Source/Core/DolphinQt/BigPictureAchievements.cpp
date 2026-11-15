// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureAchievements.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include <QCoreApplication>
#include <QDateTime>

#include "Core/AchievementManager.h"

std::shared_ptr<const BigPicture::AchievementsSnapshot> BigPictureAchievements::Read()
{
  auto result = std::make_shared<BigPicture::AchievementsSnapshot>();
#ifdef USE_RETRO_ACHIEVEMENTS
  auto& manager = AchievementManager::GetInstance();
  std::lock_guard lock(manager.GetLock());
  auto* client = manager.GetClient();
  if (!client || !rc_client_get_user_info(client) || !manager.IsGameLoaded())
  {
    Clear();
    return result;
  }
  const auto tr = [](const char* text) {
    return QCoreApplication::translate("BigPictureAchievements", text);
  };
  std::map<std::string, std::shared_ptr<const BigPicture::LibraryImage>> used_images;
  const auto image = [&](const std::string& id, const AchievementManager::Badge& badge) {
    std::shared_ptr<const BigPicture::LibraryImage> pixels;
    if (badge.width && badge.height && badge.width <= 512 && badge.height <= 512 &&
        badge.data.size() == static_cast<std::size_t>(badge.width) * badge.height * 4)
    {
      const auto it = m_images.find(id);
      if (it != m_images.end() && it->second->width == badge.width &&
          it->second->height == badge.height &&
          std::equal(it->second->rgba.begin(), it->second->rgba.end(), badge.data.data(),
                     badge.data.data() + badge.data.size()))
        pixels = it->second;
      else
      {
        auto copy = std::make_shared<BigPicture::LibraryImage>();
        copy->width = badge.width;
        copy->height = badge.height;
        copy->rgba.assign(badge.data.data(), badge.data.data() + badge.data.size());
        pixels = std::move(copy);
      }
      used_images.emplace(id, pixels);
    }
    return pixels;
  };
  auto subsets = std::unique_ptr<rc_client_subset_list_t, decltype(&rc_client_destroy_subset_list)>(
      rc_client_create_subset_list(client), rc_client_destroy_subset_list);
  auto list =
      std::unique_ptr<rc_client_achievement_list_t, decltype(&rc_client_destroy_achievement_list)>(
          rc_client_create_achievement_list(client,
                                            RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                            RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS),
          rc_client_destroy_achievement_list);
  if (!subsets || !list)
    return result;
  // Progress buckets such as Almost There combine achievements from multiple subsets.
  // The lock-state list supplies their subset membership through the public API.
  auto membership =
      std::unique_ptr<rc_client_achievement_list_t, decltype(&rc_client_destroy_achievement_list)>(
          rc_client_create_achievement_list(client,
                                            RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                            RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE),
          rc_client_destroy_achievement_list);
  if (!membership)
    return result;
  u32 sole_subset = 0;
  for (u32 i = 0; i < subsets->num_subsets; ++i)
    if (subsets->subsets[i]->num_achievements)
      sole_subset = subsets->subsets[i]->id;
  std::map<u32, u32> owners;
  for (u32 b = 0; b < membership->num_buckets; ++b)
    for (u32 a = 0; a < membership->buckets[b].num_achievements; ++a)
      owners.emplace(membership->buckets[b].achievements[a]->id,
                     membership->buckets[b].subset_id ? membership->buckets[b].subset_id :
                                                        sole_subset);
  for (u32 i = 0; i < subsets->num_subsets; ++i)
  {
    const auto& source = *subsets->subsets[i];
    BigPicture::AchievementSubset subset;
    subset.id = source.id;
    subset.title = source.title ? source.title : "";
    subset.badge = image("subset:" + std::to_string(source.id), manager.GetSubsetBadge(source.id));
    rc_client_user_game_summary_t summary{};
    rc_client_get_user_subset_summary(client, source.id, &summary);
    subset.summary =
        tr("You have unlocked %1 of %2 achievements, earning %3 of %4 possible points.")
            .arg(summary.num_unlocked_achievements)
            .arg(summary.num_core_achievements)
            .arg(summary.points_unlocked)
            .arg(summary.points_core)
            .toStdString();
    subset.progress = summary.num_core_achievements ?
                          std::clamp(static_cast<float>(summary.num_unlocked_achievements) /
                                         summary.num_core_achievements,
                                     0.0f, 1.0f) :
                          0;
    subset.progress_label = tr("%1%").arg(qRound(subset.progress * 100)).toStdString();
    for (u32 b = 0; b < list->num_buckets; ++b)
    {
      const auto& bucket = list->buckets[b];
      const char* label =
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED     ? "Unlocked" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED       ? "Locked" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE ? "Almost There" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED ?
                                                                            "Recently Unlocked" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE ?
                                                                            "Active Challenges" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL  ? "Unofficial" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED ? "Unsupported" :
          bucket.bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED    ? "Unsynced" :
                                                                           "Achievements";
      BigPicture::AchievementGroup group{bucket.bucket_type, tr(label).toStdString()};
      for (u32 a = 0; a < bucket.num_achievements; ++a)
      {
        const auto& entry = *bucket.achievements[a];
        const auto owner = owners.find(entry.id);
        if (owner == owners.end() || owner->second != source.id)
          continue;
        BigPicture::Achievement achievement;
        achievement.id = entry.id;
        achievement.title = entry.title ? entry.title : "";
        achievement.description = entry.description ? entry.description : "";
        achievement.points = tr("%1 points").arg(entry.points).toStdString();
        achievement.unlocked = entry.state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
        if (achievement.unlocked)
          achievement.unlocked_at =
              entry.unlock_time ?
                  tr("Unlocked: %1")
                      .arg(QDateTime::fromSecsSinceEpoch(entry.unlock_time).toString())
                      .toStdString() :
                  tr("Unlocked").toStdString();
        achievement.type =
            entry.type == RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION ? tr("Progression").toStdString() :
            entry.type == RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE    ? tr("Missable").toStdString() :
            entry.type == RC_CLIENT_ACHIEVEMENT_TYPE_WIN ? tr("Win Condition").toStdString() :
                                                           "";
        achievement.measured.assign(entry.measured_progress,
                                    std::find(std::begin(entry.measured_progress),
                                              std::end(entry.measured_progress), '\0'));
        // The formatted measurement is authoritative: rcheevos can retain an old
        // percentage when the current measured value is unknown.
        if (!achievement.unlocked && !achievement.measured.empty() &&
            std::isfinite(entry.measured_percent) && entry.measured_percent >= 0)
          achievement.progress = std::clamp(entry.measured_percent / 100, 0.0f, 1.0f);
        else
          achievement.measured.clear();
        achievement.badge = image("achievement:" + std::to_string(entry.id) +
                                      (achievement.unlocked ? ":unlocked" : ":locked"),
                                  manager.GetAchievementBadge(entry.id, !achievement.unlocked));
        group.achievements.push_back(std::move(achievement));
      }
      if (!group.achievements.empty())
        subset.groups.push_back(std::move(group));
    }
    const auto order = [](const BigPicture::AchievementGroup& group) {
      switch (group.id)
      {
      case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
        return 0;
      case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
        return 1;
      case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED:
        return 2;
      case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
        return 3;
      case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED:
        return 4;
      default:
        return 5;
      }
    };
    std::ranges::stable_sort(subset.groups, {}, order);
    result->subsets.push_back(std::move(subset));
  }
  m_images = std::move(used_images);
#endif
  return result;
}
