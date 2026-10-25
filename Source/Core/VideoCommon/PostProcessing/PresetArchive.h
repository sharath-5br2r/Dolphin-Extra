// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

namespace VideoCommon
{
struct PresetImportResult
{
  bool ok = false;
  std::string preset_name;  // basename without ".slangp", ready for GFX_ENHANCE_POST_SHADER
  std::string error;        // set when ok == false
};

// Extracts a .slangp bundle zip at zip_path into dest_root (typically the user Shaders dir),
// preserving the archive's internal directory structure. Requires exactly one .slangp entry
// in the archive; returns its extraction-relative name in preset_name.
PresetImportResult ImportPresetArchive(const std::string& zip_path, const std::string& dest_root);

// Shared, sanitized zip extractor used by ImportPresetArchive and by the buildbot downloader
// (ShaderPackDownload). Extracts every non-directory entry of the archive at zip_path into
// dest_root, preserving structure, after rejecting any absolute or "../"-containing entry
// (Zip-Slip). Returns the extraction-relative names of all ".slangp" entries found (empty on
// failure; *error set). Does NOT enforce a single-preset rule -- callers decide.
std::vector<std::string> ExtractSanitizedArchive(const std::string& zip_path,
                                                 const std::string& dest_root, std::string* error,
                                                 const std::string& strip_prefix = "");
}  // namespace VideoCommon
