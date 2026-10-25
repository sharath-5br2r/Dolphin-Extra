// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QMargins>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QStringList>
#include <QToolBar>
#include <gtest/gtest.h>

#include "DolphinQt/QtUtils/NonAutodismissibleMenu.h"

#include "ui_MainWindow.h"

static QStringList MenuActionNames(const QMenu* menu)
{
  QStringList names;
  for (const QAction* const action : menu->actions())
  {
    if (action->isSeparator())
      continue;

    const QMenu* const submenu = action->menu();
    names.push_back(submenu ? submenu->objectName() : action->objectName());
  }
  return names;
}

TEST(MainWindowUiTest, FormOwnsTheCentralStackAndPromotedStatusBar)
{
  QMainWindow window;
  Ui::MainWindow ui;

  ui.setupUi(&window);

  EXPECT_EQ(window.centralWidget(), ui.mainStack);
  EXPECT_EQ(window.statusBar(), ui.statusBar);
  EXPECT_EQ(ui.mainStack->count(), 1);
  EXPECT_EQ(ui.mainStack->currentWidget(), ui.gameListPage);
  EXPECT_EQ(ui.gameListLayout->contentsMargins(), QMargins());
  EXPECT_EQ(ui.gameListLayout->spacing(), 0);
  EXPECT_TRUE(window.acceptDrops());
}

TEST(MainWindowUiTest, PromotedStatusBarKeepsTheExistingGameCountText)
{
  QMainWindow window;
  Ui::MainWindow ui;
  ui.setupUi(&window);

  ui.statusBar->OnGameCountUpdated(10, 8);

  EXPECT_EQ(ui.statusBar->currentMessage(),
            QStringLiteral("10 game(s) in your collection (8 visible, 2 filtered)"));
}

TEST(MainWindowUiTest, FormOwnsTheToolbarAndItsStaticActions)
{
  QMainWindow window;
  Ui::MainWindow ui;
  ui.setupUi(&window);

  EXPECT_EQ(window.toolBarArea(ui.toolbar), Qt::TopToolBarArea);
  EXPECT_EQ(ui.toolbar->objectName(), QStringLiteral("toolbar"));
  EXPECT_EQ(ui.toolbar->toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
  EXPECT_EQ(ui.toolbar->iconSize(), QSize(32, 32));
  EXPECT_FALSE(ui.toolbar->isFloatable());

  QStringList action_names;
  for (const QAction* const action : ui.toolbar->actions())
  {
    if (!action->isSeparator())
      action_names.push_back(action->objectName());
  }

  EXPECT_EQ(
      action_names,
      QStringList({QStringLiteral("actionToolbarStep"), QStringLiteral("actionToolbarStepOver"),
                   QStringLiteral("actionToolbarStepOut"), QStringLiteral("actionToolbarSkip"),
                   QStringLiteral("actionToolbarShowPC"), QStringLiteral("actionToolbarSetPC"),
                   QStringLiteral("actionToolbarOpen"), QStringLiteral("actionToolbarRefresh"),
                   QStringLiteral("actionToolbarPlayPause"), QStringLiteral("actionToolbarStop"),
                   QStringLiteral("actionToolbarFullScreen"),
                   QStringLiteral("actionToolbarScreenShot"),
                   QStringLiteral("actionToolbarSettings"), QStringLiteral("actionToolbarGraphics"),
                   QStringLiteral("actionToolbarControllers")}));
}

TEST(MainWindowUiTest, FormToolbarAndDynamicDockRestoreStateSavedByThePreviousShell)
{
  QMainWindow previous_window;
  auto* const previous_toolbar = new QToolBar(&previous_window);
  previous_toolbar->setObjectName(QStringLiteral("toolbar"));
  previous_window.addToolBar(Qt::BottomToolBarArea, previous_toolbar);
  previous_toolbar->hide();
  auto* const previous_dock = new QDockWidget(&previous_window);
  previous_dock->setObjectName(QStringLiteral("log"));
  previous_window.addDockWidget(Qt::RightDockWidgetArea, previous_dock);
  previous_dock->hide();
  const QByteArray previous_state = previous_window.saveState();

  QMainWindow window;
  Ui::MainWindow ui;
  ui.setupUi(&window);
  auto* const dock = new QDockWidget(&window);
  dock->setObjectName(QStringLiteral("log"));
  window.addDockWidget(Qt::LeftDockWidgetArea, dock);

  ASSERT_TRUE(window.restoreState(previous_state));
  EXPECT_EQ(window.toolBarArea(ui.toolbar), Qt::BottomToolBarArea);
  EXPECT_TRUE(ui.toolbar->isHidden());
  EXPECT_EQ(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
  EXPECT_TRUE(dock->isHidden());
}

TEST(MainWindowUiTest, FormOwnsTheStaticMenuHierarchy)
{
  QMainWindow window;
  Ui::MainWindow ui;
  ui.setupUi(&window);

  QStringList top_level_menus;
  for (const QAction* const action : ui.menubar->actions())
  {
    ASSERT_NE(action->menu(), nullptr);
    top_level_menus.push_back(action->menu()->objectName());
  }
  EXPECT_EQ(top_level_menus,
            QStringList({QStringLiteral("menuFile"), QStringLiteral("menuEmulation"),
                         QStringLiteral("menuMovie"), QStringLiteral("menuOptions"),
                         QStringLiteral("menuTools"), QStringLiteral("menuView"),
                         QStringLiteral("menuHelp")}));

  EXPECT_NE(dynamic_cast<QtUtils::NonAutodismissibleMenu*>(ui.menuMovie), nullptr);
  EXPECT_NE(dynamic_cast<QtUtils::NonAutodismissibleMenu*>(ui.menuOptions), nullptr);
  EXPECT_NE(dynamic_cast<QtUtils::NonAutodismissibleMenu*>(ui.menuView), nullptr);
  EXPECT_NE(dynamic_cast<QtUtils::NonAutodismissibleMenu*>(ui.menuListColumns), nullptr);
  EXPECT_NE(dynamic_cast<QtUtils::NonAutodismissibleMenu*>(ui.menuShowPlatforms), nullptr);
  EXPECT_NE(dynamic_cast<QtUtils::NonAutodismissibleMenu*>(ui.menuShowRegions), nullptr);

  EXPECT_EQ(
      MenuActionNames(ui.menuFile),
      QStringList({QStringLiteral("actionFileOpen"), QStringLiteral("actionFileChangeDisc"),
                   QStringLiteral("actionFileEjectDisc"),
                   QStringLiteral("actionFileOpenUserFolder"), QStringLiteral("actionFileExit")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuEmulation),
      QStringList({QStringLiteral("actionEmulationPlay"), QStringLiteral("actionEmulationPause"),
                   QStringLiteral("actionEmulationStop"), QStringLiteral("actionEmulationReset"),
                   QStringLiteral("actionEmulationFullscreen"),
                   QStringLiteral("actionEmulationFrameAdvance"),
                   QStringLiteral("actionEmulationScreenshot"), QStringLiteral("menuStateLoad"),
                   QStringLiteral("menuStateSave"), QStringLiteral("menuStateSlot")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuMovie),
      QStringList(
          {QStringLiteral("actionMovieStartRecording"), QStringLiteral("actionMoviePlayRecording"),
           QStringLiteral("actionMovieStopRecording"), QStringLiteral("actionMovieExportRecording"),
           QStringLiteral("actionMovieReadOnly"), QStringLiteral("actionMovieTasInput"),
           QStringLiteral("actionMoviePauseAtEnd"), QStringLiteral("actionMovieEnableWindow"),
           QStringLiteral("actionMovieConfigureWindow"), QStringLiteral("actionMovieDumpFrames"),
           QStringLiteral("actionMovieDumpAudio")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuOptions),
      QStringList(
          {QStringLiteral("actionOptionsConfiguration"), QStringLiteral("actionOptionsGraphics"),
           QStringLiteral("actionOptionsAudio"), QStringLiteral("actionOptionsControllers"),
           QStringLiteral("actionOptionsHotkeys"), QStringLiteral("actionOptionsFreeLook")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuTools),
      QStringList(
          {QStringLiteral("actionToolsResourcePackManager"),
           QStringLiteral("actionToolsCheatsManager"), QStringLiteral("actionToolsFifoPlayer"),
           QStringLiteral("menuEmulatedUsbDevices"), QStringLiteral("actionToolsStartNetPlay"),
           QStringLiteral("actionToolsBrowseNetPlay"), QStringLiteral("menuGameCubeMainMenu"),
           QStringLiteral("actionToolsMemoryCardManager"),
           QStringLiteral("actionToolsBootSystemMenu"), QStringLiteral("actionToolsInstallWad"),
           QStringLiteral("menuManageNand"), QStringLiteral("menuOnlineUpdate"),
           QStringLiteral("actionToolsImportWiiSave"), QStringLiteral("actionToolsImportWiiSaves"),
           QStringLiteral("actionToolsExportWiiSaves"), QStringLiteral("menuConnectWiiRemotes")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuView),
      QStringList({QStringLiteral("actionViewBigPicture"), QStringLiteral("actionViewShowLog"),
                   QStringLiteral("actionViewShowLogConfiguration"),
                   QStringLiteral("actionViewShowToolbar"), QStringLiteral("actionViewLockWidgets"),
                   QStringLiteral("actionViewList"), QStringLiteral("actionViewGrid"),
                   QStringLiteral("menuListColumns"), QStringLiteral("menuShowPlatforms"),
                   QStringLiteral("menuShowRegions"), QStringLiteral("actionViewShowGameCount"),
                   QStringLiteral("actionViewPurgeGameListCache"),
                   QStringLiteral("actionViewSearch")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuHelp),
      QStringList({QStringLiteral("actionHelpWebsite"), QStringLiteral("actionHelpDocumentation"),
                   QStringLiteral("actionHelpGitHub"), QStringLiteral("actionHelpBugTracker"),
                   QStringLiteral("actionHelpAbout")}));

  EXPECT_EQ(
      MenuActionNames(ui.menuStateLoad),
      QStringList({QStringLiteral("actionStateLoadFile"),
                   QStringLiteral("actionStateLoadSelectedSlot"),
                   QStringLiteral("menuStateLoadSlots"), QStringLiteral("actionStateLoadUndo")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuStateSave),
      QStringList({QStringLiteral("actionStateSaveFile"),
                   QStringLiteral("actionStateSaveSelectedSlot"),
                   QStringLiteral("actionStateSaveOldestSlot"),
                   QStringLiteral("menuStateSaveSlots"), QStringLiteral("actionStateSaveUndo")}));
  EXPECT_EQ(
      MenuActionNames(ui.menuEmulatedUsbDevices),
      QStringList({QStringLiteral("actionToolsSkylandersPortal"),
                   QStringLiteral("actionToolsInfinityBase"), QStringLiteral("actionToolsWiiSpeak"),
                   QStringLiteral("actionToolsLogitechMicrophone")}));
  EXPECT_EQ(MenuActionNames(ui.menuGameCubeMainMenu),
            QStringList({QStringLiteral("actionToolsGameCubeNtscJ"),
                         QStringLiteral("actionToolsGameCubeNtscU"),
                         QStringLiteral("actionToolsGameCubePal"),
                         QStringLiteral("actionToolsGameCubeTriforce")}));
  EXPECT_EQ(MenuActionNames(ui.menuManageNand),
            QStringList({QStringLiteral("actionToolsImportNandBackup"),
                         QStringLiteral("actionToolsCheckNand"),
                         QStringLiteral("actionToolsExtractCertificates")}));
  EXPECT_EQ(MenuActionNames(ui.menuOnlineUpdate),
            QStringList({QStringLiteral("actionToolsUpdateCurrentRegion"),
                         QStringLiteral("actionToolsUpdateEurope"),
                         QStringLiteral("actionToolsUpdateJapan"),
                         QStringLiteral("actionToolsUpdateKorea"),
                         QStringLiteral("actionToolsUpdateUnitedStates")}));

  EXPECT_TRUE(ui.menuStateLoadSlots->actions().isEmpty());
  EXPECT_TRUE(ui.menuStateSaveSlots->actions().isEmpty());
  EXPECT_TRUE(ui.menuStateSlot->actions().isEmpty());
  EXPECT_TRUE(ui.menuConnectWiiRemotes->actions().isEmpty());
  EXPECT_TRUE(ui.menuListColumns->actions().isEmpty());
  EXPECT_TRUE(ui.menuShowPlatforms->actions().isEmpty());
  EXPECT_EQ(MenuActionNames(ui.menuShowRegions),
            QStringList({QStringLiteral("actionViewShowAllRegions"),
                         QStringLiteral("actionViewHideAllRegions")}));
}
