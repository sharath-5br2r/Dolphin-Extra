// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/PresetTree.h"

using namespace VideoCommon;

namespace
{
// Renders the tree as one indented line per node so a whole expected shape fits in a literal.
// Folders print as "name/", leaves as "name -> path".
void Dump(const std::vector<PresetTreeNode>& nodes, int depth, std::string* out)
{
  for (const PresetTreeNode& node : nodes)
  {
    out->append(static_cast<size_t>(depth) * 2, ' ');
    if (node.is_preset)
      out->append(node.label).append(" -> ").append(node.path).append("\n");
    else
      out->append(node.label).append("/ [").append(node.path).append("]\n");
    Dump(node.children, depth + 1, out);
  }
}

std::string Dump(const std::vector<PresetTreeNode>& nodes)
{
  std::string out;
  Dump(nodes, 0, &out);
  return out;
}
}  // namespace

TEST(PresetTree, EmptyListProducesEmptyTree)
{
  EXPECT_TRUE(BuildPresetTree({}).empty());
}

TEST(PresetTree, TwoPresetsInOneFolderShareOneFolderNode)
{
  const std::vector<PresetTreeNode> tree = BuildPresetTree({"crt/a", "crt/b"});

  ASSERT_EQ(tree.size(), 1u);
  EXPECT_EQ(tree[0].label, "crt");
  EXPECT_EQ(tree[0].path, "crt");
  EXPECT_FALSE(tree[0].is_preset);
  ASSERT_EQ(tree[0].children.size(), 2u);
  EXPECT_EQ(tree[0].children[0].label, "a");
  EXPECT_EQ(tree[0].children[0].path, "crt/a");
  EXPECT_TRUE(tree[0].children[0].is_preset);
  EXPECT_EQ(tree[0].children[1].path, "crt/b");
}

TEST(PresetTree, NestedFoldersNestOneNodePerLevel)
{
  // Every level is preserved: a two-folder path is two folder nodes deep, not flattened to one.
  EXPECT_EQ(Dump(BuildPresetTree({"crt/nested/x"})), "crt/ [crt]\n"
                                                     "  nested/ [crt/nested]\n"
                                                     "    x -> crt/nested/x\n");
}

TEST(PresetTree, RootPresetIsATopLevelLeaf)
{
  const std::vector<PresetTreeNode> tree = BuildPresetTree({"bloom"});

  ASSERT_EQ(tree.size(), 1u);
  EXPECT_EQ(tree[0].label, "bloom");
  EXPECT_EQ(tree[0].path, "bloom");
  EXPECT_TRUE(tree[0].is_preset);
  EXPECT_TRUE(tree[0].children.empty());
}

TEST(PresetTree, SameNamedFoldersUnderDifferentParentsStayDistinct)
{
  // Folders are interned by folder *path*, not by name: "a/shared" and "b/shared" are two nodes.
  EXPECT_EQ(Dump(BuildPresetTree({"a/shared/x", "b/shared/y"})), "a/ [a]\n"
                                                                 "  shared/ [a/shared]\n"
                                                                 "    x -> a/shared/x\n"
                                                                 "b/ [b]\n"
                                                                 "  shared/ [b/shared]\n"
                                                                 "    y -> b/shared/y\n");
}

TEST(PresetTree, AFolderAndALeafMayShareAName)
{
  // "misc" is both a preset at the root and a folder holding another preset. Keying folders on
  // the path alone would make the leaf's path collide with the folder's.
  EXPECT_EQ(Dump(BuildPresetTree({"misc", "misc/x"})), "misc -> misc\n"
                                                       "misc/ [misc]\n"
                                                       "  x -> misc/x\n");
}

TEST(PresetTree, OrderingIsDeterministicRegardlessOfInputOrder)
{
  // GetPresetList returns whatever order the directory walk produced, so the tree sorts for
  // itself. Two shuffles of the same corpus must produce the same tree.
  const std::vector<std::string> presets = {"crt/b", "a", "crt/nested/y", "crt/a", "crt/nested/x",
                                            "zz/q",  "B"};
  std::vector<std::string> shuffled = presets;
  std::ranges::reverse(shuffled);

  const std::string expected = Dump(BuildPresetTree(presets));
  EXPECT_EQ(Dump(BuildPresetTree(shuffled)), expected);

  // The order is a byte-wise sort of the full relative paths, as PCSX2's ShaderPresets::Enumerate
  // does before feeding its own tree builder: uppercase before lowercase, and folders and leaves
  // interleaved by name rather than folders first.
  EXPECT_EQ(expected, "B -> B\n"
                      "a -> a\n"
                      "crt/ [crt]\n"
                      "  a -> crt/a\n"
                      "  b -> crt/b\n"
                      "  nested/ [crt/nested]\n"
                      "    x -> crt/nested/x\n"
                      "    y -> crt/nested/y\n"
                      "zz/ [zz]\n"
                      "  q -> zz/q\n");
}

TEST(PresetTree, DuplicateEntriesCollapse)
{
  // GetPresetList searches the user and sys Shaders dirs, so the same relative path can appear
  // twice. One leaf, since both spellings resolve to the same stored setting value.
  EXPECT_EQ(Dump(BuildPresetTree({"crt/a", "crt/a"})), "crt/ [crt]\n"
                                                       "  a -> crt/a\n");
}

TEST(PresetTree, EmptySegmentsAndEmptyPathsAreSkipped)
{
  // Defensive: a stray leading/doubled separator must not produce a blank folder node, and an
  // entry with no segments at all contributes nothing.
  EXPECT_EQ(Dump(BuildPresetTree({"/crt//a", "", "/"})), "crt/ [crt]\n"
                                                         "  a -> /crt//a\n");
}
