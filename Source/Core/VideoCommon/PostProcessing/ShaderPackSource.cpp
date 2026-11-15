// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/ShaderPackSource.h"

#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

namespace VideoCommon
{
const std::vector<ShaderPackSource>& GetShaderPackSources()
{
  static const std::vector<ShaderPackSource> sources = {
      {"libretro", "libretro slang shaders",
       "https://buildbot.libretro.com/assets/frontend/shaders_slang.zip",
       /*extract_subpath=*/"", /*install_subdir=*/"shaders_slang", /*depends_on=*/{},
       /*install_marker=*/"shaders_slang/crt/shaders/guest/advanced/stock.slang"},
      {"satpixie", "CRT-SatPixie",
       "https://github.com/Conkwer/satpixie-crt-shader/releases/download/20260122/"
       "satpixie-crt-shader-20260122.zip",
       "satpixie-crt-shader/RetroArch/shaders/shaders_slang/", "shaders_slang", {},
       /*install_marker=*/"shaders_slang/crt/satpixie-crt.slangp"},
      {"retrocrisis", "RetroCrisis GDV-NTSC",
       "https://github.com/RetroCrisis/Retro-Crisis-GDV-NTSC/releases/download/20260820/"
       "Retro.Crisis.GDV-NTSC.2026.08.20.zip.zip",
       /*extract_subpath=*/"", /*install_subdir=*/"RetroCrisis", /*depends_on=*/{"libretro"},
       /*install_marker=*/"RetroCrisis/retro crisis"},
      {"lola-retro", "Lola Retro",
       "https://api.github.com/repos/zoran123456/lola-retro/releases/latest",
       /*extract_subpath=*/"", /*install_subdir=*/"shaders_slang", /*depends_on=*/{},
       /*install_marker=*/"shaders_slang/lola-retro/systems/curved/gamecube.slangp",
       /*release_asset_name_template=*/"lola-retro-{tag}.zip"},
  };
  return sources;
}

const ShaderPackSource* FindShaderPackSource(std::string_view id)
{
  for (const ShaderPackSource& source : GetShaderPackSources())
  {
    if (source.id == id)
      return &source;
  }
  return nullptr;
}

std::optional<std::string> FindShaderPackReleaseAssetUrl(const ShaderPackSource& source,
                                                         std::string_view release_json)
{
  if (source.release_asset_name_template.empty())
    return std::nullopt;

  picojson::value release;
  auto begin = release_json.begin();
  if (!picojson::parse(release, begin, release_json.end()).empty() ||
      !release.is<picojson::object>())
  {
    return std::nullopt;
  }
  const auto& tag = release.get("tag_name");
  const auto& assets = release.get("assets");
  if (!tag.is<std::string>() || tag.get<std::string>().empty() || !assets.is<picojson::array>())
    return std::nullopt;

  const std::string expected_name =
      ReplaceAll(source.release_asset_name_template, "{tag}", tag.get<std::string>());
  std::optional<std::string> url;
  for (const auto& asset : assets.get<picojson::array>())
  {
    if (!asset.is<picojson::object>())
      continue;
    const auto& name = asset.get("name");
    if (!name.is<std::string>() || name.get<std::string>() != expected_name)
      continue;
    const auto& download_url = asset.get("browser_download_url");
    if (url || !download_url.is<std::string>() ||
        !download_url.get<std::string>().starts_with("https://github.com/"))
    {
      return std::nullopt;
    }
    url = download_url.get<std::string>();
  }
  return url;
}

std::vector<std::string> MissingDependencies(const ShaderPackSource& source,
                                             const std::string& shaders_root)
{
  std::vector<std::string> missing;
  for (const std::string& dep_id : source.depends_on)
  {
    const ShaderPackSource* dep = FindShaderPackSource(dep_id);
    if (dep == nullptr)
      continue;
    if (dep->install_marker.empty() || !File::Exists(shaders_root + "/" + dep->install_marker))
      missing.push_back(dep_id);
  }
  return missing;
}
}  // namespace VideoCommon
