// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/ShaderPackSource.h"

#include "Common/FileUtil.h"

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
