// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/LutTexture.h"

#include <string>
#include <vector>

#include "Common/Buffer.h"
#include "Common/FileUtil.h"
#include "Common/Image.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/PostProcessing/MipGen.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon
{
std::unique_ptr<AbstractTexture> LoadLutTexture(const SlangLutConfig& lut)
{
  std::string contents;
  if (!File::ReadFileToString(lut.path, contents))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to read LUT '{}' from '{}'", lut.name, lut.path);
    return nullptr;
  }

  Common::UniqueBuffer<u8> decoded;
  u32 width = 0;
  u32 height = 0;
  const std::span<const u8> input(reinterpret_cast<const u8*>(contents.data()), contents.size());
  if (!Common::LoadPNG(input, &decoded, &width, &height) || width == 0 || height == 0)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to decode LUT PNG '{}' ('{}')", lut.name, lut.path);
    return nullptr;
  }

  std::vector<MipLevel> mips;
  if (lut.mipmap)
  {
    mips = GenerateBoxMips(width, height, decoded.data());
  }
  else
  {
    MipLevel level0;
    level0.width = width;
    level0.height = height;
    level0.rgba8.assign(decoded.data(), decoded.data() + static_cast<size_t>(width) * height * 4);
    mips.push_back(std::move(level0));
  }

  // A LUT is one layer, but it must still be an array texture: translated slang passes declare
  // every sampler as sampler2DArray (SLANG_INPUT_TEXTURE_TYPE), and a Texture_2D bound to one of
  // those samplers is the same silent black render as the mismatch in the other direction.
  const TextureConfig config(width, height, static_cast<u32>(mips.size()), 1, 1,
                             AbstractTextureFormat::RGBA8, 0, SLANG_INPUT_TEXTURE_TYPE);
  auto texture = g_gfx->CreateTexture(config, "slang LUT: " + lut.name);
  if (!texture)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to allocate texture for LUT '{}'", lut.name);
    return nullptr;
  }

  for (u32 level = 0; level < mips.size(); ++level)
  {
    const MipLevel& mip = mips[level];
    texture->Load(level, mip.width, mip.height, mip.width, mip.rgba8.data(), mip.rgba8.size());
  }

  return texture;
}
}  // namespace VideoCommon
