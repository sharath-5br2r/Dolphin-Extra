// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/BigPicture/StatePreview.h"

#include <array>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/ScopeGuard.h"

namespace BigPicture
{
namespace
{
constexpr std::array<char, 8> MAGIC{'D', 'O', 'L', 'P', 'T', 'H', '0', '1'};
constexpr std::size_t HEADER_SIZE = 24;
bool ValidSize(std::uint32_t width, std::uint32_t height)
{
  return width && height && width <= 320 && height <= 240;
}
}  // namespace

std::shared_ptr<const LibraryImage> ReadStatePreview(const std::string& path,
                                                     std::uint64_t timestamp)
{
  File::IOFile file(path, "rb");
  if (!file)
    return {};
  std::array<char, 8> magic{};
  std::uint64_t saved_timestamp;
  auto image = std::make_shared<LibraryImage>();
  if (!file.ReadArray(&magic) || magic != MAGIC || !file.ReadArray(&saved_timestamp, 1) ||
      saved_timestamp != timestamp || !file.ReadArray(&image->width, 1) ||
      !file.ReadArray(&image->height, 1) || !ValidSize(image->width, image->height))
    return {};
  const auto bytes = image->width * image->height * 4;
  if (file.GetSize() != HEADER_SIZE + bytes)
    return {};
  image->rgba.resize(bytes);
  if (!file.ReadBytes(image->rgba.data(), bytes))
    return {};
  image->revision = timestamp;
  return image;
}

bool WriteStatePreview(const std::string& path, std::uint64_t timestamp, const LibraryImage& image)
{
  if (!ValidSize(image.width, image.height) || image.rgba.size() != image.width * image.height * 4)
    return false;
  const auto temporary = File::GetTempFilenameForAtomicWrite(path);
  const Common::ScopeGuard cleanup(
      [&] { File::Delete(temporary, File::IfAbsentBehavior::NoConsoleWarning); });
  File::IOFile file(temporary, "wb");
  if (!file || !file.WriteArray(MAGIC) || !file.WriteArray(&timestamp, 1) ||
      !file.WriteArray(&image.width, 1) || !file.WriteArray(&image.height, 1) ||
      !file.WriteBytes(image.rgba.data(), image.rgba.size()) || !file.Close())
    return false;
  return File::Rename(temporary, path);
}
}  // namespace BigPicture
