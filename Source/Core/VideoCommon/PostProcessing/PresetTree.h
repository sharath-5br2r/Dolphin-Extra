// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

namespace VideoCommon
{
// One node per path segment of a preset list entry. Folders are grouping nodes only -- the picker
// dialog makes them unselectable -- while leaves carry the full relative preset path, which is the
// value stored in GFX_ENHANCE_POST_SHADER.
struct PresetTreeNode
{
  std::string label;  // this segment, for display
  std::string path;   // folder path for folders, full relative preset path for leaves
  bool is_preset = false;
  std::vector<PresetTreeNode> children;
};

// Groups a flat list of '/'-separated relative preset paths -- what
// MultipassPostProcessing::GetPresetList() returns -- into a tree, one level per path segment.
// Folders are distinguished by their whole path, not their name, so same-named folders in
// different shader packs stay separate.
//
// The result is ordered by a byte-wise sort of the full relative paths (so uppercase sorts before
// lowercase, and folders and leaves interleave by name), which is the order PCSX2's
// ShaderPresets::Enumerate hands to its own tree builder. Sorting happens here because
// GetPresetList returns the directory walk's order, which is not guaranteed. Duplicate paths --
// possible because GetPresetList searches both the user and the sys Shaders dir -- collapse into
// one leaf.
std::vector<PresetTreeNode> BuildPresetTree(const std::vector<std::string>& presets);
}  // namespace VideoCommon
