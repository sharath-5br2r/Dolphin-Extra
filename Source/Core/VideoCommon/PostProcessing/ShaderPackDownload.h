// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

namespace VideoCommon
{
struct ShaderPackSource;

// The canonical libretro slang shader pack on the buildbot (verified 2026-07-12).
constexpr char SLANG_SHADER_PACK_URL[] =
    "https://buildbot.libretro.com/assets/frontend/shaders_slang.zip";

struct ShaderPackDownloadResult
{
  bool ok = false;
  u32 preset_count = 0;  // number of .slangp files extracted from the pack
  std::string error;     // set when ok == false
};

// Progress callback: (bytes_downloaded, bytes_total). bytes_total may be 0 if the server
// does not report Content-Length. Return false to cancel the download. May be null.
using DownloadProgress = std::function<bool(s64 downloaded, s64 total)>;

// Downloads the slang shader pack from `url` and extracts it (sanitized) into `dest_root`
// (typically File::GetUserPath(D_SHADERS_IDX)). Blocking; run off the UI thread. The zip is
// written to a temp file, extracted via ExtractSanitizedArchive, then the temp file removed.
ShaderPackDownloadResult DownloadAndInstallShaderPack(const std::string& url,
                                                      const std::string& dest_root,
                                                      DownloadProgress progress = nullptr);

// Extracts an already-downloaded pack zip at local_zip_path into dest_root. This is the
// network-free half of DownloadAndInstallShaderPack (which fetches then calls this). Counts
// the .slangp files extracted.
ShaderPackDownloadResult InstallShaderPackFromZip(const std::string& local_zip_path,
                                                  const std::string& dest_root);

// Installs an already-downloaded zip for a known source id, applying its extract_subpath and
// install_subdir under shaders_root. Network-free half; used by tests and the downloader.
ShaderPackDownloadResult InstallShaderPackSource(const ShaderPackSource& source,
                                                 const std::string& local_zip_path,
                                                 const std::string& shaders_root);

// Downloads + installs the source with the given id into shaders_root. Returns ok=false with
// error set if the id is unknown or the download/extract fails. For retrocrisis, profile
// selects which profile folder (e.g. "1080p Flat") to install; defaults to "1080p Flat" if empty.
ShaderPackDownloadResult DownloadShaderPackById(std::string_view id,
                                                const std::string& shaders_root,
                                                DownloadProgress progress = nullptr,
                                                const std::string& profile = "");
}  // namespace VideoCommon
