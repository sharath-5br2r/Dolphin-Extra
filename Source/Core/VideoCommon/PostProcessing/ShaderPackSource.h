// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VideoCommon
{
struct ShaderPackSource
{
  std::string id;
  std::string display_name;
  std::string url;
  std::string extract_subpath;
  std::string install_subdir;
  std::vector<std::string> depends_on;
  std::string install_marker;  // path under shaders_root that exists iff installed; "" = skip
  // If set, url is a GitHub release API endpoint; substitute {tag} from tag_name in this filename.
  std::string release_asset_name_template;
};

const std::vector<ShaderPackSource>& GetShaderPackSources();
const ShaderPackSource* FindShaderPackSource(std::string_view id);
// Selects exactly one matching release asset. Malformed metadata or no unambiguous match fails.
std::optional<std::string> FindShaderPackReleaseAssetUrl(const ShaderPackSource& source,
                                                         std::string_view release_json);
std::vector<std::string> MissingDependencies(const ShaderPackSource& source,
                                             const std::string& shaders_root);
}  // namespace VideoCommon
