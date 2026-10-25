// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/PresetArchive.h"

#include <algorithm>
#include <string_view>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"

namespace VideoCommon
{
namespace
{
// Normalizes backslashes to '/' for uniform inspection.
std::string NormalizeSeparators(std::string name)
{
  std::replace(name.begin(), name.end(), '\\', '/');
  return name;
}

// True if the (separator-normalized) entry name would escape dest_root: absolute path or any
// ".." path segment.
bool IsUnsafeEntryName(const std::string& name)
{
  if (name.empty() || name.front() == '/')
    return true;
  size_t start = 0;
  while (start <= name.size())
  {
    const auto slash = name.find('/', start);
    const auto end = slash == std::string::npos ? name.size() : slash;
    if (name.compare(start, end - start, "..") == 0)
      return true;
    if (slash == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

bool IsDirectoryEntry(const std::string& name, u64 uncompressed_size)
{
  return !name.empty() && name.back() == '/' && uncompressed_size == 0;
}

std::string BaseNameWithoutSlangp(const std::string& entry)
{
  const auto slash = entry.find_last_of('/');
  const std::string base = slash == std::string::npos ? entry : entry.substr(slash + 1);
  constexpr std::string_view kExt = ".slangp";
  if (base.size() >= kExt.size() && base.compare(base.size() - kExt.size(), kExt.size(), kExt) == 0)
    return base.substr(0, base.size() - kExt.size());
  return base;
}

size_t PathDepth(const std::string& name)
{
  return static_cast<size_t>(std::count(name.begin(), name.end(), '/'));
}
}  // namespace

std::vector<std::string> ExtractSanitizedArchive(const std::string& zip_path,
                                                 const std::string& dest_root, std::string* error,
                                                 const std::string& strip_prefix)
{
  const auto fail = [error](std::string message) -> std::vector<std::string> {
    if (error != nullptr)
      *error = std::move(message);
    return {};
  };

  void* reader = mz_zip_reader_create();
  if (reader == nullptr)
    return fail("failed to create zip reader");
  Common::ScopeGuard reader_guard{[&] { mz_zip_reader_delete(&reader); }};

  if (mz_zip_reader_open_file(reader, zip_path.c_str()) != MZ_OK)
    return fail("could not open archive");

  std::vector<std::string> slangp_entries;

  int32_t status = mz_zip_reader_goto_first_entry(reader);
  while (status == MZ_OK)
  {
    mz_zip_file* info = nullptr;
    if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || info == nullptr)
      return fail("failed to read archive entry");

    const std::string name = NormalizeSeparators(info->filename == nullptr ? "" : info->filename);

    if (IsUnsafeEntryName(name))
      return fail("archive contains an unsafe path: " + name);

    std::string rel = name;
    if (!strip_prefix.empty())
    {
      if (rel.size() < strip_prefix.size() || rel.compare(0, strip_prefix.size(), strip_prefix) != 0)
      {
        status = mz_zip_reader_goto_next_entry(reader);
        continue;
      }
      rel = rel.substr(strip_prefix.size());
      if (rel.empty())  // the prefix directory entry itself
      {
        status = mz_zip_reader_goto_next_entry(reader);
        continue;
      }
    }

    if (!IsDirectoryEntry(name, info->uncompressed_size))
    {
      // Track presets.
      constexpr std::string_view kExt = ".slangp";
      if (rel.size() >= kExt.size() &&
          rel.compare(rel.size() - kExt.size(), kExt.size(), kExt) == 0)
      {
        slangp_entries.push_back(rel);
      }

      // Extract.
      const std::string out_path = dest_root + "/" + rel;
      if (!File::CreateFullPath(out_path))
        return fail("failed to create path for " + rel);

      std::vector<u8> buffer(info->uncompressed_size);
      if (info->uncompressed_size != 0 && !Common::ReadFileFromZip(reader, &buffer))
        return fail("failed to read " + rel);

      File::IOFile out(out_path, "wb");
      if (!out || (!buffer.empty() && !out.WriteBytes(buffer.data(), buffer.size())))
        return fail("failed to write " + rel);
    }

    status = mz_zip_reader_goto_next_entry(reader);
  }

  if (status != MZ_END_OF_LIST && status != MZ_OK)
    return fail("failed while iterating archive");

  return slangp_entries;
}

PresetImportResult ImportPresetArchive(const std::string& zip_path, const std::string& dest_root)
{
  std::string error;
  const std::vector<std::string> presets = ExtractSanitizedArchive(zip_path, dest_root, &error);
  if (presets.empty() && !error.empty())
    return {false, "", error};

  if (presets.empty())
    return {false, "", "archive contains no .slangp preset"};

  // Pick the shallowest preset; if a tie remains, it is ambiguous.
  std::string chosen = presets.front();
  size_t chosen_depth = PathDepth(chosen);
  size_t shallowest_count = 1;
  for (size_t i = 1; i < presets.size(); ++i)
  {
    const size_t depth = PathDepth(presets[i]);
    if (depth < chosen_depth)
    {
      chosen = presets[i];
      chosen_depth = depth;
      shallowest_count = 1;
    }
    else if (depth == chosen_depth)
    {
      ++shallowest_count;
    }
  }

  if (shallowest_count > 1)
    return {false, "", "archive contains multiple .slangp presets"};

  return {true, BaseNameWithoutSlangp(chosen), ""};
}
}  // namespace VideoCommon
