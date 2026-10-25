// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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
};

const std::vector<ShaderPackSource>& GetShaderPackSources();
const ShaderPackSource* FindShaderPackSource(std::string_view id);
std::vector<std::string> MissingDependencies(const ShaderPackSource& source,
                                              const std::string& shaders_root);
}  // namespace VideoCommon
