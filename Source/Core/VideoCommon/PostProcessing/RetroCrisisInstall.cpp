// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/RetroCrisisInstall.h"

#include <deque>
#include <sstream>
#include <string_view>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"

namespace VideoCommon
{
namespace
{
constexpr char kRoot[] = "retro crisis/";
constexpr char kManifest[] = "/.dolphin-retrocrisis-profile";

// Reads #reference targets from preset text (thin re-scan; the full parser is not needed here).
std::vector<std::string> ReferenceTargets(const std::string& text)
{
  std::vector<std::string> refs;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line))
  {
    const std::string_view trimmed = StripSpaces(line);
    constexpr std::string_view kRef = "#reference";
    if (trimmed.size() >= kRef.size() && trimmed.compare(0, kRef.size(), kRef) == 0)
    {
      const std::string_view target_view = StripSpaces(trimmed.substr(kRef.size()));
      std::string target(target_view);
      if (target.size() >= 2 && target.front() == '"' && target.back() == '"')
        target = target.substr(1, target.size() - 2);
      refs.push_back(target);
    }
  }
  return refs;
}
}  // namespace

std::string RetroCrisisProfileOf(const std::string& preset_rel_path)
{
  const std::size_t root_len = std::string(kRoot).size();
  if (preset_rel_path.size() <= root_len || preset_rel_path.compare(0, root_len, kRoot) != 0)
    return {};
  const std::size_t next = preset_rel_path.find('/', root_len);
  if (next == std::string::npos)
    return {};
  return preset_rel_path.substr(root_len, next - root_len);
}

std::set<std::string> ComputeRetroCrisisClosure(
    const std::string& chosen_profile,
    const std::map<std::string, std::set<std::string>>& profile_references)
{
  std::set<std::string> closure;
  std::deque<std::string> work = {chosen_profile};
  while (!work.empty())
  {
    const std::string profile = work.front();
    work.pop_front();
    if (!closure.insert(profile).second)
      continue;
    const auto it = profile_references.find(profile);
    if (it == profile_references.end())
      continue;
    for (const std::string& ref : it->second)
      work.push_back(ref);
  }
  return closure;
}

std::map<std::string, std::set<std::string>> ScanRetroCrisisReferences(
    const std::vector<std::string>& preset_rel_paths, const std::string& extract_root,
    const SlangPresetReader& reader)
{
  std::map<std::string, std::set<std::string>> profile_refs;
  for (const std::string& rel : preset_rel_paths)
  {
    const std::string profile = RetroCrisisProfileOf(rel);
    if (profile.empty())
      continue;
    profile_refs.try_emplace(profile);  // ensure the profile is present even with no refs

    std::string text;
    if (!reader(extract_root + "/" + rel, &text))
      continue;
    const std::string base_dir = extract_root + "/" + rel.substr(0, rel.find_last_of('/'));
    for (const std::string& target : ReferenceTargets(text))
    {
      // Resolve the reference to its profile folder (targets look like "../<Profile>/x.slangp").
      const std::string resolved = NormalizePath(base_dir + "/" + target);
      // Reduce resolved path back to a "retro crisis/<Profile>/..." rel path for RetroCrisisProfileOf.
      const std::size_t root_pos = resolved.rfind("retro crisis/");
      if (root_pos == std::string::npos)
        continue;
      const std::string ref_profile = RetroCrisisProfileOf(resolved.substr(root_pos));
      if (!ref_profile.empty() && ref_profile != profile)
        profile_refs[profile].insert(ref_profile);
    }
  }
  return profile_refs;
}

u32 InstallRetroCrisisProfile(const std::string& extract_root, const std::string& install_root,
                              const std::string& chosen_profile)
{
  const SlangPresetReader reader = [](const std::string& p, std::string* out) {
    return File::ReadFileToString(p, *out);
  };

  // Discover all .slangp under the extracted tree, as rel paths starting at "retro crisis/".
  const std::vector<std::string> all =
      Common::DoFileSearch({extract_root}, {".slangp"}, /*recursive=*/true);
  std::vector<std::string> rel_paths;
  for (const std::string& abs : all)
  {
    const std::size_t root_pos = abs.rfind("retro crisis/");
    if (root_pos != std::string::npos)
    {
      const std::string rel = abs.substr(root_pos);
      // Skip AppleDouble artifacts (__MACOSX/ or ._ prefixed basenames)
      if (rel.find("__MACOSX/") != std::string::npos)
        continue;
      const std::size_t last_slash = rel.find_last_of('/');
      if (last_slash != std::string::npos && last_slash + 1 < rel.size())
      {
        const std::string basename = rel.substr(last_slash + 1);
        if (basename.size() >= 2 && basename.substr(0, 2) == "._")
          continue;
      }
      rel_paths.push_back(rel);
    }
  }

  const auto profile_refs = ScanRetroCrisisReferences(rel_paths, extract_root, reader);
  const std::set<std::string> closure = ComputeRetroCrisisClosure(chosen_profile, profile_refs);

  u32 chosen_count = 0;
  for (const std::string& rel : rel_paths)
  {
    const std::string profile = RetroCrisisProfileOf(rel);
    if (closure.count(profile) == 0)
      continue;
    const std::string src = extract_root + "/" + rel;
    const std::string dst = install_root + "/" + rel;
    File::CreateFullPath(dst);
    if (!File::Copy(src, dst))
    {
      WARN_LOG_FMT(VIDEO, "RetroCrisis: failed to copy {} -> {}", src, dst);
      continue;  // skip counting this file
    }
    if (profile == chosen_profile)
      ++chosen_count;
  }

  // Manifest naming the chosen profile (used by discovery to hide closure-only base folders).
  const std::string manifest = install_root + kManifest;
  File::CreateFullPath(manifest);
  File::IOFile mf(manifest, "wb");
  if (!mf || !mf.WriteBytes(chosen_profile.data(), chosen_profile.size()))
    return 0;  // manifest write failed
  return chosen_count;
}

std::string ReadRetroCrisisProfile(const std::string& install_root)
{
  std::string text;
  if (!File::ReadFileToString(install_root + kManifest, text))
    return {};
  return std::string(StripSpaces(text));
}

const std::vector<std::string>& GetRetroCrisisProfiles()
{
  static const std::vector<std::string> profiles = {
      "1080p Flat",   "1440p Flat",   "4K Flat",         "1080p Curved",
      "1440p Curved", "4K Curved",    "720p Steam Deck",
  };
  return profiles;
}

bool IsHiddenRetroCrisisPreset(const std::string& preset_abs_path, const std::string& install_root,
                               const std::string& chosen_profile)
{
  if (preset_abs_path.compare(0, install_root.size(), install_root) != 0)
    return false;  // not under the RetroCrisis tree
  const std::size_t root_pos = preset_abs_path.rfind("retro crisis/");
  if (root_pos == std::string::npos)
    return false;
  const std::string profile = RetroCrisisProfileOf(preset_abs_path.substr(root_pos));
  return !profile.empty() && profile != chosen_profile;
}
}  // namespace VideoCommon
