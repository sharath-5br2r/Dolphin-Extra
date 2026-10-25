// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/ShaderPackDownload.h"

#include <chrono>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "VideoCommon/PostProcessing/PresetArchive.h"
#include "VideoCommon/PostProcessing/RetroCrisisInstall.h"
#include "VideoCommon/PostProcessing/ShaderPackSource.h"

namespace VideoCommon
{
ShaderPackDownloadResult InstallShaderPackFromZip(const std::string& local_zip_path,
                                                  const std::string& dest_root)
{
  std::string error;
  const std::vector<std::string> presets =
      ExtractSanitizedArchive(local_zip_path, dest_root, &error);
  if (presets.empty() && !error.empty())
    return {false, 0, error};

  return {true, static_cast<u32>(presets.size()), ""};
}

ShaderPackDownloadResult DownloadAndInstallShaderPack(const std::string& url,
                                                      const std::string& dest_root,
                                                      DownloadProgress progress)
{
  INFO_LOG_FMT(VIDEO, "Downloading slang shader pack from {}", url);

  Common::HttpRequest::ProgressCallback curl_progress = nullptr;
  if (progress)
  {
    curl_progress = [progress](s64 dltotal, s64 dlnow, s64, s64) -> bool {
      return progress(dlnow, dltotal);
    };
  }

  Common::HttpRequest http(std::chrono::seconds(60), std::move(curl_progress));
  http.FollowRedirects(10);  // the buildbot may 30x.

  const Common::HttpRequest::Response response = http.Get(url);
  if (!response)
  {
    const std::string error =
        "download failed (HTTP " + std::to_string(http.GetLastResponseCode()) + ")";
    ERROR_LOG_FMT(VIDEO, "Slang shader pack {}", error);
    return {false, 0, error};
  }

  const std::string tmp_dir = File::CreateTempDir();
  if (tmp_dir.empty())
    return {false, 0, "could not create temp directory"};
  Common::ScopeGuard tmp_guard{[&] { File::DeleteDirRecursively(tmp_dir); }};

  const std::string zip_path = tmp_dir + DIR_SEP "shaders_slang.zip";
  {
    File::IOFile zip(zip_path, "wb");
    if (!zip || !zip.WriteBytes(response->data(), response->size()))
      return {false, 0, "failed to write downloaded archive"};
  }

  INFO_LOG_FMT(VIDEO, "Downloaded {} bytes; extracting shader pack into {}", response->size(),
               dest_root);
  return InstallShaderPackFromZip(zip_path, dest_root);
}

ShaderPackDownloadResult InstallShaderPackSource(const ShaderPackSource& source,
                                                 const std::string& local_zip_path,
                                                 const std::string& shaders_root)
{
  // Normalize shaders_root: strip trailing DIR_SEP to avoid doubling when appending subdir.
  std::string root = shaders_root;
  if (!root.empty() && root.back() == DIR_SEP_CHR)
    root.pop_back();

  const std::string dest =
      source.install_subdir.empty() ? root : root + DIR_SEP + source.install_subdir;

  std::string error;
  const std::vector<std::string> presets =
      ExtractSanitizedArchive(local_zip_path, dest, &error, source.extract_subpath);
  if (presets.empty() && !error.empty())
    return {false, 0, error};
  if (presets.empty())
  {
    const std::string message = source.extract_subpath.empty()
                                    ? "no shader presets found in archive"
                                    : "no shader presets found matching subpath";
    return {false, 0, message};
  }
  return {true, static_cast<u32>(presets.size()), ""};
}

ShaderPackDownloadResult DownloadShaderPackById(std::string_view id,
                                                const std::string& shaders_root,
                                                DownloadProgress progress,
                                                const std::string& profile)
{
  const ShaderPackSource* source = FindShaderPackSource(id);
  if (source == nullptr)
    return {false, 0, "unknown shader pack id"};

  for (const std::string& dep_id : MissingDependencies(*source, shaders_root))
  {
    const ShaderPackDownloadResult dep_result = DownloadShaderPackById(dep_id, shaders_root, progress);
    if (!dep_result.ok)
      return {false, 0, "failed to install dependency '" + dep_id + "': " + dep_result.error};
  }

  Common::HttpRequest::ProgressCallback curl_progress = nullptr;
  if (progress)
  {
    curl_progress = [progress](s64 dltotal, s64 dlnow, s64, s64) -> bool {
      return progress(dlnow, dltotal);
    };
  }

  Common::HttpRequest http(std::chrono::seconds(60), std::move(curl_progress));
  http.FollowRedirects(10);
  const Common::HttpRequest::Response response = http.Get(source->url);
  if (!response)
  {
    return {false, 0,
            "download failed (HTTP " + std::to_string(http.GetLastResponseCode()) + ")"};
  }

  const std::string tmp_dir = File::CreateTempDir();
  if (tmp_dir.empty())
    return {false, 0, "could not create temp directory"};
  Common::ScopeGuard tmp_guard{[&] { File::DeleteDirRecursively(tmp_dir); }};

  const std::string zip_path = tmp_dir + DIR_SEP "pack.zip";
  {
    File::IOFile zip(zip_path, "wb");
    if (!zip || !zip.WriteBytes(response->data(), response->size()))
      return {false, 0, "failed to write downloaded archive"};
  }

  // RetroCrisis uses profile-scoped installation: extract the whole zip to a temp dir, then
  // install only the chosen profile + its closure.
  if (source->id == "retrocrisis")
  {
    // Normalize shaders_root the same way InstallShaderPackSource does.
    std::string root = shaders_root;
    if (!root.empty() && root.back() == DIR_SEP_CHR)
      root.pop_back();

    const std::string install_root =
        source->install_subdir.empty() ? root : root + DIR_SEP + source->install_subdir;

    const std::string temp_extract = File::CreateTempDir();
    if (temp_extract.empty())
      return {false, 0, "could not create temp extraction directory"};
    Common::ScopeGuard extract_guard{[&] { File::DeleteDirRecursively(temp_extract); }};

    std::string extract_error;
    const std::vector<std::string> extracted =
        ExtractSanitizedArchive(zip_path, temp_extract, &extract_error);
    if (extracted.empty() && !extract_error.empty())
      return {false, 0, extract_error};

    const u32 preset_count =
        InstallRetroCrisisProfile(temp_extract, install_root,
                                  profile.empty() ? "1080p Flat" : profile);
    if (preset_count == 0)
      return {false, 0, "no presets installed for profile"};
    return {true, preset_count, ""};
  }

  return InstallShaderPackSource(*source, zip_path, shaders_root);
}
}  // namespace VideoCommon
