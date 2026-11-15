// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMargins>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QWidget>

#include <gtest/gtest.h>

#include "Core/Config/UISettings.h"

#include "DolphinQt/GameList/GridProxyModel.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameListDiscGroups.h"

#include "ui_GameListWidget.h"

TEST(GameListUiTest, NewProfilesDefaultToGridWithCoverDownloads)
{
  EXPECT_FALSE(Settings::DEFAULT_PREFERRED_VIEW_IS_LIST);
  EXPECT_TRUE(Config::MAIN_USE_GAME_COVERS.GetDefaultValue());
}

namespace
{
class GameListDiscGroupsTest : public testing::Test
{
protected:
  std::shared_ptr<const UICommon::GameFile> Disc(const QString& filename, u8 number,
                                                 u8 revision = 0, const char* id = "GAFE01")
  {
    // Only disc metadata is needed. No copyrighted game data or bootable image is used.
    QByteArray header(0x3000, '\0');
    std::copy_n(id, 6, header.data());
    header[6] = number;
    header[7] = revision;
    header.replace(0x1c, 4, QByteArray::fromHex(QByteArrayLiteral("c2339f3d")));
    header[0x422] = 0x10;  // A nonzero DOL offset distinguishes normal discs from Datel discs.
    const QString path = directory.filePath(filename);
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly)) << file.errorString().toStdString();
    EXPECT_EQ(file.write(header), header.size());
    file.close();
    auto game = std::make_shared<UICommon::GameFile>(path.toStdString());
    EXPECT_TRUE(game->IsValid());
    EXPECT_EQ(game->GetDiscNumber(), number);
    return game;
  }

  QTemporaryDir directory;
  UICommon::GameListDiscGroups groups;
};
}  // namespace

TEST_F(GameListDiscGroupsTest, CollapsesDiscsInDiscOrderRegardlessOfScanOrder)
{
  const auto first = Disc(QStringLiteral("disc1.iso"), 0);
  const auto second = Disc(QStringLiteral("disc2.iso"), 1);
  const UICommon::GameListDiscGroups::Games games{second, first};
  groups.Rebuild(games);
  ASSERT_EQ(groups.GetDiscs(*first).size(), 2u);
  EXPECT_EQ(groups.GetDiscs(*second)[0], first);
  EXPECT_EQ(groups.GetDiscs(*first)[1], second);
  EXPECT_TRUE(groups.IsRepresentative(*first));
  EXPECT_FALSE(groups.IsRepresentative(*second));
}

TEST_F(GameListDiscGroupsTest, KeepsDifferentRevisionsRegionsAndTitlesSeparate)
{
  const auto first = Disc(QStringLiteral("disc1.iso"), 0);
  const auto revision = Disc(QStringLiteral("revision.iso"), 1, 1);
  const auto region = Disc(QStringLiteral("region.iso"), 1, 0, "GAFP01");
  const auto title = Disc(QStringLiteral("title.iso"), 1, 0, "GZLE01");
  const UICommon::GameListDiscGroups::Games games{first, revision, region, title};
  groups.Rebuild(games);
  for (const auto& game : games)
  {
    EXPECT_TRUE(groups.GetDiscs(*game).empty());
    EXPECT_TRUE(groups.IsRepresentative(*game));
  }
}

TEST_F(GameListDiscGroupsTest, DoesNotCollapseCopiesOfASingleDisc)
{
  const auto first = Disc(QStringLiteral("copy1.iso"), 0);
  const auto copy = Disc(QStringLiteral("copy2.iso"), 0);
  const UICommon::GameListDiscGroups::Games games{copy, first};
  groups.Rebuild(games);
  EXPECT_TRUE(groups.GetDiscs(*first).empty());
  EXPECT_TRUE(groups.IsRepresentative(*copy));
}

TEST_F(GameListDiscGroupsTest, RebuildRestoresRemainingDiscAfterRemoval)
{
  const auto first = Disc(QStringLiteral("disc1.iso"), 0);
  const auto second = Disc(QStringLiteral("disc2.iso"), 1);
  const UICommon::GameListDiscGroups::Games games{first, second};
  groups.Rebuild(games);
  EXPECT_FALSE(groups.IsRepresentative(*second));
  const UICommon::GameListDiscGroups::Games remaining{second};
  groups.Rebuild(remaining);
  EXPECT_TRUE(groups.IsRepresentative(*second));
  EXPECT_TRUE(groups.GetDiscs(*second).empty());
}

TEST_F(GameListDiscGroupsTest, DoesNotCollapseModDescriptorsWithTheirBaseGames)
{
  const auto first = Disc(QStringLiteral("disc1.iso"), 0);
  const auto second = Disc(QStringLiteral("disc2.iso"), 1);
  const QString path = directory.filePath(QStringLiteral("mod.json"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  const QJsonObject descriptor{
      {QStringLiteral("type"), QStringLiteral("dolphin-game-mod-descriptor")},
      {QStringLiteral("version"), 1},
      {QStringLiteral("base-file"), QString::fromStdString(first->GetFilePath())},
      {QStringLiteral("display-name"), QStringLiteral("Test Mod")}};
  file.write(QJsonDocument(descriptor).toJson());
  file.close();
  const auto mod = std::make_shared<UICommon::GameFile>(path.toStdString());
  ASSERT_TRUE(mod->IsValid());
  ASSERT_TRUE(mod->IsModDescriptor());
  const UICommon::GameListDiscGroups::Games games{mod, first, second};
  groups.Rebuild(games);
  EXPECT_TRUE(groups.IsRepresentative(*mod));
  EXPECT_TRUE(groups.GetDiscs(*mod).empty());
  EXPECT_EQ(groups.GetDiscs(*first).size(), 2u);
}

TEST(GameListUiTest, DefaultViewportFitsSevenColumnsByThreeRows)
{
  EXPECT_EQ(GameListGrid::CalculateViewportSize(QSize(166, 242), 16, 7, 3, 15), QSize(1305, 790));
}

TEST(GameListUiTest, FormOwnsThePermanentLibraryControlsAndViewStack)
{
  QWidget widget;
  Ui::GameListWidget ui;
  ui.setupUi(&widget);

  EXPECT_EQ(ui.rootLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.rootLayout->spacing(), 0);
  EXPECT_EQ(ui.rootLayout->count(), 2);
  EXPECT_EQ(ui.rootLayout->itemAt(0)->widget(), ui.controlBar);
  EXPECT_EQ(ui.rootLayout->itemAt(1)->widget(), ui.viewStack);

  EXPECT_FALSE(ui.controlBar->isHidden());
  EXPECT_EQ(ui.controlBar->minimumHeight(), 40);
  EXPECT_EQ(ui.controlBar->maximumHeight(), 40);
  ASSERT_EQ(ui.controlsLayout->count(), 6);
  EXPECT_EQ(ui.controlsLayout->itemAt(0)->widget(), ui.listViewButton);
  EXPECT_EQ(ui.controlsLayout->itemAt(1)->widget(), ui.gridViewButton);
  EXPECT_EQ(ui.controlsLayout->itemAt(2)->widget(), ui.gridScaleSlider);
  EXPECT_EQ(ui.controlsLayout->itemAt(3)->widget(), ui.platformFilter);
  EXPECT_EQ(ui.controlsLayout->itemAt(4)->widget(), ui.regionFilter);
  EXPECT_EQ(ui.controlsLayout->itemAt(5)->widget(), ui.searchEdit);

  EXPECT_TRUE(ui.listViewButton->isCheckable());
  EXPECT_TRUE(ui.gridViewButton->isCheckable());
  EXPECT_TRUE(ui.listViewButton->autoExclusive());
  EXPECT_TRUE(ui.gridViewButton->autoExclusive());
  EXPECT_EQ(ui.listViewButton->accessibleName(), QStringLiteral("List View"));
  EXPECT_EQ(ui.gridViewButton->accessibleName(), QStringLiteral("Grid View"));
  EXPECT_EQ(ui.listViewButton->toolTip(), QStringLiteral("List View"));
  EXPECT_EQ(ui.gridViewButton->toolTip(), QStringLiteral("Grid View"));
  EXPECT_EQ(ui.listViewButton->minimumSize(), QSize(32, 32));
  EXPECT_EQ(ui.gridViewButton->minimumSize(), QSize(32, 32));

  EXPECT_EQ(ui.gridScaleSlider->minimum(), 10);
  EXPECT_EQ(ui.gridScaleSlider->maximum(), 200);
  EXPECT_EQ(ui.gridScaleSlider->singleStep(), 10);
  EXPECT_EQ(ui.gridScaleSlider->value(), 100);
  EXPECT_EQ(ui.gridScaleSlider->accessibleName(), QStringLiteral("Grid size"));
  EXPECT_EQ(ui.platformFilter->currentText(), QStringLiteral("All Platforms"));
  EXPECT_EQ(ui.regionFilter->currentText(), QStringLiteral("All Regions"));
  EXPECT_EQ(ui.platformFilter->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);
  EXPECT_EQ(ui.regionFilter->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);

  EXPECT_EQ(ui.searchEdit->placeholderText(), QStringLiteral("Search games..."));
  EXPECT_EQ(ui.searchEdit->accessibleName(), QStringLiteral("Search games"));
  EXPECT_TRUE(ui.searchEdit->isClearButtonEnabled());
  EXPECT_EQ(ui.searchEdit->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);

  EXPECT_EQ(ui.listViewButton->nextInFocusChain(), ui.gridViewButton);
  EXPECT_EQ(ui.gridViewButton->nextInFocusChain(), ui.gridScaleSlider);
  EXPECT_EQ(ui.gridScaleSlider->nextInFocusChain(), ui.platformFilter);
  EXPECT_EQ(ui.platformFilter->nextInFocusChain(), ui.regionFilter);
  EXPECT_EQ(ui.regionFilter->nextInFocusChain(), ui.searchEdit);

  widget.resize(360, 240);
  ui.rootLayout->setGeometry(widget.rect());
  ui.controlsLayout->setGeometry(ui.controlBar->rect());
  EXPECT_EQ(ui.listViewButton->width(), 32);
  EXPECT_EQ(ui.gridViewButton->width(), 32);
  EXPECT_GE(ui.gridScaleSlider->width(), 80);
  EXPECT_GT(ui.platformFilter->width(), 0);
  EXPECT_GT(ui.regionFilter->width(), 0);
  EXPECT_GT(ui.searchEdit->width(), 0);
}
