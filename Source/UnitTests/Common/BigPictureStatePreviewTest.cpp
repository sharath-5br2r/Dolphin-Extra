// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/BigPicture/StatePreview.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"

namespace
{
class BigPictureStatePreviewTest : public testing::Test
{
protected:
  void SetUp() override
  {
    directory = File::CreateTempDir();
    ASSERT_FALSE(directory.empty());
    path = directory + "/state.preview";
  }
  void TearDown() override
  {
    if (!directory.empty())
      File::DeleteDirRecursively(directory);
  }
  BigPicture::LibraryImage image{0, 4, 3, std::vector<u8>(4 * 3 * 4, 123)};
  std::string directory;
  std::string path;
};

TEST_F(BigPictureStatePreviewTest, RoundTripAndReplacementPreserveDimensionsAndPixels)
{
  ASSERT_TRUE(BigPicture::WriteStatePreview(path, 1234, image));
  const auto loaded = BigPicture::ReadStatePreview(path, 1234);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->revision, 1234u);
  EXPECT_EQ(loaded->width, image.width);
  EXPECT_EQ(loaded->height, image.height);
  EXPECT_EQ(loaded->rgba, image.rgba);
  image.rgba[0] = 42;
  ASSERT_TRUE(BigPicture::WriteStatePreview(path, 5678, image));
  EXPECT_FALSE(BigPicture::ReadStatePreview(path, 1234));
  ASSERT_TRUE(BigPicture::ReadStatePreview(path, 5678));
  EXPECT_EQ(BigPicture::ReadStatePreview(path, 5678)->rgba[0], 42);
}

TEST_F(BigPictureStatePreviewTest, MissingStaleAndTruncatedSidecarsHaveNoPreview)
{
  EXPECT_FALSE(BigPicture::ReadStatePreview(path, 1));
  ASSERT_TRUE(BigPicture::WriteStatePreview(path, 1, image));
  EXPECT_FALSE(BigPicture::ReadStatePreview(path, 2));
  File::IOFile file(path, "wb");
  ASSERT_TRUE(file.WriteString("DOLPTH01"));
  ASSERT_TRUE(file.Close());
  EXPECT_FALSE(BigPicture::ReadStatePreview(path, 1));
}

TEST_F(BigPictureStatePreviewTest, CorruptDimensionsAreRejectedBeforeAllocatingPixels)
{
  File::IOFile file(path, "wb");
  const u64 timestamp = 1;
  const u32 width = 0xffffffff;
  const u32 height = 240;
  ASSERT_TRUE(file.WriteString("DOLPTH01"));
  ASSERT_TRUE(file.WriteArray(&timestamp, 1));
  ASSERT_TRUE(file.WriteArray(&width, 1));
  ASSERT_TRUE(file.WriteArray(&height, 1));
  ASSERT_TRUE(file.Close());
  EXPECT_FALSE(BigPicture::ReadStatePreview(path, 1));
}

TEST_F(BigPictureStatePreviewTest, InvalidWritesLeaveTheExistingPreviewIntact)
{
  ASSERT_TRUE(BigPicture::WriteStatePreview(path, 1, image));
  image.rgba.pop_back();
  EXPECT_FALSE(BigPicture::WriteStatePreview(path, 2, image));
  image.width = 321;
  EXPECT_FALSE(BigPicture::WriteStatePreview(path, 2, image));
  EXPECT_TRUE(BigPicture::ReadStatePreview(path, 1));
}
}  // namespace
