// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <thread>

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "DolphinQt/BigPictureGraphicsConfig.h"
#include "DolphinQt/BigPictureLibrary.h"
#include "DolphinQt/BigPictureTextInput.h"
#include "NullConfigLoader.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
using O = BigPicture::LibraryOperation;
using Game = BigPicture::LibraryGame;
template <typename Predicate>
bool Wait(Predicate predicate)
{
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate() && std::chrono::steady_clock::now() < until)
  {
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return predicate();
}
Game Entry(std::string path, std::string title, std::string platform = "GameCube",
           std::string region = "NTSC-U", std::vector<std::string> tags = {})
{
  Game game;
  game.path = std::move(path);
  game.title = std::move(title);
  game.platform = std::move(platform);
  game.region = std::move(region);
  game.tags = std::move(tags);
  return game;
}

TEST(BigPictureLibraryTest, PathsSurviveSortFilterAndIncrementalScansWithoutMutatingReaders)
{
  BigPictureLibrary library(nullptr, {});
  auto a = Entry("a.gcm", "Game 10", "GameCube", "NTSC-U", {"Favorite"});
  auto b = Entry("b.gcm", "Game 2", "Wii", "PAL", {"Co-op"});
  b.played_ms = 100;
  library.SetGames({a, b});
  EXPECT_EQ(library.GetSnapshot()->games.front().path, "b.gcm");
  ASSERT_TRUE(library.Execute(O::Select, a.path, 0).empty());
  const auto retained = library.GetSnapshot();
  library.Execute(O::Sort, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->games.front().path, "b.gcm");
  EXPECT_EQ(library.GetSnapshot()->selected_path, a.path);
  library.Execute(O::Platform, {}, 0);
  ASSERT_EQ(library.GetSnapshot()->games.size(), 1u);
  EXPECT_EQ(library.GetSnapshot()->games.front().path, a.path);
  library.Execute(O::ClearFilters, {}, 0);
  auto c = Entry("c.gcm", "Game 1");
  library.SetGames({b, a, c});
  EXPECT_EQ(library.GetSnapshot()->selected_path, a.path);
  library.SetGames({b, c});
  EXPECT_TRUE(library.GetSnapshot()->selected_path.empty());
  EXPECT_EQ(retained->games.size(), 2u);
  EXPECT_EQ(retained->selected_path, a.path);
  EXPECT_FALSE(library.Execute(O::Select, a.path, 0).empty());
}

TEST(BigPictureLibraryTest, DetailedMetadataIsTranslatedAndRetainedByImmutableSnapshots)
{
  BigPictureLibrary library(nullptr, {});
  auto game =
      Entry("E:/Games/日本語/game.iso", "Game", "GameCube", "NTSC-J", {"Favorite", "Co-op"});
  game.game_id = "GTEST1";
  game.size = 1024 * 1024;
  game.played_ms = (2 * 60 + 3) * 60000;
  library.SetGames({game});
  const auto retained = library.GetSnapshot();
  const auto details = retained->games.front().details;
  EXPECT_NE(details.find("Platform: GameCube"), std::string::npos);
  EXPECT_NE(details.find("Region: NTSC-J"), std::string::npos);
  EXPECT_NE(details.find("Game ID: GTEST1"), std::string::npos);
  EXPECT_NE(details.find("Time Played: 2h 3m"), std::string::npos);
  EXPECT_NE(details.find("File Size:"), std::string::npos);
  EXPECT_NE(details.find("日本語"), std::string::npos);
  EXPECT_NE(details.find("Tags: Favorite, Co-op"), std::string::npos);
  game.played_ms = 4 * 60 * 60000;
  library.SetGames({game});
  EXPECT_EQ(retained->games.front().details, details);
  EXPECT_NE(library.GetSnapshot()->games.front().details.find("4h 0m"), std::string::npos);
}

TEST(BigPictureLibraryTest, FiltersCycleBackToAllIncludingAnEmptyCatalog)
{
  BigPictureLibrary library(nullptr, {});
  EXPECT_TRUE(library.Execute(O::Tag, {}, 0).empty());
  library.SetGames({Entry("a", "A", "GameCube", "PAL", {"Favorite"}),
                    Entry("b", "B", "Wii", "NTSC-U", {"Co-op"})});
  library.Execute(O::Tag, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->tag, "Co-op");
  library.Execute(O::Tag, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->tag, "Favorite");
  library.Execute(O::Tag, {}, 0);
  EXPECT_TRUE(library.GetSnapshot()->tag.empty());
  library.Execute(O::Region, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->games.size(), 1u);
  library.Execute(O::ClearFilters, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->games.size(), 2u);
}

TEST(BigPictureLibraryTest, FullRefreshKeepsSelectionUntilTheNewCatalogIsComplete)
{
  BigPictureLibrary library(nullptr, {});
  library.SetGames({Entry("a", "A"), Entry("b", "B")});
  library.Execute(O::Select, "b", 0);
  const auto before = library.GetSnapshot();
  library.SetScanning(true);
  library.SetGames({});
  library.SetGames({Entry("a", "A")});
  EXPECT_EQ(library.GetSnapshot()->selected_path, "b");
  EXPECT_EQ(library.GetSnapshot()->games.size(), 2u);
  library.SetGames({Entry("b", "B"), Entry("a", "A"), Entry("c", "C")});
  library.SetScanning(false);
  EXPECT_EQ(library.GetSnapshot()->selected_path, "b");
  EXPECT_EQ(library.GetSnapshot()->games.size(), 3u);
  EXPECT_EQ(before->games.size(), 2u);
}

TEST(BigPictureLibraryTest, FolderAndInputSettingsDoNotInvalidateIdleGraphicsConfiguration)
{
  Config::Init();
  Config::AddLayer(std::make_unique<NullLoader>());
  const auto initial = BigPicture::ReadGraphicsConfig();
  Config::SetBase(Config::MAIN_RECURSIVE_ISO_PATHS, true);
  Config::SetBase(Config::MAIN_DEFAULT_ISO, std::string("game.iso"));
  EXPECT_EQ(BigPicture::ReadGraphicsConfig(), initial);
  Config::SetBase(Config::MAIN_GFX_BACKEND, std::string("Vulkan"));
  EXPECT_NE(BigPicture::ReadGraphicsConfig(), initial);
  const auto changed_backend = BigPicture::ReadGraphicsConfig();
  Config::SetBase(Config::GFX_VSYNC, true);
  EXPECT_NE(BigPicture::ReadGraphicsConfig(), changed_backend);
  Config::Shutdown();
}

TEST(BigPictureLibraryTest, SurfaceCompatibilityIgnoresRendererOptionsButTracksBackendChanges)
{
  Config::Init();
  Config::AddLayer(std::make_unique<NullLoader>());
  const auto initial = BigPicture::ReadRenderSurfaceConfig();
  Config::SetBase(Config::GFX_VSYNC, true);
  Config::SetBase(Config::MAIN_EMULATION_SPEED, 2.0f);
  EXPECT_EQ(BigPicture::ReadRenderSurfaceConfig(), initial);
  Config::SetBase(Config::MAIN_GFX_BACKEND, std::string("OGL"));
  EXPECT_NE(BigPicture::ReadRenderSurfaceConfig(), initial);
  const auto opengl = BigPicture::ReadRenderSurfaceConfig();
  Config::SetBase(Config::GFX_STEREO_MODE, StereoMode::QuadBuffer);
  EXPECT_NE(BigPicture::ReadRenderSurfaceConfig(), opengl);
  Config::Shutdown();
}

TEST(BigPictureLibraryTest, SearchUsesUnicodeCaseFoldingAndCodePointErasure)
{
  BigPictureLibrary library(nullptr, {});
  library.SetGames({Entry("a", "ПРИВЕТ 日本語"), Entry("b", "Other")});
  library.Execute(O::Search, {}, 0);
  for (const auto& key : {"п", "р", "и", "в", "е", "т"})
    ASSERT_TRUE(library.Execute(O::TextKey, key, 0).empty());
  EXPECT_EQ(library.GetSnapshot()->games.size(), 2u);
  library.Execute(O::TextDone, {}, 0);
  ASSERT_EQ(library.GetSnapshot()->games.size(), 1u);
  EXPECT_EQ(library.GetSnapshot()->games.front().path, "a");
  library.Execute(O::TextKey, "😀", 0);
  library.Execute(O::TextErase, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->draft, "привет");
  EXPECT_FALSE(library.Execute(O::TextKey, "oversized-value", 0).empty());
}

TEST(BigPictureLibraryTest, ClearingSearchPreservesOtherFiltersAndCanClearAnUncommittedDraft)
{
  BigPictureLibrary library(nullptr, {{}, {}, {}, {}, {}, {}, [] { return true; }});
  library.SetGames({Entry("a.iso", "First", "GameCube"), Entry("b.iso", "Second", "Wii")});
  library.Execute(O::Platform, {}, 0);
  const auto platform = library.GetSnapshot()->platform;
  library.Execute(O::Search, {}, 0);
  EXPECT_TRUE(library.GetSnapshot()->native_text_input);
  library.Execute(O::TextReplace, "First", 0);
  library.Execute(O::TextDone, {}, 0);
  EXPECT_EQ(library.GetSnapshot()->query, "First");
  library.Execute(O::ClearSearch, {}, 0);
  EXPECT_TRUE(library.GetSnapshot()->query.empty());
  EXPECT_EQ(library.GetSnapshot()->platform, platform);
  library.Execute(O::Search, {}, 0);
  library.Execute(O::ControllerKeyboard, {}, 0);
  EXPECT_FALSE(library.GetSnapshot()->native_text_input);
  library.Execute(O::TextKey, "text", 0);
  library.Execute(O::TextClear, {}, 0);
  EXPECT_TRUE(library.GetSnapshot()->draft.empty());
}

TEST(BigPictureLibraryTest, DiscPickerUsesUnfilteredLibraryAndPrefersOtherDiscsOfTheRunningGame)
{
  BigPictureLibrary library(nullptr, {});
  auto first = Entry("first.gcm", "Matching Game (Disc 1)");
  auto second = Entry("second.gcm", "Matching Game (Disc 2)");
  first.game_id = second.game_id = "GAME01";
  library.SetGames({Entry("other.iso", "Other"), first, second, Entry("program.dol", "Program")});
  library.Execute(O::Search, {}, 0);
  library.Execute(O::TextReplace, "Other", 0);
  library.Execute(O::TextDone, {}, 0);
  library.SetDiscContext(first.path, first.game_id, "Matching Game", {});
  library.Execute(O::OpenDisc, {}, 0);
  const auto snapshot = library.GetSnapshot();
  EXPECT_TRUE(snapshot->picker_library);
  EXPECT_FALSE(snapshot->picker_loading);
  ASSERT_EQ(snapshot->files.size(), 3u);
  EXPECT_EQ(snapshot->files[0].path, second.path);
  EXPECT_EQ(snapshot->files[1].path, first.path);
  EXPECT_EQ(snapshot->games.size(), 1u);
}

TEST(BigPictureLibraryTest, EmptyDiscLibraryStartsAtConfiguredFolderAndBrowseFilesUsesGameFolder)
{
  QTemporaryDir configured;
  QTemporaryDir running;
  ASSERT_TRUE(configured.isValid());
  ASSERT_TRUE(running.isValid());
  BigPictureLibrary library(nullptr, {});
  library.SetDiscContext({}, {}, {}, {"missing-folder", configured.path().toStdString()});
  library.Execute(O::OpenDisc, {}, 0);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  EXPECT_EQ(library.GetSnapshot()->directory, configured.path().toStdString());
  EXPECT_FALSE(library.GetSnapshot()->picker_library);
  library.SetGames({Entry("library.iso", "Library")});
  library.SetDiscContext((running.path() + QStringLiteral("/game.iso")).toStdString(), {}, {},
                         {configured.path().toStdString()});
  library.Execute(O::OpenDisc, {}, 0);
  EXPECT_TRUE(library.GetSnapshot()->picker_library);
  library.Execute(O::BrowseDiscs, {}, 0);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  EXPECT_EQ(library.GetSnapshot()->directory, running.path().toStdString());
}

TEST(BigPictureTextInputTest, PhysicalKeyboardUsesNativeEditorWithQuickClearAndUnicode)
{
  bool requested_system = false;
  BigPictureTextInput input(nullptr, QStringLiteral("Search"), QStringLiteral("Initial"),
                            {true, [&] { requested_system = true; }, [] { return false; }});
  input.show();
  QCoreApplication::processEvents();
  auto* edit = input.findChild<QLineEdit*>();
  ASSERT_NE(edit, nullptr);
  EXPECT_TRUE(edit->isClearButtonEnabled());
  edit->setText(QStringLiteral("日本語"));
  EXPECT_EQ(input.GetText(), QStringLiteral("日本語"));
  for (auto* button : input.findChildren<QPushButton*>())
    if (button->text() == QStringLiteral("Clear"))
      button->click();
  EXPECT_TRUE(input.GetText().isEmpty());
  EXPECT_FALSE(requested_system);
}

TEST(BigPictureTextInputTest, UnavailableSystemKeyboardFallsBackWithoutCommittingDraft)
{
  bool requested_system = false;
  int result = -1;
  BigPictureTextInput input(nullptr, QStringLiteral("Search"), QStringLiteral("Original"),
                            {false, [&] { requested_system = true; }, [] { return false; }});
  QObject::connect(&input, &QDialog::finished, [&](int code) { result = code; });
  input.show();
  ASSERT_TRUE(Wait([&] { return result != -1; }));
  EXPECT_TRUE(requested_system);
  EXPECT_EQ(result, BigPictureTextInput::ControllerKeyboard);
  EXPECT_EQ(input.GetText(), QStringLiteral("Original"));
}

TEST(BigPictureTextInputTest, AvailableSystemKeyboardKeepsNativeEditorAndCancelDoesNotAccept)
{
  bool checked = false;
  int result = -1;
  BigPictureTextInput input(nullptr, QStringLiteral("Search"), QStringLiteral("Original"),
                            {false, [] {},
                             [&] {
                               checked = true;
                               return true;
                             }});
  QObject::connect(&input, &QDialog::finished, [&](int code) { result = code; });
  input.show();
  ASSERT_TRUE(Wait([&] { return checked; }));
  EXPECT_TRUE(input.isVisible());
  EXPECT_EQ(result, -1);
  input.reject();
  EXPECT_EQ(result, QDialog::Rejected);
}

TEST(BigPictureLibraryTest, FolderSetupUsesCanonicalPathsAndRejectsStaleListings)
{
  QTemporaryDir folder;
  ASSERT_TRUE(folder.isValid());
  std::string added;
  bool recursive = false;
  BigPictureLibrary library(nullptr, {{},
                                      [&](const auto& path) { added = path; },
                                      {},
                                      {},
                                      [&] { return recursive; },
                                      [&](bool value) { recursive = value; }});
  library.Execute(O::AddFolder, {}, 0);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  const auto old_revision = library.GetSnapshot()->picker_revision;
  EXPECT_TRUE(
      library.Execute(O::EnterDirectory, folder.path().toStdString(), old_revision).empty());
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  EXPECT_FALSE(library.Execute(O::ChooseFolder, folder.path().toStdString(), old_revision).empty());
  EXPECT_TRUE(added.empty());
  library.Execute(O::Recursive, {}, 0);
  EXPECT_TRUE(recursive);
  EXPECT_TRUE(library.GetSnapshot()->recursive);
  const auto snapshot = library.GetSnapshot();
  EXPECT_TRUE(
      library.Execute(O::ChooseFolder, snapshot->directory, snapshot->picker_revision).empty());
  EXPECT_EQ(added, QFileInfo(folder.path()).canonicalFilePath().toStdString());
}

TEST(BigPictureLibraryTest, LatestPickerWinsAndDiscListingsExcludeExecutables)
{
  QTemporaryDir folder;
  ASSERT_TRUE(folder.isValid());
  for (const auto& name :
       {QStringLiteral("game.iso"), QStringLiteral("boot.dol"), QStringLiteral("note.txt")})
  {
    QFile file(folder.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  }
  BigPictureLibrary library(nullptr, {});
  for (int i = 0; i < 50; ++i)
    library.Execute(i == 49 ? O::OpenDisc : O::AddFolder, {}, 0);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  library.Execute(O::EnterDirectory, folder.path().toStdString(),
                  library.GetSnapshot()->picker_revision);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  const auto snapshot = library.GetSnapshot();
  ASSERT_EQ(snapshot->files.size(), 1u);
  EXPECT_EQ(snapshot->files.front().name, "game.iso");
  EXPECT_FALSE(snapshot->files.front().directory);
  library.Execute(O::OpenFile, {}, 0);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  library.Execute(O::EnterDirectory, folder.path().toStdString(),
                  library.GetSnapshot()->picker_revision);
  ASSERT_TRUE(Wait([&] { return !library.GetSnapshot()->picker_loading; }));
  EXPECT_EQ(library.GetSnapshot()->files.size(), 2u);
  EXPECT_EQ(snapshot->files.size(), 1u);
}

TEST(BigPictureLibraryTest, CoverDecodeIsBoundedAndOldCatalogRequestsCannotInstallTextures)
{
  QImage image(600, 1000, QImage::Format_RGBA8888);
  image.fill(Qt::red);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  ASSERT_TRUE(image.save(&buffer, "PNG"));
  const auto owner = std::this_thread::get_id();
  int reads = 0;
  BigPictureLibrary library(nullptr, {{}, {}, {}, [&](const std::string&) {
                                        EXPECT_EQ(std::this_thread::get_id(), owner);
                                        ++reads;
                                        return std::vector<unsigned char>(bytes.begin(),
                                                                          bytes.end());
                                      }});
  std::vector<Game> games;
  for (int i = 0; i < 32; ++i)
    games.push_back(Entry(std::to_string(i), "Game"));
  library.SetGames(games);
  const auto revision = library.GetSnapshot()->catalog_revision;
  for (const auto& game : games)
  {
    library.Execute(O::Cover, game.path, revision);
    ASSERT_TRUE(Wait([&] { return library.GetSnapshot()->covers.contains(game.path); }));
    const auto& cover = *library.GetSnapshot()->covers.at(game.path);
    EXPECT_LE(cover.width, 256u);
    EXPECT_LE(cover.height, 384u);
    EXPECT_EQ(cover.rgba.size(), static_cast<std::size_t>(cover.width) * cover.height * 4);
    EXPECT_LE(library.GetSnapshot()->covers.size(), 24u);
  }
  const auto retained = library.GetSnapshot();
  library.SetGames(games);
  library.Execute(O::Cover, "0", revision);
  EXPECT_EQ(reads, 32);
  EXPECT_TRUE(library.GetSnapshot()->covers.empty());
  EXPECT_EQ(retained->covers.size(), 24u);
  const auto old_revision = library.GetSnapshot()->catalog_revision;
  library.Execute(O::Cover, "0", old_revision);
  library.SetGames(games);
  const auto new_revision = library.GetSnapshot()->catalog_revision;
  library.Execute(O::Cover, "1", new_revision);
  ASSERT_TRUE(Wait([&] { return library.GetSnapshot()->covers.contains("1"); }));
  EXPECT_FALSE(library.GetSnapshot()->covers.contains("0"));
  EXPECT_EQ(library.GetSnapshot()->covers.at("1")->revision, new_revision);
}
}  // namespace
