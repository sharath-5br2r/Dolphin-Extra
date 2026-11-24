// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QMenuBar>
#include <QMimeData>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleHints>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QWindow>
#include <QMessageBox>
#include <QByteArray>
#include <QJsonDocument>

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <optional>
#include <set>
#include <utility>
#include <variant>

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <signal.h>

#include "QtUtils/SignalDaemon.h"
#endif

#ifndef _WIN32
#include <qpa/qplatformnativeinterface.h>
#else
#include <windows.h>
#endif

#include "AudioCommon/AudioCommon.h"
#ifdef _WIN32
#include "AudioCommon/WASAPIStream.h"
#endif
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/Version.h"
#include "Common/WindowSystemInfo.h"
#include "Common/HttpRequest.h"
#include "Common/scmrev.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CommonTitles.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Config/UISettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FreeLookManager.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/NetPlayServer.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/TimePlayed.h"
#include "Core/WiiForwarder.h"
#include "Core/WiiUtils.h"

#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/NANDImporter.h"
#include "DiscIO/RiivolutionPatcher.h"

#include "Common/BigPicture/PresentationState.h"
#include "Common/CommonPaths.h"
#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/Achievements/AchievementsWindow.h"
#include "DolphinQt/BigPictureHost.h"
#include "DolphinQt/BigPictureLibrary.h"
#include "DolphinQt/BigPictureSettings.h"
#include "DolphinQt/BigPictureTextInput.h"
#include "DolphinQt/CheatsManager.h"
#include "DolphinQt/Config/FreeLookWindow.h"
#include "DolphinQt/Config/LogConfigWidget.h"
#include "DolphinQt/Config/LogWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/Debugger/AssemblerWidget.h"
#include "DolphinQt/Debugger/BreakpointWidget.h"
#include "DolphinQt/Debugger/CodeViewWidget.h"
#include "DolphinQt/Debugger/CodeWidget.h"
#include "DolphinQt/Debugger/JITWidget.h"
#include "DolphinQt/Debugger/MemoryWidget.h"
#include "DolphinQt/Debugger/NetworkWidget.h"
#include "DolphinQt/Debugger/RegisterWidget.h"
#include "DolphinQt/Debugger/ThreadWidget.h"
#include "DolphinQt/Debugger/WatchWidget.h"
#include "DolphinQt/DiscordHandler.h"
#include "DolphinQt/EmulatedUSB/LogitechMicWindow.h"
#include "DolphinQt/EmulatedUSB/WiiSpeakWindow.h"
#include "DolphinQt/EmulationStatusWidget.h"
#include "DolphinQt/FIFO/FIFOPlayerWindow.h"
#include "DolphinQt/GCMemcardManager.h"
#include "DolphinQt/GameCount.h"
#include "DolphinQt/GameList/GameList.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/HotkeyScheduler.h"
#include "DolphinQt/InfinityBase/InfinityBaseWindow.h"
#include "DolphinQt/MenuBar.h"
#include "DolphinQt/NKitWarningDialog.h"
#include "DolphinQt/NetPlay/NetPlayBrowser.h"
#include "DolphinQt/NetPlay/NetPlayDialog.h"
#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"
#include "DolphinQt/UpdaterDialogs/InstallUpdateDialog.h"
#include "DolphinQt/UpdaterDialogs/UpdateDialog.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/FileOpenEventFilter.h"
#include "DolphinQt/QtUtils/MenuFontFallbacks.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/QtUtils/WindowActivationEventFilter.h"
#include "DolphinQt/RenderWidget.h"
#include "DolphinQt/ResourcePackManager.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/RiivolutionBootWidget.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/InterfaceSettingsUtils.h"
#include "DolphinQt/SkylanderPortal/SkylanderPortalWindow.h"
#include "DolphinQt/TAS/GBATASInputWindow.h"
#include "DolphinQt/TAS/GCTASInputWindow.h"
#include "DolphinQt/TAS/WiiTASInputWindow.h"
#include "DolphinQt/ToolBar.h"
#include "DolphinQt/Updater.h"
#include "DolphinQt/WiiUpdate.h"
#include "InputCommon/BigPictureInput.h"
#include "VideoCommon/BigPicture/Runtime.h"
#include "VideoCommon/BigPicture/StandaloneSession.h"
#include "VideoCommon/VideoBackendBase.h"
#ifdef _WIN32
#include "DolphinQt/resource.h"
#endif

#include "ui_MainWindow.h"

#include "UICommon/DiscordPresence.h"
#include "UICommon/GameFile.h"
#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/ResourcePack.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/VideoBackendBase.h"

namespace
{
constexpr int DEFAULT_GRID_COLUMNS = 7;
constexpr int DEFAULT_GRID_ROWS = 3;
constexpr int GRACEFUL_SHUTDOWN_TIMEOUT_MS = 10'000;

#ifdef _WIN32
void SetWindowsAppIcon(HWND window)
{
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  const auto load_icon = [instance](int width, int height) {
    return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, width,
                                         height, LR_DEFAULTCOLOR | LR_SHARED));
  };

  if (const HICON icon = load_icon(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)))
  {
    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SetClassLongPtrW(window, GCLP_HICON, reinterpret_cast<LONG_PTR>(icon));
  }

  if (const HICON icon = load_icon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)))
  {
    SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    SetClassLongPtrW(window, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(icon));
  }
}
#endif
}  // namespace

#ifdef HAVE_XRANDR
#include "UICommon/X11Utils.h"
// This #define within X11/X.h conflicts with our WiimoteSource enum.
#undef None
#endif

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
void MainWindow::OnSignal()
{
  close();
}

static void InstallSignalHandler()
{
  struct sigaction sa;
  sa.sa_handler = &SignalDaemon::HandleInterrupt;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_RESETHAND;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}
#endif

static WindowSystemType GetWindowSystemType()
{
  // Determine WSI type based on Qt platform.
  QString platform_name = QGuiApplication::platformName();
  if (platform_name == QStringLiteral("windows"))
    return WindowSystemType::Windows;
  else if (platform_name == QStringLiteral("cocoa"))
    return WindowSystemType::MacOS;
  else if (platform_name == QStringLiteral("xcb"))
    return WindowSystemType::X11;
  else if (platform_name == QStringLiteral("wayland"))
    return WindowSystemType::Wayland;
  else if (platform_name == QStringLiteral("haiku"))
    return WindowSystemType::Haiku;

  ModalMessageBox::critical(
      nullptr, QStringLiteral("Error"),
      QString::asprintf("Unknown Qt platform: %s", platform_name.toStdString().c_str()));
  return WindowSystemType::Headless;
}

static WindowSystemInfo GetWindowSystemInfo(QWindow* window)
{
  WindowSystemInfo wsi;
  wsi.type = GetWindowSystemType();

  // Our Win32 Qt external doesn't have the private API.
#if defined(WIN32) || defined(__APPLE__) || defined(__HAIKU__)
  wsi.render_window = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
  wsi.render_surface = wsi.render_window;
#else
  QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
  wsi.display_connection = pni->nativeResourceForWindow("display", window);
  if (wsi.type == WindowSystemType::Wayland)
    wsi.render_window = window ? pni->nativeResourceForWindow("surface", window) : nullptr;
  else
    wsi.render_window = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
  wsi.render_surface = wsi.render_window;
#endif
  wsi.render_surface_scale = window ? static_cast<float>(window->devicePixelRatio()) : 1.0f;

  return wsi;
}

static std::vector<std::string> StringListToStdVector(const QStringList& list)
{
  std::vector<std::string> result;
  result.reserve(list.size());

  for (const QString& s : list)
    result.push_back(s.toStdString());

  return result;
}

static std::string ContinuationPathForBoot(const BootParameters& boot)
{
  if (const auto* disc = std::get_if<BootParameters::Disc>(&boot.parameters))
    return State::GetContinuationPath(disc->volume->GetGameID(disc->volume->GetGamePartition()));
  if (const auto* executable = std::get_if<BootParameters::Executable>(&boot.parameters))
    return State::GetContinuationPath(SConfig::MakeGameID(
        QFileInfo(QString::fromStdString(executable->path)).fileName().toStdString()));
  if (const auto* wad = std::get_if<DiscIO::VolumeWAD>(&boot.parameters))
    return State::GetContinuationPath(fmt::format("title-{:016x}", wad->GetTMD().GetTitleId()));
  if (const auto* title = std::get_if<BootParameters::NANDTitle>(&boot.parameters))
    return State::GetContinuationPath(fmt::format("title-{:016x}", title->id));
  return {};
}

static bool ContinuationLoadingAllowed()
{
  return !NetPlay::IsNetPlayRunning() && !AchievementManager::GetInstance().IsHardcoreModeActive();
}

MainWindow::MainWindow(Core::System& system, std::unique_ptr<BootParameters> boot_parameters,
                       const std::string& movie_path, bool start_big_picture)
    : QMainWindow(nullptr), m_system(system), m_ui(std::make_unique<Ui::MainWindow>())
{
  m_ui->setupUi(this);

  setWindowTitle(QString::fromStdString(Common::GetScmRevStr()));
  setAttribute(Qt::WA_NativeWindow);
  setWindowIcon(Resources::GetAppIcon());
#ifdef _WIN32
  SetWindowsAppIcon(reinterpret_cast<HWND>(winId()));
#endif

  m_graceful_shutdown_timer = new QTimer(this);
  m_graceful_shutdown_timer->setSingleShot(true);
  connect(m_graceful_shutdown_timer, &QTimer::timeout, this, [this] {
    if (m_stop_requested && !Core::IsUninitialized(m_system))
      ForceStop();
  });

  CreateComponents();
  m_big_picture_requested = start_big_picture && !Settings::Instance().IsBatchModeEnabled();
  m_big_picture_settings = std::make_unique<BigPictureSettings>(this, [this] {
    if (m_big_picture_host && !m_shutting_down)
      m_big_picture_host->Refresh();
  });
  connect(m_big_picture_settings.get(), &BigPictureSettings::SettingApplied, this,
          [this](const QString& setting_id) {
            const auto id = setting_id.toStdString();
            auto& settings = Settings::Instance();
            if (id == "native.main_use_built_in_title_database" ||
                id == "native.main_recursive_iso_paths")
              emit settings.GameListRefreshRequested();
            else if (id == "native.main_use_game_covers")
              settings.RefreshMetadata();
            else if (id == "native.main_keep_window_on_top")
              emit settings.KeepWindowOnTopChanged(Config::Get(Config::MAIN_KEEP_WINDOW_ON_TOP));
            else if (id == "native.main_show_cursor")
              emit settings.CursorVisibilityChanged();
            else if (id == "native.main_lock_cursor")
              emit settings.LockCursorChanged();
            else if (id == "native.main_disable_screensaver")
              UpdateScreenSaverInhibition();
            else if (id == "native.main_use_panic_handlers")
              Common::SetEnableAlert(Config::Get(Config::MAIN_USE_PANIC_HANDLERS));
          });

  m_big_picture_library = new BigPictureLibrary(
      this,
      {[this] {
         // Covers and listings arrive in bursts. Coalesce them into one state read per event
         // loop turn; a command dispatch refreshes the state itself once it completes.
         if (!m_big_picture_host || std::exchange(m_big_picture_refresh_queued, true))
           return;
         QTimer::singleShot(0, this, [this] {
           m_big_picture_refresh_queued = false;
           if (m_big_picture_host && !m_shutting_down)
             m_big_picture_host->Refresh();
         });
       },
       [](const std::string& path) { Settings::Instance().AddPath(QString::fromStdString(path)); },
       [] { Settings::Instance().RefreshGameList(); },
       [this](const std::string& path) {
         const auto game = m_game_list->FindGame(path);
         if (!game)
           return std::vector<unsigned char>{};
         const auto& cover = game->GetCoverImage().buffer;
         return cover.size() <= 16 * 1024 * 1024 ? cover : std::vector<unsigned char>{};
       },
       [] { return Config::Get(Config::MAIN_RECURSIVE_ISO_PATHS); },
       [](bool recursive) {
         Config::SetBase(Config::MAIN_RECURSIVE_ISO_PATHS, recursive);
         Settings::Instance().RefreshGameList();
       },
       [] { return true; }, [] { return StringListToStdVector(Settings::Instance().GetPaths()); },
       [](const std::string& path) {
         Settings::Instance().RemovePath(QString::fromStdString(path));
       }});
  // Desktop configuration can change the folders while Big Picture shows them.
  connect(&Settings::Instance(), &Settings::PathAdded, m_big_picture_library,
          &BigPictureLibrary::RefreshFolders);
  connect(&Settings::Instance(), &Settings::PathRemoved, m_big_picture_library,
          &BigPictureLibrary::RefreshFolders);

  m_big_picture_host = new BigPictureHost(
      this, [this] { return ReadBigPictureState(); },
      [this](const BigPicture::Command& command) { return ExecuteBigPictureCommand(command); },
      [this](bool paused) {
        Core::SetState(m_system, paused ? Core::State::Paused : Core::State::Running);
        if (paused && Core::GetState(m_system) == Core::State::Paused)
        {
          // Finish accepted guest presents before the menu starts its retained-frame redraws.
          const Core::CPUThreadGuard guard(m_system);
        }
      },
      [this](std::shared_ptr<const BigPicture::Snapshot> snapshot) {
        BigPicture::GetPresentationState().Publish(snapshot);
        // A pending binding capture belongs to the visible settings page only. A null snapshot
        // comes from host destruction, after the settings object is gone.
        if (snapshot &&
            (snapshot->screens.empty() || snapshot->screens.back() != BigPicture::Screen::Settings))
          m_big_picture_settings->CancelCapture();
        if (m_big_picture_text_input &&
            (!snapshot || snapshot->generation != m_big_picture_text_generation ||
             snapshot->screens.empty() ||
             snapshot->screens.back() != BigPicture::Screen::TextInput))
          m_big_picture_text_input->reject();
        if (snapshot && !m_big_picture_text_input && !m_big_picture_text_queued &&
            !snapshot->screens.empty() &&
            snapshot->screens.back() == BigPicture::Screen::TextInput && snapshot->host.library &&
            snapshot->host.library->native_text_input)
        {
          m_big_picture_text_queued = true;
          QTimer::singleShot(0, this, [this, generation = snapshot->generation] {
            m_big_picture_text_queued = false;
            ShowBigPictureTextInput(generation);
          });
        }
        if (snapshot)
          m_render_widget->RefreshInputCapture();
        VideoCommon::BigPicture::Wakeup();
      });
  m_library_timer = new QTimer(this);
  m_library_timer->setSingleShot(true);
  m_library_timer->setInterval(50);
  connect(m_library_timer, &QTimer::timeout, this, &MainWindow::RefreshBigPictureLibrary);
  const auto* library_model = &m_game_list->GetGameListModel();
  const auto update_library = [this] { m_library_timer->start(); };
  connect(library_model, &QAbstractItemModel::rowsInserted, this, update_library);
  connect(library_model, &QAbstractItemModel::rowsRemoved, this, update_library);
  connect(library_model, &QAbstractItemModel::dataChanged, this, update_library);
  connect(library_model, &QAbstractItemModel::modelReset, this, update_library);
  connect(library_model, &GameListModel::DiscGroupsChanged, this, update_library);
  connect(&Settings::Instance(), &Settings::GameListRefreshStarted, this, [this] {
    if (!m_shutting_down)
      m_big_picture_library->SetScanning(true);
  });
  connect(&Settings::Instance(), &Settings::GameListRefreshCompleted, this, [this] {
    if (m_shutting_down)
      return;
    RefreshBigPictureLibrary();
    m_big_picture_library->SetScanning(false);
    m_library_timer->stop();
  });
  m_library_timer->start();
  connect(&Settings::Instance(), &Settings::ConfigChanged, m_big_picture_host,
          &BigPictureHost::Refresh);
  connect(&Settings::Instance(), &Settings::DevicesChanged, m_big_picture_host,
          &BigPictureHost::Refresh);
  m_standalone_timer = new QTimer(this);
  m_standalone_timer->setInterval(16);
  connect(m_standalone_timer, &QTimer::timeout, this, &MainWindow::PollStandalone);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    if (!m_standalone || m_standalone_restart_queued || m_standalone_stop != StandaloneStop::None ||
        !Core::IsUninitialized(m_system))
      return;
    if (!m_standalone_ready || BigPicture::ReadGraphicsConfig() == m_standalone_graphics_config)
      return;
    m_standalone_restart_queued = true;
    QTimer::singleShot(75, this, [this, serial = m_standalone_serial] {
      m_standalone_restart_queued = false;
      if (m_standalone && serial == m_standalone_serial && m_big_picture_requested &&
          Core::IsUninitialized(m_system) && m_standalone_stop == StandaloneStop::None)
        StopStandalone(StandaloneStop::Restart);
    });
  });

  ConnectGameList();
  ConnectHost();
  ConnectToolBar();
  ConnectRenderWidget();
  ConnectStack();
  ConnectMenuBar();

  QSettings& settings = Settings::GetQSettings();
  restoreState(settings.value(QStringLiteral("mainwindow/state")).toByteArray());
  const QString geometry_key = QStringLiteral("mainwindow/geometry");
  if (!settings.contains(geometry_key) ||
      !restoreGeometry(settings.value(geometry_key).toByteArray()))
    resize(sizeHint());
  if (!Settings::Instance().IsBatchModeEnabled() && !m_big_picture_requested)
  {
    show();
  }

  InitControllers();
  ConnectHotkeys();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme colorScheme) {
            Settings::Instance().ApplyStyle();
            if (m_skylander_window)
              m_skylander_window->RefreshList();
          });
#endif

#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_game_list, &GameList::OpenAchievementSettings, this,
          &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  InitCoreCallbacks();

  NetPlayInit();

#ifdef SHOW_UPDATER
  CheckForUpdatesAuto();
#endif  // SHOW_UPDATER

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementManager::GetInstance().Init(reinterpret_cast<void*>(winId()));
  // This needs to trigger on both RA_HARDCORE_ENABLED and RA_ENABLED
  m_config_changed_callback_id = Config::AddConfigChangedCallback(
      [this] { QueueOnObject(this, [this] { this->OnHardcoreChanged(); }); });
  // If hardcore is enabled when the emulator starts, make sure it turns off what it needs to
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
    OnHardcoreChanged();
#endif  // USE_RETRO_ACHIEVEMENTS

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
  auto* daemon = new SignalDaemon(this);

  connect(daemon, &SignalDaemon::InterruptReceived, this, &MainWindow::OnSignal);

  InstallSignalHandler();
#endif

  if (boot_parameters)
  {
    m_pending_boot = std::move(boot_parameters);

    if (!movie_path.empty())
    {
      std::optional<std::string> savestate_path;
      if (m_system.GetMovie().PlayInput(movie_path, &savestate_path))
      {
        m_pending_boot->boot_session_data.SetSavestateData(std::move(savestate_path),
                                                           DeleteSavestateAfterBoot::No);
        emit RecordingStatusChanged(true);
      }
    }
  }

  State::SetSelectedSlot(
      std::clamp(Settings::Instance().GetStateSlot(), 1, static_cast<int>(State::NUM_STATES)));

  m_render_widget_geometry = settings.value(QStringLiteral("renderwidget/geometry")).toByteArray();

  // Restoring of window states can sometimes go wrong, resulting in widgets being visible when they
  // shouldn't be so we have to reapply all our rules afterwards.
  Settings::Instance().RefreshWidgetVisibility();

  if (!ResourcePack::Init())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Error occurred while loading some texture packs"));
  }

  for (auto& pack : ResourcePack::GetPacks())
  {
    if (!pack.IsValid())
    {
      ModalMessageBox::critical(this, tr("Error"),
                                tr("Invalid Pack %1 provided: %2")
                                    .arg(QString::fromStdString(pack.GetPath()))
                                    .arg(QString::fromStdString(pack.GetError())));
      m_big_picture_requested = false;
      if (!Settings::Instance().IsBatchModeEnabled())
        show();
      return;
    }
  }

  Host::GetInstance()->SetMainWindowHandle(reinterpret_cast<void*>(winId()));

  if (m_pending_boot != nullptr)
  {
    StartGame(std::move(m_pending_boot));
    m_pending_boot.reset();
  }
  // A canceled/failed startup boot must still open the requested library.
  if (m_big_picture_requested && Core::IsUninitialized(m_system) && !m_standalone)
    QTimer::singleShot(0, this, &MainWindow::EnterBigPicture);
}

BigPicture::HostState MainWindow::ReadBigPictureState()
{
  BigPicture::HostState state;
  switch (Core::GetState(m_system))
  {
  case Core::State::Uninitialized:
    state.lifecycle = BigPicture::Lifecycle::Idle;
    break;
  case Core::State::Starting:
    state.lifecycle = BigPicture::Lifecycle::Booting;
    break;
  case Core::State::Running:
    state.lifecycle = BigPicture::Lifecycle::Running;
    break;
  case Core::State::Paused:
    state.lifecycle = BigPicture::Lifecycle::Paused;
    break;
  case Core::State::Stopping:
    state.lifecycle = BigPicture::Lifecycle::Stopping;
    break;
  }
  if (m_stop_requested || m_stop_confirm_showing)
    state.lifecycle = BigPicture::Lifecycle::Stopping;
  if (state.lifecycle == BigPicture::Lifecycle::Idle && m_standalone_boot)
    state.lifecycle = BigPicture::Lifecycle::Booting;
  state.netplay = NetPlay::IsNetPlayRunning();
  state.can_save_continuation = CanSaveContinuation();
  state.continuation_pending = m_continuation_boot != nullptr;
  state.continuation_title =
      QFileInfo(QString::fromStdString(m_continuation_boot_path)).completeBaseName().toStdString();
  state.can_load_continuation =
      ContinuationLoadingAllowed() && !m_system.GetMovie().IsMovieActive();
  state.hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
  UICommon::Settings::Environment environment;
  environment.emulating = state.lifecycle != BigPicture::Lifecycle::Idle;
  environment.transitioning = state.lifecycle == BigPicture::Lifecycle::Booting ||
                              state.lifecycle == BigPicture::Lifecycle::Stopping ||
                              m_standalone_stop != StandaloneStop::None ||
                              m_standalone_restart_queued;
  environment.netplay = state.netplay;
  environment.hardcore = state.hardcore;
  environment.post_processing = m_settings_post_processing;
  environment.movie = m_system.GetMovie().IsMovieActive();
  environment.max_efb_scale = Config::Get(Config::GFX_MAX_EFB_SCALE);
  environment.ssaa = g_backend_info.bSupportsSSAA;
  environment.hdr = g_backend_info.bSupportsHDROutput;
  environment.stereo = g_backend_info.bSupportsGeometryShaders;
  environment.gpu_texture_decoding = g_backend_info.bSupportsGPUTextureDecoding;
  environment.bounding_box = g_backend_info.bSupportsBBox;
  for (std::size_t i = 0; i < g_backend_info.Adapters.size(); ++i)
    environment.graphics_adapters.push_back({static_cast<int>(i), g_backend_info.Adapters[i]});
  for (const auto samples : g_backend_info.AAModes)
    environment.anti_aliasing.push_back(
        {samples, samples == 1 ? tr("None").toStdString() : std::to_string(samples) + "x"});
  if (environment.anti_aliasing.empty())
    environment.anti_aliasing.push_back({u32{1}, tr("None").toStdString()});
  // CPU engines, backend implementations and the interface language are fixed for this
  // process. Do not repeat optional library loading/probing on every settings snapshot;
  // devices are separate options.
  static const auto cpu_engines = [] {
    decltype(environment.cpu_engines) engines;
    for (const auto engine : PowerPC::AvailableCPUCores())
      engines.push_back(
          {UICommon::Settings::ToValue(engine),
           engine == PowerPC::CPUCore::Interpreter       ? tr("Interpreter").toStdString() :
           engine == PowerPC::CPUCore::CachedInterpreter ? tr("Cached Interpreter").toStdString() :
                                                           tr("JIT Recompiler").toStdString()});
    return engines;
  }();
  environment.cpu_engines = cpu_engines;
  static const auto graphics_backends = [] {
    decltype(environment.graphics_backends) backends;
    for (const auto& backend : VideoBackendBase::GetAvailableBackends())
      if (backend->SupportsUI())
        backends.push_back({backend->GetConfigName(), backend->GetDisplayName()});
    return backends;
  }();
  environment.graphics_backends = graphics_backends;
  static const auto audio_backends = AudioCommon::GetSoundBackends();
  for (const auto& backend : audio_backends)
    environment.audio_backends.push_back({backend, backend});
  static const auto languages = InterfaceSettings::GetLanguageChoices();
  for (const auto& [label, code] : languages)
    environment.languages.push_back({code.toStdString(), label.toStdString()});
#ifdef _WIN32
  if (Config::GetBase(Config::MAIN_AUDIO_BACKEND) == BACKEND_WASAPI)
  {
    environment.wasapi_devices.push_back(
        {Config::MAIN_WASAPI_DEVICE.GetDefaultValue(), tr("Default Device").toStdString()});
    static const auto devices = WASAPIStream::GetAvailableDevices();
    for (const auto& device : devices)
      environment.wasapi_devices.push_back({device, device});
  }
#endif
  m_big_picture_settings->SetEnvironment(std::move(environment));
  BigPictureSettings::Game settings_game;
  if (state.lifecycle == BigPicture::Lifecycle::Running ||
      state.lifecycle == BigPicture::Lifecycle::Paused)
  {
    const auto& config = SConfig::GetInstance();
    settings_game = {config.GetGameID(), config.GetRevision(), config.GetTitleName(), true};
  }
  else if (m_big_picture_properties_game)
  {
    // Game Properties edits a library game's settings until its options screen closes.
    const auto current = m_big_picture_host ? m_big_picture_host->GetSnapshot() : nullptr;
    if (state.lifecycle == BigPicture::Lifecycle::Idle && current &&
        std::ranges::find(current->screens, BigPicture::Screen::GameOptions) !=
            current->screens.end())
      settings_game = *m_big_picture_properties_game;
    else
      m_big_picture_properties_game.reset();
  }
  m_big_picture_settings->SetGame(std::move(settings_game));
  state.settings = m_big_picture_settings->ReadSnapshot();
  state.settings_menu = m_big_picture_settings->ReadMenu();
  state.pause_revision = m_system.GetCPU().GetPauseRevision();
  state.game_title = SConfig::GetInstance().GetTitleDescription();
  state.game_path = m_big_picture_disc_path;
  if (state.lifecycle == BigPicture::Lifecycle::Running ||
      state.lifecycle == BigPicture::Lifecycle::Paused)
  {
    if (!m_big_picture_play_time_started)
    {
      m_big_picture_previous_play_ms =
          TimePlayed().GetTimePlayed(SConfig::GetInstance().GetGameID()).count();
      m_big_picture_play_time_started = true;
    }
    if (state.lifecycle == BigPicture::Lifecycle::Running)
    {
      if (!m_big_picture_play_timer.isValid())
        m_big_picture_play_timer.start();
    }
    else if (m_big_picture_play_timer.isValid())
    {
      m_big_picture_session_ms += m_big_picture_play_timer.elapsed();
      m_big_picture_play_timer.invalidate();
    }
    const auto session_ms =
        m_big_picture_session_ms +
        (m_big_picture_play_timer.isValid() ? m_big_picture_play_timer.elapsed() : 0);
    const auto duration = [this](qint64 ms) {
      const auto seconds = ms / 1000;
      return tr("%1h %2m %3s").arg(seconds / 3600).arg(seconds / 60 % 60).arg(seconds % 60);
    };
    state.game_play_time = tr("This Session: %1").arg(duration(session_ms)).toStdString();
    if (Config::Get(Config::MAIN_TIME_TRACKING))
      state.game_play_time += "\n" + tr("All Time: %1")
                                         .arg(duration(m_big_picture_previous_play_ms + session_ms))
                                         .toStdString();
    state.game_details =
        QFileInfo(QString::fromStdString(state.game_path)).fileName().toStdString();
    if (!state.game_details.empty())
      state.game_details += "\n";
    state.game_details += SConfig::GetInstance().GetGameID() + " | " +
                          tr("Revision %1").arg(SConfig::GetInstance().GetRevision()).toStdString();
  }
  state.menu_theme = Config::GetBase(Config::MAIN_BIG_PICTURE_THEME);
#ifdef USE_RETRO_ACHIEVEMENTS
  {
    auto& achievements = AchievementManager::GetInstance();
    std::lock_guard lock(achievements.GetLock());
    if (Config::Get(Config::RA_ENABLED))
    {
      state.achievements_logged_in = achievements.HasAPIToken() && achievements.GetClient() &&
                                     rc_client_get_user_info(achievements.GetClient()) != nullptr;
      const auto current = m_big_picture_host ? m_big_picture_host->GetSnapshot() : nullptr;
      if (state.achievements_logged_in && current &&
          std::ranges::find(current->screens, BigPicture::Screen::Achievements) !=
              current->screens.end())
        state.achievements = m_big_picture_achievements.Read();
      state.achievements_username = achievements.GetPlayerDisplayName();
      if ((state.lifecycle == BigPicture::Lifecycle::Running ||
           state.lifecycle == BigPicture::Lifecycle::Paused) &&
          achievements.GetClient() && achievements.IsGameLoaded())
      {
        const auto presence = achievements.GetRichPresence();
        state.game_rich_presence.assign(presence.begin(), std::ranges::find(presence, '\0'));
        rc_client_user_game_summary_t summary{};
        rc_client_get_user_game_summary(achievements.GetClient(), &summary);
        if (summary.num_core_achievements != 0)
        {
          state.game_achievement_progress = tr("%1/%2 achievements | %3/%4 points")
                                                .arg(summary.num_unlocked_achievements)
                                                .arg(summary.num_core_achievements)
                                                .arg(summary.points_unlocked)
                                                .arg(summary.points_core)
                                                .toStdString();
          if (state.hardcore)
            state.game_achievement_progress += " | " + tr("Hardcore").toStdString();
        }
      }
      const auto& badge = achievements.GetPlayerBadge();
      if (!state.achievements_username.empty() && badge.width && badge.height &&
          badge.width <= 256 && badge.height <= 256 &&
          badge.data.size() == static_cast<std::size_t>(badge.width) * badge.height * 4)
      {
        // Reuse the published image while the badge is unchanged; state is read on every refresh.
        const auto& cached = m_big_picture_avatar;
        if (!cached || cached->width != badge.width || cached->height != badge.height ||
            !std::ranges::equal(cached->rgba, badge.data))
        {
          auto avatar = std::make_shared<BigPicture::LibraryImage>();
          avatar->width = badge.width;
          avatar->height = badge.height;
          avatar->rgba.assign(badge.data.begin(), badge.data.end());
          m_big_picture_avatar = std::move(avatar);
        }
        state.achievements_avatar = m_big_picture_avatar;
      }
    }
  }
#endif
  state.can_change_disc = m_big_picture_disc_game;
  state.state_slots_ready = m_big_picture_state_slots_ready;
  state.state_slots = m_big_picture_state_slots;
  state.library = m_big_picture_library ? m_big_picture_library->GetSnapshot() : nullptr;
  state.boot_ipl_available[0] = File::Exists(Config::GetBootROMPath(JAP_DIR));
  state.boot_ipl_available[1] = File::Exists(Config::GetBootROMPath(USA_DIR));
  state.boot_ipl_available[2] = File::Exists(Config::GetBootROMPath(EUR_DIR));
  state.boot_ipl_available[5] = File::Exists(Config::GetBootROMPath(DEV_DIR));
  state.boot_system_menu_available = m_big_picture_system_menu_available;
  if (state.lifecycle == BigPicture::Lifecycle::Idle)
  {
    if (const auto game = m_game_list->GetSelectedGame())
    {
      state.launch_path = game->GetFilePath();
      state.launch_title = game->GetLongName();
    }
    else
    {
      state.launch_path = Config::Get(Config::MAIN_DEFAULT_ISO);
      if (!File::Exists(state.launch_path))
        state.launch_path.clear();
      state.launch_title =
          QFileInfo(QString::fromStdString(state.launch_path)).fileName().toStdString();
    }
  }
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  state.stop_requires_desktop = AchievementManager::GetInstance().CheckForModifications();
#endif
  return state;
}

BigPicture::Result MainWindow::ExecuteBigPictureCommand(const BigPicture::Command& command)
{
  using BigPicture::Action;
  using BigPicture::ResultStatus;
  if (m_shutting_down)
    return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::Busy};
  switch (command.action)
  {
  case Action::Settings:
    if (!m_big_picture_settings->Open(command.slot == 1))
      return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::NoGame};
    return {command, ResultStatus::Succeeded};
  case Action::SettingsMenu:
  {
    if (!command.settings_menu)
      return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::InvalidArgument};
    const auto status = m_big_picture_settings->Execute(*command.settings_menu);
    using Common::Settings::Status;
    if (status == Status::InvalidValue)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("The selected value or profile is invalid. Refresh the page and try again.")
                  .toStdString()};
    return {command,
            status == Status::Success ? ResultStatus::Succeeded :
            status == Status::Stale   ? ResultStatus::Stale :
                                        ResultStatus::Unavailable,
            status == Status::Success ? BigPicture::UnavailableReason::None :
                                        BigPicture::UnavailableReason::InvalidArgument};
  }
  case Action::EditSetting:
  {
    if (!command.setting)
      return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::InvalidArgument};
    const auto result = m_big_picture_settings->Apply(*command.setting);
    using Common::Settings::Status;
    if (result.status == Status::Success)
      return {command, ResultStatus::Succeeded};
    if (result.status == Status::Stale)
      return {command, ResultStatus::Stale};
    return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::InvalidArgument};
  }
  case Action::Library:
  {
    using O = BigPicture::LibraryOperation;
    const bool idle_only = command.library_operation == O::AddFolder ||
                           command.library_operation == O::OpenFile ||
                           command.library_operation == O::ChooseBootDisc;
    if ((idle_only && !Core::IsUninitialized(m_system)) ||
        (command.library_operation == O::OpenDisc &&
         BigPicture::GetAvailability(Action::ChangeDisc, m_big_picture_host->GetSnapshot()->host) !=
             BigPicture::UnavailableReason::None))
      return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::Busy};
    if (command.library_operation == O::OpenDisc)
      m_big_picture_library->SetDiscContext(m_big_picture_disc_path,
                                            SConfig::GetInstance().GetGameID(),
                                            SConfig::GetInstance().GetTitleName(),
                                            StringListToStdVector(Settings::Instance().GetPaths()));
    const auto error = m_big_picture_library->Execute(command.library_operation, command.path,
                                                      command.library_revision);
    return {command, error.empty() ? ResultStatus::Succeeded : ResultStatus::Failed,
            BigPicture::UnavailableReason::None, error};
  }
  case Action::BootIPL:
  {
    const auto state = m_big_picture_host->GetSnapshot()->host;
    if (command.slot < 0 || command.slot >= static_cast<int>(state.boot_ipl_available.size()) ||
        !state.boot_ipl_available[command.slot])
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("The selected GameCube IPL is unavailable.").toStdString()};
    OnBootGameCubeIPL(static_cast<DiscIO::Region>(command.slot));
    if (Core::IsUninitialized(m_system) && !m_standalone_boot)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("Failed to init core").toStdString()};
    return {command, ResultStatus::Dispatched};
  }
  case Action::BootSystemMenu:
    if (!m_big_picture_system_menu_available)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("The Wii System Menu is not installed.").toStdString()};
    BootWiiSystemMenu();
    if (m_continuation_boot)
      return {command, ResultStatus::Succeeded};
    if (Core::IsUninitialized(m_system) && !m_standalone_boot)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("Failed to init core").toStdString()};
    return {command, ResultStatus::Dispatched};
  case Action::Boot:
  {
    if (!File::Exists(command.path))
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("The selected game is no longer available: %1")
                  .arg(QString::fromStdString(command.path))
                  .toStdString()};
    // Grouped launches are selected in Big Picture's controller-navigable picker before Boot.
    StartGame(command.path, ScanForSecondDisc::Yes);
    if (m_continuation_boot)
      return {command, ResultStatus::Succeeded};
    if (Core::IsUninitialized(m_system) && !m_standalone_boot)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("Failed to init core").toStdString()};
    break;
  }
  case Action::ContinueGame:
  case Action::FreshBoot:
  case Action::DeleteContinuation:
  {
    if (!m_continuation_boot)
      return {command, ResultStatus::Stale};
    if (command.action == Action::ContinueGame &&
        (!File::Exists(m_continuation_boot_path) || !ContinuationLoadingAllowed() ||
         m_system.GetMovie().IsMovieActive()))
      return {
          command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
          tr("The continuation state is unavailable or loading states is disabled.").toStdString()};
    if (command.action == Action::DeleteContinuation &&
        !State::DeleteContinuation(m_continuation_boot_path))
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("Could not delete the continuation state.").toStdString()};
    if (command.action == Action::ContinueGame)
      m_continuation_boot->boot_session_data.SetSavestateData(m_continuation_boot_path,
                                                              DeleteSavestateAfterBoot::No);
    auto boot = std::move(m_continuation_boot);
    StartGame(std::move(boot), true);
    return {command, Core::IsUninitialized(m_system) && !m_standalone_boot ?
                         ResultStatus::Failed :
                         ResultStatus::Dispatched};
  }
  case Action::Back:
  case Action::Cancel:
    ClearContinuationBoot();
    return {command, ResultStatus::Cancelled};
  case Action::Resume:
    Core::SetState(m_system, Core::State::Running);
    return {command, Core::GetState(m_system) == Core::State::Running ? ResultStatus::Succeeded :
                                                                        ResultStatus::Failed};
  case Action::Stop:
  case Action::Exit:
    if (command.action == Action::Exit)
    {
      m_exit_requested = true;
      if (Core::IsUninitialized(m_system))
      {
        StopStandalone(StandaloneStop::Exit);
        return {command, ResultStatus::Dispatched};
      }
    }
    if (!RequestStopWithConfirmation(true, command.slot == 1))
    {
      if (command.action == Action::Exit)
        m_exit_requested = false;
      return {command, command.slot == 1 ? ResultStatus::Failed : ResultStatus::Cancelled,
              BigPicture::UnavailableReason::None,
              tr("Could not create the continuation state. The game is still open.").toStdString()};
    }
    break;
  case Action::Reset:
    Reset();
    break;
  case Action::SaveState:
    if (State::GetSlotInfo(command.slot).exists && command.confirmation_id != command.id)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("This slot changed. Refresh the slots and confirm the overwrite.").toStdString()};
    BeginBigPictureStateRequest(command.id, true, command.slot);
    StateSaveSlotAt(command.slot);
    break;
  case Action::LoadState:
    if (!State::GetSlotInfo(command.slot).readable)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::EmptySlot,
              tr("This slot is empty or unreadable. Refresh the slots and choose another.")
                  .toStdString()};
    BeginBigPictureStateRequest(command.id, false, command.slot);
    StateLoadSlotAt(command.slot);
    break;
  case Action::RefreshStates:
    RefreshBigPictureStateSlots();
    return {command, ResultStatus::Succeeded};
  case Action::GameProperties:
  case Action::GameStates:
  case Action::BootState:
  case Action::ResetPlayTime:
  {
    const auto game = m_game_list->FindGame(command.path);
    if (!game)
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("The selected game is no longer available: %1")
                  .arg(QString::fromStdString(command.path))
                  .toStdString()};
    const auto& game_id = game->GetGameID();
    if (command.action == Action::GameStates)
    {
      RequestBigPictureStateSlots(game_id);
      return {command, ResultStatus::Succeeded};
    }
    if (command.action == Action::BootState)
    {
      if (!State::GetSlotInfo(game_id, static_cast<u32>(command.slot)).readable)
        return {command, ResultStatus::Failed, BigPicture::UnavailableReason::EmptySlot,
                tr("This slot is empty or unreadable. Refresh the slots and choose another.")
                    .toStdString()};
      if (!ContinuationLoadingAllowed() || m_system.GetMovie().IsMovieActive())
        return {
            command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
            tr("Loading save states is unavailable in Hardcore mode or NetPlay.").toStdString()};
      StartGame(command.path, ScanForSecondDisc::Yes,
                std::make_unique<BootSessionData>(
                    State::GetSlotPath(game_id, static_cast<u32>(command.slot)),
                    DeleteSavestateAfterBoot::No));
      if (Core::IsUninitialized(m_system) && !m_standalone_boot)
        return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
                tr("Failed to init core").toStdString()};
      return {command, ResultStatus::Dispatched};
    }
    if (game_id.empty())
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("This game has no game ID, so it has no per-game settings or play time.")
                  .toStdString()};
    if (command.action == Action::ResetPlayTime)
    {
      m_game_list->ResetTimePlayed(game_id);
      return {command, ResultStatus::Succeeded};
    }
    m_big_picture_properties_game =
        BigPictureSettings::Game{game_id, game->GetRevision(), game->GetLongName(), false};
    m_big_picture_settings->SetGame(*m_big_picture_properties_game);
    if (!m_big_picture_settings->Open(true))
    {
      m_big_picture_properties_game.reset();
      return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
              tr("Settings for this game cannot be stored per game.").toStdString()};
    }
    return {command, ResultStatus::Succeeded};
  }
  case Action::Screenshot:
    ScreenShot();
    break;
  case Action::DesktopSettings:
    QTimer::singleShot(0, this, [this, generation = command.generation] {
      if (m_big_picture_host->GetSnapshot()->generation != generation)
        return;
      if (m_standalone)
      {
        StopStandalone(StandaloneStop::Settings);
        return;
      }
      if (LeaveBigPictureForDesktop(generation))
        ShowSettingsWindow();
    });
    break;
  case Action::ChangeDisc:
  {
    if (!command.path.empty())
    {
      if (!File::Exists(command.path))
        return {command, ResultStatus::Failed, BigPicture::UnavailableReason::None,
                tr("The selected disc is no longer available.").toStdString()};
      m_system.GetDVDInterface().ChangeDisc(Core::CPUThreadGuard{m_system}, {command.path});
      m_big_picture_disc_path = command.path;
      return {command, ResultStatus::Dispatched};
    }
    // The Big Picture picker opens through Library/OpenDisc. A disc-change
    // execution must carry that picker's selected path, never a desktop handoff.
    return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::InvalidArgument};
  }
  case Action::OpenGame:
    QTimer::singleShot(0, this, [this, generation = command.generation] {
      const auto snapshot = m_big_picture_host->GetSnapshot();
      if (snapshot->generation == generation && m_standalone &&
          m_standalone_stop == StandaloneStop::None &&
          BigPicture::GetAvailability(Action::OpenGame, snapshot->host) ==
              BigPicture::UnavailableReason::None)
        StopStandalone(StandaloneStop::OpenGame);
    });
    break;
  case Action::Fullscreen:
    QTimer::singleShot(0, this, [this, generation = command.generation] {
      const auto lifecycle = m_big_picture_host->GetSnapshot()->host.lifecycle;
      const bool active =
          lifecycle == BigPicture::Lifecycle::Running || lifecycle == BigPicture::Lifecycle::Paused;
      if (!m_shutting_down && m_big_picture_host->GetSnapshot()->generation == generation &&
          ((m_standalone && m_standalone_ready && m_standalone_stop == StandaloneStop::None) ||
           active))
        FullScreen();
    });
    return {command, ResultStatus::Succeeded};
  default:
    return {command, ResultStatus::Unavailable, BigPicture::UnavailableReason::InvalidArgument};
  }
  return {command, ResultStatus::Dispatched};
}

MainWindow::~MainWindow()
{
  m_shutting_down = true;
  m_state_save_hook.reset();
  m_state_result_hook.reset();
  m_state_slot_worker.StopAndCancel();
  m_library_timer->stop();
  delete m_big_picture_library;
  m_big_picture_library = nullptr;
  if (m_standalone)
  {
    m_standalone_timer->stop();
    m_standalone->Stop();
    // Normal closes finish asynchronously before reaching here. External application quits
    // must also keep pumping native messages until the graphics owner has released the window.
    while (!m_standalone->IsFinished())
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
      QThread::msleep(1);
    }
    m_standalone.reset();
  }
  // Shut down NetPlay first to avoid race condition segfault
  Settings::Instance().ResetNetPlayClient();
  Settings::Instance().ResetNetPlayServer();

#ifdef USE_RETRO_ACHIEVEMENTS
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
  AchievementManager::GetInstance().Shutdown();
#endif  // USE_RETRO_ACHIEVEMENTS

  delete m_render_widget;
  delete m_netplay_dialog;

  for (int i = 0; i < 4; i++)
  {
    delete m_gc_tas_input_windows[i];
    delete m_gba_tas_input_windows[i];
    delete m_wii_tas_input_windows[i];
  }

  ShutdownControllers();

  QSettings& settings = Settings::GetQSettings();

  if (!Settings::Instance().IsBatchModeEnabled())
  {
    settings.setValue(QStringLiteral("mainwindow/state"), saveState());
    settings.setValue(QStringLiteral("mainwindow/geometry"), saveGeometry());
  }

  settings.setValue(QStringLiteral("renderwidget/geometry"), m_render_widget_geometry);

  Config::Save();
}

WindowSystemInfo MainWindow::GetWindowSystemInfo() const
{
  auto wsi = ::GetWindowSystemInfo(m_render_widget->GetRenderSurfaceWindow());
  // Input/focus belong to the persistent presentation widget, while graphics APIs receive
  // a replaceable native child with its own pixel format/surface lifetime.
  wsi.render_window = ::GetWindowSystemInfo(m_render_widget->windowHandle()).render_window;
  return wsi;
}

void MainWindow::InitControllers()
{
  if (g_controller_interface.IsInit())
    return;

  UICommon::InitControllers(::GetWindowSystemInfo(windowHandle()));
  BigPicture::GetPresentationState().SetMenuFontPaths(QtUtils::FindMenuFontFallbacks(
      QStandardPaths::standardLocations(QStandardPaths::FontsLocation)));

  BigPicture::NavigationText menu_text{
      tr("Big Picture").toStdString(),
      tr("Menu Preferences").toStdString(),
      tr("Close Menu").toStdString(),
      tr("Swap Confirm and Back").toStdString(),
      tr("Interface Scale").toStdString(),
      tr("Safe Area").toStdString(),
      tr("Back").toStdString(),
      tr("Select").toStdString(),
      tr("Previous Page").toStdString(),
      tr("Next Page").toStdString(),
      tr("On").toStdString(),
      tr("Off").toStdString(),
      tr("Enter: Select   Esc: Back   Page Up/Down: Previous/Next Page").toStdString()};
  menu_text.actions = {tr("Open Menu").toStdString(),
                       tr("Back").toStdString(),
                       tr("Settings").toStdString(),
                       tr("Save State").toStdString(),
                       tr("Load State").toStdString(),
                       tr("Return to Desktop").toStdString(),
                       tr("Start Game").toStdString(),
                       tr("Resume Game").toStdString(),
                       tr("Close Game").toStdString(),
                       tr("Reset").toStdString(),
                       tr("Save State").toStdString(),
                       tr("Load State").toStdString(),
                       tr("Confirm").toStdString(),
                       tr("Cancel").toStdString(),
                       tr("Save Screenshot").toStdString(),
                       tr("Change Disc").toStdString(),
                       tr("Advanced Settings (Desktop)").toStdString(),
                       tr("Refresh Slots").toStdString(),
                       tr("Exit Dolphin").toStdString(),
                       tr("Open Game (Desktop)").toStdString(),
                       tr("Toggle Fullscreen").toStdString()};
  menu_text.reasons = {
      std::string{},
      tr("An operation is in progress.").toStdString(),
      tr("No game is running.").toStdString(),
      tr("A game is already running.").toStdString(),
      tr("Unavailable during NetPlay.").toStdString(),
      tr("Unavailable in hardcore mode.").toStdString(),
      tr("Unavailable here.").toStdString(),
      tr("Available when running a disc game.").toStdString(),
      tr("Return to the desktop to review unsaved achievement modifications.").toStdString(),
      tr("This slot is empty or unreadable.").toStdString()};
  for (std::size_t slot = 0; slot < menu_text.slot_labels.size(); ++slot)
    menu_text.slot_labels[slot] = tr("Slot %1").arg(slot + 1).toStdString();
  menu_text.confirm = tr("Confirm").toStdString();
  menu_text.cancel = tr("Cancel").toStdString();
  menu_text.confirm_stop =
      tr("Stop the current emulation? Unsaved progress will be lost.").toStdString();
  menu_text.confirm_reset =
      tr("Reset the current game? Unsaved progress will be lost.").toStdString();
  menu_text.confirm_exit =
      tr("Stop emulation and exit Dolphin? Unsaved progress will be lost.").toStdString();
  menu_text.confirm_overwrite = tr("Overwrite this save-state slot?").toStdString();
  menu_text.error = tr("Operation Failed").toStdString();
  menu_text.requested = tr("Request sent. Watch the on-screen message for the result. Screenshots "
                           "finish on the next game frame.")
                            .toStdString();
  menu_text.loading_slots =
      tr("Open or refresh this browser to read slot information.").toStdString();
  menu_text.empty_slot = tr("Empty").toStdString();
  menu_text.unknown_slot = tr("Unreadable state header").toStdString();
  menu_text.state_browser_help = tr("Choose a slot. Refresh Slots updates saved dates. Older or "
                                    "incompatible states may not load.")
                                     .toStdString();
  menu_text.desktop_settings_help = tr("Open desktop Configuration.").toStdString();
  menu_text.desktop_disc_help = tr("Choose a disc using the desktop file picker.").toStdString();
  menu_text.stop_help = tr("Return to the desktop game list.").toStdString();
  menu_text.standalone_help =
      tr("Open a game using the desktop picker, or return to the desktop game library.")
          .toStdString();
  menu_text.confirm_exit_idle = tr("Exit Dolphin?").toStdString();
  using LibraryOp = BigPicture::LibraryOperation;
  const auto library_label = [&](LibraryOp op, const QString& label) {
    menu_text.library_actions[static_cast<std::size_t>(op)] = label.toStdString();
  };
  library_label(LibraryOp::Options, tr("Library Options"));
  library_label(LibraryOp::Search, tr("Search"));
  library_label(LibraryOp::AddFolder, tr("Add Game Folder"));
  library_label(LibraryOp::OpenFile, tr("Open Game"));
  library_label(LibraryOp::OpenDisc, tr("Change Disc"));
  library_label(LibraryOp::ChooseBootDisc, tr("Select Disc"));
  library_label(LibraryOp::Refresh, tr("Refresh"));
  library_label(LibraryOp::View, tr("Grid / List"));
  library_label(LibraryOp::Sort, tr("Sort"));
  library_label(LibraryOp::Platform, tr("Platform"));
  library_label(LibraryOp::Region, tr("Region"));
  library_label(LibraryOp::Tag, tr("Tag"));
  library_label(LibraryOp::ClearFilters, tr("Clear Filters"));
  library_label(LibraryOp::EditPath, tr("Enter Path"));
  library_label(LibraryOp::TextDone, tr("Done"));
  library_label(LibraryOp::TextErase, tr("Erase"));
  library_label(LibraryOp::Recursive, tr("Scan Subfolders"));
  library_label(LibraryOp::ClearSearch, tr("Clear Search"));
  library_label(LibraryOp::TextClear, tr("Clear"));
  library_label(LibraryOp::BrowseDiscs, tr("Browse Files"));
  library_label(LibraryOp::Folders, tr("Game Folders"));
  library_label(LibraryOp::RemoveFolder, tr("Remove Folder"));
  menu_text.library_folders_help =
      tr("Select a folder to remove it from the library. Its files are not deleted.").toStdString();
  menu_text.library_no_folders =
      tr("No game folders. Add one to build your library.").toStdString();
  menu_text.confirm_remove_folder =
      tr("Remove this folder from the library? Its games leave the library; no files are "
         "deleted.")
          .toStdString();
  menu_text.library_disc_games = tr("Library games — matching titles first").toStdString();
  menu_text.library_empty = tr("Add a game folder to build your library.").toStdString();
  menu_text.library_no_matches =
      tr("No games match these filters. Clear filters in Library Options.").toStdString();
  menu_text.library_scanning = tr("Loading…").toStdString();
  menu_text.library_choose_folder = tr("Use This Folder").toStdString();
  menu_text.library_parent = tr("Parent Folder").toStdString();
  menu_text.library_roots = tr("Locations").toStdString();
  menu_text.library_space = tr("Space").toStdString();
  menu_text.library_keyboard = tr("Enter Text").toStdString();
  menu_text.library_keyboard_prompts =
      tr("Type text   Enter: Done   Backspace: Erase   Esc: Cancel").toStdString();
  menu_text.library_cancel = tr("Cancel").toStdString();
  menu_text.library_grid = tr("Grid").toStdString();
  menu_text.library_list = tr("List").toStdString();
  menu_text.library_all = tr("All").toStdString();
  menu_text.disc_change_requested =
      tr("Disc change requested. Resume emulation to complete insertion.").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::BootIPL)] =
      tr("GameCube IPL").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::BootSystemMenu)] =
      tr("Wii System Menu").toStdString();
  menu_text.home_labels = {tr("Game List").toStdString(), tr("Start Game").toStdString(),
                           tr("Settings").toStdString(), tr("Exit").toStdString()};
  menu_text.home_descriptions = {
      tr("Launch a game from images scanned from your game directories.").toStdString(),
      tr("Launch a game from a file, or start the GameCube or Wii system menu.").toStdString(),
      tr("Changes settings for the application.").toStdString(),
      tr("Return to desktop mode, or exit the application.").toStdString()};
  // PromptFont's keycaps, matching the fullscreen reference. Keep labels independently translated.
  menu_text.home_keyboard_prompts = "\xe2\x91\xa0 " + tr("About").toStdString() +
                                    "   \xe2\x91\xaa " + tr("Toggle Fullscreen").toStdString() +
                                    "   \xe2\x8f\xb4\xe2\x8f\xb6 " + tr("Navigate").toStdString() +
                                    "   \xe2\x90\xba " + tr("Game List").toStdString() +
                                    "   \xe2\x90\xae " + tr("Select").toStdString() +
                                    "   \xe2\x90\xaf " + tr("Exit").toStdString();
  menu_text.game_grid = tr("Game Grid").toStdString();
  menu_text.game_list = tr("Game List").toStdString();
  menu_text.library_change_view = tr("Change View").toStdString();
  menu_text.library_select_game = tr("Select Game").toStdString();
  menu_text.start_file = tr("Start File").toStdString();
  menu_text.start_file_description =
      tr("Launch a game by selecting a file or disc image.").toStdString();
  menu_text.start_ipl_description = tr("Start the GameCube without a disc inserted.").toStdString();
  menu_text.start_wii_description = tr("Start the installed Wii System Menu.").toStdString();
  menu_text.picker_title = tr("Select Game File").toStdString();
  menu_text.settings_change_page = tr("Change Page").toStdString();
  menu_text.global_settings = tr("Global Settings").toStdString();
  menu_text.game_settings = tr("Game Settings").toStdString();
  menu_text.settings_options = tr("Setting Options").toStdString();
  menu_text.settings_navigation = tr("Menu Navigation").toStdString();
  menu_text.settings_navigation_help = {
      tr("Swap the controller buttons used to select and go back.").toStdString(),
      tr("Adjust the size of Big Picture menus.").toStdString(),
      tr("Keep menu content inside the visible area of the display.").toStdString()};
  menu_text.settings_keyboard_hints =
      "\xe2\x8f\xb4\xe2\x8f\xb6 " + tr("Change Page").toStdString() +
      "   \xe2\x8f\xb5\xe2\x8f\xb7 " + tr("Navigate").toStdString() + "   \xe2\x90\xae " +
      tr("Select").toStdString() + "   \xe2\x90\xaf " + tr("Back").toStdString();
  menu_text.settings_keyboard_options = "\xe2\x91\xa1 " + tr("Setting Options").toStdString();
  menu_text.settings_adjust = tr("Adjust Value").toStdString();
  menu_text.settings_adjust_keyboard_hints =
      "\xe2\x8f\xb4\xe2\x8f\xb6 " + menu_text.settings_adjust + "   \xe2\x8f\xb5\xe2\x8f\xb7 " +
      tr("Navigate").toStdString() + "   \xe2\x90\xae " + tr("Select").toStdString() +
      "   \xe2\x90\xaf " + tr("Back").toStdString();
  menu_text.start_keyboard_hints = "\xe2\x8f\xb4\xe2\x8f\xb6 " + tr("Navigate").toStdString() +
                                   "   \xe2\x90\xae " + tr("Select").toStdString() +
                                   "   \xe2\x90\xaf " + tr("Back").toStdString();
  menu_text.picker_keyboard_hints =
      "\xe2\x8f\xb5\xe2\x8f\xb7 " + tr("Change Selection").toStdString() + "   \xe2\x90\xba " +
      tr("Parent Folder").toStdString() + "   \xe2\x90\xae " + tr("Select").toStdString() +
      "   \xe2\x90\xaf " + tr("Cancel").toStdString();
  menu_text.library_keyboard_hints =
      "\xe2\x8f\xb5\xe2\x8f\xb7\xe2\x8f\xb4\xe2\x8f\xb6 " + menu_text.library_select_game +
      "   \xe2\x91\xa0 " + menu_text.library_change_view + "   \xe2\x91\xa1 " +
      tr("Settings").toStdString() + "   \xe2\x91\xa2 " + tr("Library Options").toStdString() +
      "   \xe2\x91\xa3 " + tr("Search").toStdString() + "   \xe2\x91\xa4 " +
      tr("Refresh List").toStdString() + "   \xe2\x91\xa5 " + tr("Game Options").toStdString() +
      "   \xe2\x90\xae " + tr("Start Game").toStdString() + "   \xe2\x90\xaf " +
      tr("Back").toStdString();
  menu_text.navigate = tr("Navigate").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::Achievements)] =
      tr("Achievements").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::ContinueGame)] =
      tr("Continue from Save State").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::FreshBoot)] =
      tr("Start Fresh").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::DeleteContinuation)] =
      tr("Delete Continuation and Start Fresh").toStdString();
  menu_text.continuation_title = tr("Continue Game?").toStdString();
  menu_text.continuation_description =
      tr("A continuation save state is available. Start Fresh keeps it for later.").toStdString();
  menu_text.exit_and_save = tr("Exit and Save State").toStdString();
  menu_text.exit_without_saving = tr("Exit Without Saving").toStdString();
  menu_text.game_options = tr("Game Options").toStdString();
  menu_text.state_saved = tr("Saved").toStdString();
  menu_text.state_loaded = tr("Loaded").toStdString();
  menu_text.confirm_reset_play_time =
      tr("Reset the play time for this game? This cannot be undone.").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::GameOptions)] =
      menu_text.game_options;
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::GameProperties)] =
      tr("Game Properties").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::GameStates)] =
      tr("Load State").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::BootState)] =
      tr("Load State").toStdString();
  menu_text.actions[static_cast<std::size_t>(BigPicture::Action::ResetPlayTime)] =
      tr("Reset Play Time").toStdString();
  menu_text.reasons[static_cast<std::size_t>(BigPicture::UnavailableReason::LoginRequired)] =
      tr("Log in to RetroAchievements to view achievements.").toStdString();
  menu_text.achievements_empty = tr("No achievements are available for this game.").toStdString();
  menu_text.achievements_loading = tr("Loading achievements...").toStdString();
  menu_text.achievements_subsets = tr("Change Subset").toStdString();
  menu_text.achievements_keyboard_hints =
      "\xe2\x8f\xb5\xe2\x8f\xb7 " + tr("Navigate").toStdString() + "   PgUp/PgDn " +
      menu_text.achievements_subsets + "   \xe2\x90\xae " + tr("Select").toStdString() +
      "   \xe2\x90\xaf " + tr("Back").toStdString();
  menu_text.return_to_game = tr("Return to Game").toStdString();
  menu_text.pause_keyboard_prompts =
      "\xe2\x8f\xb5\xe2\x8f\xb7 " + tr("Change Selection").toStdString() + "   \xe2\x90\xae " +
      tr("Select").toStdString() + "   \xe2\x90\xaf " + menu_text.return_to_game;
  menu_text.state_keyboard_hints =
      "\xe2\x8f\xb4\xe2\x8f\xb5\xe2\x8f\xb6\xe2\x8f\xb7 " + tr("Select State").toStdString() +
      "   F1 " + tr("Refresh Slots").toStdString() + "   \xe2\x90\xae " +
      tr("Load/Save State").toStdString() + "   \xe2\x90\xaf " + tr("Cancel").toStdString();
  menu_text.about_title = tr("About Dolphin").toStdString();
  menu_text.about_description = Common::GetScmRevStr() + "\n\n" +
                                tr("Dolphin is a GameCube and Wii emulator.").toStdString() +
                                "\n\nhttps://dolphin-emu.org";
  BigPicture::GetPresentationState().SetNavigationText(std::move(menu_text));

  m_hotkey_scheduler = new HotkeyScheduler();
  m_hotkey_scheduler->Start();

  // Defaults won't work reliably without loading and saving the config first

  Wiimote::LoadConfig();
  Wiimote::GetConfig()->SaveConfig();

  Pad::LoadConfig();
  Pad::GetConfig()->SaveConfig();

  Pad::LoadGBAConfig();
  Pad::GetGBAConfig()->SaveConfig();

  Keyboard::LoadConfig();
  Keyboard::GetConfig()->SaveConfig();

  FreeLook::LoadInputConfig();
  FreeLook::GetInputConfig()->SaveConfig();
}

void MainWindow::ShutdownControllers()
{
  m_hotkey_scheduler->Stop();

  Settings::Instance().UnregisterDevicesChangedCallback();

  UICommon::ShutdownControllers();

  m_hotkey_scheduler->deleteLater();
}

void MainWindow::InitCoreCallbacks()
{
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    // Backend information belongs to graphics initialization. Sample capabilities
    // after the owner publishes readiness, never while it is still constructing them.
    if (state == Core::State::Running || state == Core::State::Paused)
      m_settings_post_processing = g_backend_info.bSupportsPostProcessing;
    m_big_picture_host->Refresh();
    if (state == Core::State::Uninitialized)
      OnStopComplete();

    if (state == Core::State::Running && m_fullscreen_requested)
    {
      FullScreen();
      m_fullscreen_requested = false;
    }
  });
  installEventFilter(this);
  m_render_widget->installEventFilter(this);

  // Handle file open events
  auto* filter = new FileOpenEventFilter(QGuiApplication::instance());
  connect(filter, &FileOpenEventFilter::fileOpened, this, [this](const QString& file_name) {
    StartGame(BootParameters::GenerateFromFile(file_name.toStdString()));
  });
}

static void InstallHotkeyFilter(QWidget* dialog)
{
  auto* filter = new WindowActivationEventFilter(dialog);
  dialog->installEventFilter(filter);

  filter->connect(filter, &WindowActivationEventFilter::windowDeactivated,
                  [] { HotkeyManagerEmu::Enable(true); });
  filter->connect(filter, &WindowActivationEventFilter::windowActivated,
                  [] { HotkeyManagerEmu::Enable(false); });
}

void MainWindow::CreateComponents()
{
  m_menu_bar = new MenuBar(*m_ui, this);
  m_tool_bar = new ToolBar(*m_ui, this);
  m_game_list = new GameList(m_ui->gameListPage);
  m_emulation_status = new EmulationStatusWidget(m_ui->statusBar);
  m_ui->statusBar->addPermanentWidget(m_emulation_status);
  m_render_widget = new RenderWidget;
  m_stack = m_ui->mainStack;

  for (int i = 0; i < 4; i++)
  {
    m_gc_tas_input_windows[i] = new GCTASInputWindow(nullptr, i);
    m_gba_tas_input_windows[i] = new GBATASInputWindow(nullptr, i);
    m_wii_tas_input_windows[i] = new WiiTASInputWindow(nullptr, i);
  }

  m_jit_widget = new JITWidget(m_system, this);
  m_log_widget = new LogWidget(this);
  m_log_config_widget = new LogConfigWidget(this);
  m_memory_widget = new MemoryWidget(m_system, this);
  m_network_widget = new NetworkWidget(this);
  m_register_widget = new RegisterWidget(this);
  m_thread_widget = new ThreadWidget(this);
  m_watch_widget = new WatchWidget(this);
  m_breakpoint_widget = new BreakpointWidget(this);
  m_code_widget = new CodeWidget(this);
  m_assembler_widget = new AssemblerWidget(this);

  const auto request_watch = [this](const QString& name, u32 addr) {
    m_watch_widget->AddWatch(name, addr);
  };
  const auto request_breakpoint = [this](u32 addr) { m_breakpoint_widget->AddBP(addr); };
  const auto request_memory_breakpoint = [this](u32 addr) {
    m_breakpoint_widget->AddAddressMBP(addr);
  };
  const auto request_view_in_memory = [this](u32 addr) { m_memory_widget->SetAddress(addr); };
  const auto request_view_in_code = [this](u32 addr) {
    m_code_widget->SetAddress(addr, CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
  };

  connect(m_jit_widget, &JITWidget::SetCodeAddress, m_code_widget, &CodeWidget::OnSetCodeAddress);
  connect(m_watch_widget, &WatchWidget::RequestMemoryBreakpoint, request_memory_breakpoint);
  connect(m_watch_widget, &WatchWidget::ShowMemory, m_memory_widget, &MemoryWidget::SetAddress);
  connect(m_register_widget, &RegisterWidget::RequestMemoryBreakpoint, request_memory_breakpoint);
  connect(m_register_widget, &RegisterWidget::RequestWatch, request_watch);
  connect(m_register_widget, &RegisterWidget::RequestViewInMemory, request_view_in_memory);
  connect(m_register_widget, &RegisterWidget::RequestViewInCode, request_view_in_code);
  connect(m_thread_widget, &ThreadWidget::RequestBreakpoint, request_breakpoint);
  connect(m_thread_widget, &ThreadWidget::RequestMemoryBreakpoint, request_memory_breakpoint);
  connect(m_thread_widget, &ThreadWidget::RequestWatch, request_watch);
  connect(m_thread_widget, &ThreadWidget::RequestViewInMemory, request_view_in_memory);
  connect(m_thread_widget, &ThreadWidget::RequestViewInCode, request_view_in_code);

  connect(m_code_widget, &CodeWidget::RequestPPCComparison, m_jit_widget,
          &JITWidget::OnRequestPPCComparison);
  connect(m_code_widget, &CodeWidget::ShowMemory, m_memory_widget, &MemoryWidget::SetAddress);
  connect(m_memory_widget, &MemoryWidget::ShowCode, m_code_widget, [this](u32 address) {
    m_code_widget->SetAddress(address, CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
  });
  connect(m_memory_widget, &MemoryWidget::RequestWatch, request_watch);

  connect(m_breakpoint_widget, &BreakpointWidget::ShowCode, [this](u32 address) {
    if (Core::GetState(m_system) == Core::State::Paused)
      m_code_widget->SetAddress(address, CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
  });
  connect(m_breakpoint_widget, &BreakpointWidget::ShowMemory, m_memory_widget,
          &MemoryWidget::SetAddress);
}

void MainWindow::ConnectMenuBar()
{
  // File
  connect(m_menu_bar, &MenuBar::Open, this, &MainWindow::Open);
  connect(m_menu_bar, &MenuBar::Exit, this, &MainWindow::close);
  connect(m_menu_bar, &MenuBar::EjectDisc, this, &MainWindow::EjectDisc);
  connect(m_menu_bar, &MenuBar::ChangeDisc, this, &MainWindow::ChangeDisc);
  connect(m_menu_bar, &MenuBar::OpenUserFolder, this, &MainWindow::OpenUserFolder);
  connect(m_menu_bar, &MenuBar::OpenConfigFolder, this, &MainWindow::OpenConfigFolder);
  connect(m_menu_bar, &MenuBar::OpenCacheFolder, this, &MainWindow::OpenCacheFolder);

  // Emulation
  connect(m_menu_bar, &MenuBar::Pause, this, &MainWindow::Pause);
  connect(m_menu_bar, &MenuBar::Play, this, [this] { Play(); });
  connect(m_menu_bar, &MenuBar::Stop, this, &MainWindow::RequestStop);
  connect(m_menu_bar, &MenuBar::Reset, this, &MainWindow::Reset);
  connect(m_menu_bar, &MenuBar::Fullscreen, this, &MainWindow::FullScreen);
  connect(m_menu_bar, &MenuBar::FrameAdvance, this, &MainWindow::FrameAdvance);
  connect(m_menu_bar, &MenuBar::Screenshot, this, &MainWindow::ScreenShot);
  connect(m_menu_bar, &MenuBar::StateLoad, this, &MainWindow::StateLoad);
  connect(m_menu_bar, &MenuBar::StateSave, this, &MainWindow::StateSave);
  connect(m_menu_bar, &MenuBar::StateLoadSlot, this, &MainWindow::StateLoadSlot);
  connect(m_menu_bar, &MenuBar::StateSaveSlot, this, &MainWindow::StateSaveSlot);
  connect(m_menu_bar, &MenuBar::StateLoadSlotAt, this, &MainWindow::StateLoadSlotAt);
  connect(m_menu_bar, &MenuBar::StateSaveSlotAt, this, &MainWindow::StateSaveSlotAt);
  connect(m_menu_bar, &MenuBar::StateLoadUndo, this, &MainWindow::StateLoadUndo);
  connect(m_menu_bar, &MenuBar::StateSaveUndo, this, &MainWindow::StateSaveUndo);
  connect(m_menu_bar, &MenuBar::StateSaveOldest, this, &MainWindow::StateSaveOldest);
  connect(m_menu_bar, &MenuBar::SetStateSlot, this, &MainWindow::SetStateSlot);

  // Options
  connect(m_menu_bar, &MenuBar::Configure, this, &MainWindow::ShowSettingsWindow);
  connect(m_menu_bar, &MenuBar::ConfigureGraphics, this, &MainWindow::ShowGraphicsWindow);
  connect(m_menu_bar, &MenuBar::ConfigureAudio, this, &MainWindow::ShowAudioWindow);
  connect(m_menu_bar, &MenuBar::ConfigureControllers, this, &MainWindow::ShowControllersWindow);
  connect(m_menu_bar, &MenuBar::ConfigureHotkeys, this, &MainWindow::ShowHotkeyDialog);
  connect(m_menu_bar, &MenuBar::ConfigureBigPicture, this, &MainWindow::ShowBigPictureInputDialog);
  connect(m_menu_bar, &MenuBar::EnterBigPicture, this, &MainWindow::EnterBigPicture);
  connect(m_menu_bar, &MenuBar::ConfigureFreelook, this, &MainWindow::ShowFreeLookWindow);

  // Tools
  connect(m_menu_bar, &MenuBar::ShowMemcardManager, this, &MainWindow::ShowMemcardManager);
  connect(m_menu_bar, &MenuBar::ShowCloudSaves, this, &MainWindow::ShowCloudSavesWindow);
  connect(m_menu_bar, &MenuBar::ShowResourcePackManager, this,
          &MainWindow::ShowResourcePackManager);
  connect(m_menu_bar, &MenuBar::ShowCheatsManager, this, &MainWindow::ShowCheatsManager);
  connect(m_menu_bar, &MenuBar::BootGameCubeIPL, this, &MainWindow::OnBootGameCubeIPL);
  connect(m_menu_bar, &MenuBar::ImportNANDBackup, this, &MainWindow::OnImportNANDBackup);
  connect(m_menu_bar, &MenuBar::PerformOnlineUpdate, this, &MainWindow::PerformOnlineUpdate);
  connect(m_menu_bar, &MenuBar::BootWiiSystemMenu, this, &MainWindow::BootWiiSystemMenu);
  connect(m_menu_bar, &MenuBar::StartNetPlay, this, &MainWindow::ShowNetPlaySetupDialog);
  connect(m_menu_bar, &MenuBar::BrowseNetPlay, this, &MainWindow::ShowNetPlayBrowser);
  connect(m_menu_bar, &MenuBar::ShowFIFOPlayer, this, &MainWindow::ShowFIFOPlayer);
  connect(m_menu_bar, &MenuBar::ShowSkylanderPortal, this, &MainWindow::ShowSkylanderPortal);
  connect(m_menu_bar, &MenuBar::ShowInfinityBase, this, &MainWindow::ShowInfinityBase);
  connect(m_menu_bar, &MenuBar::ShowWiiSpeakWindow, this, &MainWindow::ShowWiiSpeakWindow);
  connect(m_menu_bar, &MenuBar::ShowLogitechMicWindow, this, &MainWindow::ShowLogitechMicWindow);
  connect(m_menu_bar, &MenuBar::ConnectWiiRemote, this, &MainWindow::OnConnectWiiRemote);

#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_menu_bar, &MenuBar::ShowAchievementsWindow, this, &MainWindow::ShowAchievementsWindow);
#endif  // USE_RETRO_ACHIEVEMENTS

  // Movie
  connect(m_menu_bar, &MenuBar::PlayRecording, this, &MainWindow::OnPlayRecording);
  connect(m_menu_bar, &MenuBar::StartRecording, this, &MainWindow::OnStartRecording);
  connect(m_menu_bar, &MenuBar::StopRecording, this, &MainWindow::OnStopRecording);
  connect(m_menu_bar, &MenuBar::ExportRecording, this, &MainWindow::OnExportRecording);
  connect(m_menu_bar, &MenuBar::ShowTASInput, this, &MainWindow::ShowTASInput);
  connect(m_menu_bar, &MenuBar::ConfigureOSD, this, &MainWindow::ShowOSDWindow);

  // View
  connect(m_menu_bar, &MenuBar::ShowList, m_game_list, &GameList::SetListView);
  connect(m_menu_bar, &MenuBar::ShowGrid, m_game_list, &GameList::SetGridView);
  connect(m_game_list, &GameList::PreferredViewChanged, m_menu_bar,
          &MenuBar::SetPreferredViewChecked);
  connect(m_menu_bar, &MenuBar::PurgeGameListCache, m_game_list, &GameList::PurgeCache);
  connect(m_menu_bar, &MenuBar::ShowSearch, m_game_list, &GameList::ShowSearch);

  connect(m_menu_bar, &MenuBar::ColumnVisibilityToggled, m_game_list,
          &GameList::OnColumnVisibilityToggled);

  connect(m_menu_bar, &MenuBar::GameListPlatformVisibilityToggled, m_game_list,
          &GameList::OnGameListVisibilityChanged);
  connect(m_menu_bar, &MenuBar::GameListRegionVisibilityToggled, m_game_list,
          &GameList::OnGameListVisibilityChanged);

  connect(m_menu_bar, &MenuBar::ShowAboutDialog, this, &MainWindow::ShowAboutDialog);
#ifdef SHOW_UPDATER
  connect(m_menu_bar, &MenuBar::ShowUpdateDialog, this, &MainWindow::ShowUpdateDialog);
#endif  // SHOW_UPDATER

  connect(m_game_list, &GameList::SelectionChanged, m_menu_bar, &MenuBar::SelectionChanged);
  connect(this, &MainWindow::ReadOnlyModeChanged, m_menu_bar, &MenuBar::ReadOnlyModeChanged);
  connect(this, &MainWindow::RecordingStatusChanged, m_menu_bar, &MenuBar::RecordingStatusChanged);
}

void MainWindow::EnterBigPicture()
{
  if (m_shutting_down || Settings::Instance().IsBatchModeEnabled())
    return;
  const auto state = Core::GetState(m_system);
  if (state == Core::State::Starting || state == Core::State::Stopping)
    return;
  if (m_standalone)
  {
    m_render_widget->window()->raise();
    m_render_widget->window()->activateWindow();
    return;
  }
  m_big_picture_requested = true;
  if (!Core::IsUninitialized(m_system))
  {
    if (!g_video_backend->SupportsUI())
    {
      m_big_picture_requested = false;
      ModalMessageBox::warning(this, tr("Big Picture"),
                               tr("Big Picture is unavailable with this graphics backend."));
      return;
    }
    RefreshBigPictureLibrary();
    m_big_picture_host->RequestAction(m_big_picture_host->GetSnapshot()->generation,
                                      BigPicture::Action::OpenMenu);
    if (!m_rendering_to_main)
      hide();
    return;
  }

  // Uninitialized is published at the end of core teardown. Join that owner before creating
  // any new graphics globals, including when the stop notification arrived just beforehand.
  Core::Shutdown(m_system);
  // Reuse the desktop's installed-title check rather than creating another IOS filesystem
  // just to populate the library options on each renderer handoff.
  m_big_picture_system_menu_available = m_ui->actionToolsBootSystemMenu->isEnabled();
  RefreshBigPictureLibrary();
  if (const auto selected = m_game_list->GetSelectedGame())
    m_big_picture_library->SetInitialSelection(selected->GetFilePath());
  // Prepare the opaque shell before showing either its own window or the main window.
  PrepareBigPictureSurface();
  ShowRenderWidget(true);
  m_render_widget->setWindowTitle(tr("Dolphin Big Picture"));
  // A desktop menu closes after its action runs and can restore focus to the old game-list
  // control. Move keyboard input to the persistent menu shell after that popup has closed.
  QTimer::singleShot(0, m_render_widget, [this] {
    if (m_big_picture_requested && m_render_widget->isVisible())
      m_render_widget->setFocus(Qt::OtherFocusReason);
  });
  if (m_rendering_to_main)
    setWindowTitle(tr("Dolphin Big Picture"));
  if (!m_rendering_to_main && m_render_widget_geometry.isEmpty())
    m_render_widget->resize(1280, 720);
  auto wsi = GetWindowSystemInfo();
  VideoBackendBase::PopulateBackendInfo(wsi);
  g_video_backend->PrepareWindow(wsi);
  m_standalone_graphics_config = BigPicture::ReadGraphicsConfig();
  m_big_picture_host->InvalidatePresentation();
  if (m_big_picture_host->GetSnapshot()->mode == BigPicture::Mode::Desktop)
    m_big_picture_host->RequestAction(m_big_picture_host->GetSnapshot()->generation,
                                      BigPicture::Action::OpenMenu);
  m_standalone_ready = false;
  m_standalone_stop = StandaloneStop::None;
  ++m_standalone_serial;
  m_standalone = VideoCommon::BigPicture::StandaloneSession::Create(g_video_backend, wsi);
  m_standalone_timer->start();
}

void MainWindow::PrepareBigPictureSurface()
{
  const auto config = BigPicture::ReadRenderSurfaceConfig();
  m_render_widget->PrepareRenderSurface(config != m_big_picture_surface_config);
  m_big_picture_surface_config = config;
}

void MainWindow::ClearContinuationBoot()
{
  m_continuation_boot.reset();
  m_continuation_boot_path.clear();
}

void MainWindow::StopStandalone(StandaloneStop next)
{
  if (m_shutting_down || m_standalone_stop == StandaloneStop::Exit)
    return;
  if (next == StandaloneStop::Restart && m_standalone_stop != StandaloneStop::None)
    return;
  if (next == StandaloneStop::Desktop || next == StandaloneStop::Settings ||
      next == StandaloneStop::Exit)
  {
    m_big_picture_requested = false;
    m_standalone_boot.reset();
    m_standalone_continuation_checked = false;
    ClearContinuationBoot();
  }
  m_standalone_stop = next;
  if (m_standalone)
    m_standalone->Stop();
  else
  {
    m_standalone_stop = StandaloneStop::None;
    CompleteStandaloneStop(next, false);
  }
}

void MainWindow::PollStandalone()
{
  if (!m_standalone || m_shutting_down)
    return;
  using Status = VideoCommon::BigPicture::StandaloneSession::Status;
  const auto status = m_standalone->GetStatus();
  if (status == Status::Ready && !m_standalone_ready && m_standalone_stop == StandaloneStop::None)
  {
    m_settings_post_processing = g_backend_info.bSupportsPostProcessing;
    m_standalone_ready = true;
    m_menu_bar->SetBigPictureActive(true);
    m_big_picture_host->Refresh();
    if (BigPicture::ReadGraphicsConfig() != m_standalone_graphics_config)
      StopStandalone(StandaloneStop::Restart);
  }
  if (status == Status::Ready && m_standalone_stop == StandaloneStop::None &&
      m_big_picture_host->GetSnapshot()->mode == BigPicture::Mode::Desktop)
    StopStandalone(StandaloneStop::Desktop);
  if (!m_standalone->IsFinished())
    return;
  const bool failed = m_standalone->GetStatus() == Status::Failed;
  const auto next =
      m_standalone_stop == StandaloneStop::None ? StandaloneStop::Desktop : m_standalone_stop;
  m_standalone.reset();  // IsFinished acknowledges all graphics destruction; joining is now safe.
  m_standalone_ready = false;
  m_standalone_timer->stop();
  m_standalone_stop = StandaloneStop::None;
  CompleteStandaloneStop(next, failed);
}

void MainWindow::CompleteStandaloneStop(StandaloneStop next, bool failed)
{
  m_menu_bar->SetBigPictureActive(false);
  if (next == StandaloneStop::Exit)
    hide();
  const bool keep_window = !failed && m_big_picture_requested &&
                           (next == StandaloneStop::Boot || next == StandaloneStop::Restart);
  if (!keep_window)
  {
    HideRenderWidget(next != StandaloneStop::Exit, next == StandaloneStop::Exit);
    SetFullScreenResolution(false);
  }
  if (failed)
  {
    m_big_picture_requested = false;
    ClearContinuationBoot();
    ModalMessageBox::warning(
        this, tr("Big Picture"),
        tr("Big Picture could not initialize or continue using the selected graphics backend. "
           "Dolphin has returned to the desktop."));
  }
  if (next == StandaloneStop::Boot)
  {
    auto boot = std::move(m_standalone_boot);
    if (boot)
      StartGame(std::move(boot), std::exchange(m_standalone_continuation_checked, false));
    m_big_picture_host->Refresh();
    if (Core::IsUninitialized(m_system) && m_big_picture_requested)
      QTimer::singleShot(0, this, &MainWindow::EnterBigPicture);
    return;
  }
  if (next == StandaloneStop::Restart && !failed && m_big_picture_requested)
  {
    EnterBigPicture();
    return;
  }
  m_big_picture_host->FinishStopToDesktop();
  if (next == StandaloneStop::Exit)
  {
    if (m_assembler_widget->ApplicationCloseRequest())
      QGuiApplication::exit(0);
    else
    {
      m_exit_requested = false;
      show();
      raise();
    }
    return;
  }
  show();
  raise();
  if (next == StandaloneStop::Settings)
    ShowSettingsWindow();
  else if (next == StandaloneStop::OpenGame)
  {
    Open();
    if (Core::IsUninitialized(m_system) && m_big_picture_requested)
      QTimer::singleShot(0, this, &MainWindow::EnterBigPicture);
  }
}

bool MainWindow::LeaveBigPictureForDesktop(std::uint64_t generation)
{
  if (m_big_picture_host->GetSnapshot()->generation != generation)
    return false;
  // A desktop handoff leaves a local game paused. In NetPlay the menu never owned a pause.
  m_big_picture_requested = false;
  m_big_picture_host->InvalidatePauseOwnership();
  m_big_picture_host->RequestAction(generation, BigPicture::Action::ReturnToDesktop);
  if (m_render_widget->isFullScreen())
    FullScreen();
  show();
  raise();
  activateWindow();
  return true;
}

void MainWindow::RefreshBigPictureStateSlots()
{
  static_assert(BigPicture::STATE_SLOT_COUNT == State::NUM_STATES);
  if (!Core::IsRunning(m_system))
    return;
  RequestBigPictureStateSlots(SConfig::GetInstance().GetGameID());
}

void MainWindow::RequestBigPictureStateSlots(std::string game_id)
{
  m_big_picture_state_slots_ready = false;
  m_state_slot_worker.Push({++m_big_picture_state_slots_revision, std::move(game_id)});
}

void MainWindow::ClearBigPictureStateSlots()
{
  // Drops any read still in flight for the previous game.
  ++m_big_picture_state_slots_revision;
  m_big_picture_state_slots_ready = false;
  m_big_picture_state_slots = {};
}

void MainWindow::BeginBigPictureStateRequest(std::uint64_t command_id, bool save, int slot)
{
  m_big_picture_state_request = {
      command_id, save,
      State::GetSlotPath(SConfig::GetInstance().GetGameID(), static_cast<u32>(slot))};
  // A result that never arrives must not leave the menu busy.
  QTimer::singleShot(std::chrono::seconds(30), this, [this, command_id] {
    if (!m_big_picture_state_request || m_big_picture_state_request->command_id != command_id)
      return;
    m_big_picture_state_request.reset();
    m_big_picture_host->CompleteCommand(
        command_id, BigPicture::ResultStatus::Failed,
        tr("Dolphin did not report whether the save state operation finished. Check the slot "
           "before relying on it.")
            .toStdString());
  });
}

void MainWindow::OnBigPictureStateResult(const State::OperationResult& result)
{
  using Error = State::OperationResult::Error;
  if (m_shutting_down || !m_big_picture_state_request)
    return;
  const auto request = *m_big_picture_state_request;
  if ((result.kind == State::OperationResult::Kind::Save) != request.save ||
      result.filename != request.filename)
    return;
  m_big_picture_state_request.reset();
  if (result.error == Error::None)
  {
    m_big_picture_host->CompleteCommand(request.command_id, BigPicture::ResultStatus::Succeeded);
    return;
  }
  const QString message =
      result.error == Error::NotAllowed ?
          tr("Loading save states is unavailable during NetPlay or in hardcore mode.") :
      result.error == Error::Capture ?
          tr("The game state could not be captured, so nothing was saved.") :
      result.error == Error::Write ?
          tr("The save state file could not be written.") :
      result.error == Error::Unreadable ?
          tr("The save state file could not be read.") :
          tr("The save state could not be loaded, so the game was restored. It may come from "
             "another game or Dolphin version.");
  m_big_picture_host->CompleteCommand(request.command_id, BigPicture::ResultStatus::Failed,
                                      message.toStdString());
}

void MainWindow::ShowBigPictureTextInput(std::uint64_t generation)
{
  const auto snapshot = m_big_picture_host->GetSnapshot();
  if (m_shutting_down || m_big_picture_text_input || snapshot->generation != generation ||
      snapshot->screens.empty() || snapshot->screens.back() != BigPicture::Screen::TextInput ||
      !snapshot->host.library || !snapshot->host.library->native_text_input)
    return;
  auto* dialog = new BigPictureTextInput(m_render_widget->window(),
                                         snapshot->host.library->editing_path ? tr("Enter Path") :
                                                                                tr("Search"),
                                         QString::fromStdString(snapshot->host.library->draft),
                                         BigPictureTextInput::GetInputMethods());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  m_big_picture_text_input = dialog;
  m_big_picture_text_generation = generation;
  connect(dialog, &QDialog::finished, this, [this, dialog, generation](int result) {
    m_big_picture_text_input = nullptr;
    if (m_shutting_down)
      return;
    const auto current = m_big_picture_host->GetSnapshot();
    if (current->generation != generation || current->screens.empty() ||
        current->screens.back() != BigPicture::Screen::TextInput)
      return;
    using O = BigPicture::LibraryOperation;
    const auto send = [&](O operation, std::string value = {}) {
      m_big_picture_host->RequestAction(generation, BigPicture::Action::Library, {}, 0, 0,
                                        std::move(value), operation);
    };
    if (result == QDialog::Accepted)
    {
      send(O::TextReplace, dialog->GetText().toStdString());
      send(O::TextDone);
    }
    else if (result == BigPictureTextInput::ControllerKeyboard)
    {
      send(O::TextReplace, dialog->GetText().toStdString());
      send(O::ControllerKeyboard);
    }
    else
      m_big_picture_host->RequestBack();
    FocusRenderWidget();
  });
  dialog->open();
}

void MainWindow::RefreshBigPictureLibrary()
{
  if (!m_big_picture_library || m_shutting_down)
    return;
  if (!m_big_picture_requested && m_big_picture_host &&
      m_big_picture_host->GetSnapshot()->mode == BigPicture::Mode::Desktop &&
      !Core::IsRunning(m_system))
    return;
  const auto& model = m_game_list->GetGameListModel();
  std::vector<BigPicture::LibraryGame> games;
  // Metadata updates (play time, titles, tags, scan rows) must keep decoded covers and GPU
  // textures. Game files are immutable, so only compare covers of replaced entries.
  std::map<std::string, std::shared_ptr<const UICommon::GameFile>> cover_sources;
  bool covers_changed = false;
  for (int row = 0; row < model.rowCount({}); ++row)
  {
    if (!model.IsDiscGroupRepresentative(row))
      continue;
    const auto game = model.GetGameFile(row);
    BigPicture::LibraryGame entry;
    entry.path = game->GetFilePath();
    if (const auto previous = m_big_picture_cover_sources.find(entry.path);
        previous != m_big_picture_cover_sources.end() && previous->second != game &&
        previous->second->GetCoverImage().buffer != game->GetCoverImage().buffer)
      covers_changed = true;
    cover_sources.emplace(entry.path, game);
    entry.title = model.data(model.index(row, static_cast<int>(GameListModel::Column::Title)))
                      .toString()
                      .toStdString();
    entry.game_id = game->GetGameID();
    for (const auto& disc : model.GetDiscChoices(*game))
    {
      entry.discs.push_back({disc->GetFilePath(),
                             tr("Disc %1 — %2")
                                 .arg(disc->GetDiscNumber() + 1)
                                 .arg(QString::fromStdString(disc->GetFilePath()))
                                 .toStdString(),
                             false});
    }
    entry.platform =
        game->GetPlatform() == DiscIO::Platform::GameCubeDisc ? tr("GameCube").toStdString() :
        game->GetPlatform() == DiscIO::Platform::Triforce     ? tr("Triforce").toStdString() :
        DiscIO::IsWii(game->GetPlatform())                    ? tr("Wii").toStdString() :
                                                                tr("Executable").toStdString();
    entry.region = DiscIO::GetName(game->GetRegion(), true);
    entry.played_ms =
        model
            .data(model.index(row, static_cast<int>(GameListModel::Column::TimePlayed)),
                  GameListModel::SORT_ROLE)
            .toULongLong();
    entry.size = game->GetFileSize();
    for (const auto& tag : model.GetGameTags(entry.path))
      entry.tags.push_back(tag.toStdString());
    entry.description = entry.platform + " | " + entry.region + " | " + entry.game_id;
    if (!entry.tags.empty())
      for (const auto& tag : entry.tags)
        entry.description += " | " + tag;
    games.push_back(std::move(entry));
  }
  m_big_picture_cover_sources = std::move(cover_sources);
  m_big_picture_library->SetGames(std::move(games), covers_changed);
}

void MainWindow::ConnectHotkeys()
{
  connect(m_hotkey_scheduler, &HotkeyScheduler::BigPictureAction, this,
          [this](int action_value, quint64 generation, quint64 input_epoch, int slot,
                 quint64 confirmation_id, const QString& path, int library_operation,
                 quint64 library_revision) {
            const auto action = static_cast<BigPicture::Action>(action_value);
            if (action == BigPicture::Action::OpenMenu &&
                generation == m_big_picture_host->GetSnapshot()->generation &&
                BigPicture::GetPresentationState().IsInputCurrent(input_epoch) &&
                Core::IsRunning(m_system))
              RefreshBigPictureLibrary();
            if ((action == BigPicture::Action::SaveStates ||
                 action == BigPicture::Action::LoadStates) &&
                generation == m_big_picture_host->GetSnapshot()->generation &&
                BigPicture::GetPresentationState().IsInputCurrent(input_epoch) &&
                Core::IsRunning(m_system))
              RefreshBigPictureStateSlots();
            m_big_picture_host->RequestAction(
                generation, action, input_epoch, slot, confirmation_id, path.toStdString(),
                static_cast<BigPicture::LibraryOperation>(library_operation), library_revision);
          });
  qRegisterMetaType<Common::Settings::Request>();
  qRegisterMetaType<BigPicture::SettingsRequest>();
  connect(m_hotkey_scheduler, &HotkeyScheduler::BigPictureSettingsMenu, this,
          [this](BigPicture::SettingsRequest request, quint64 generation, quint64 input_epoch) {
            m_big_picture_host->RequestSettingsMenu(generation, std::move(request), input_epoch);
          });
  connect(m_hotkey_scheduler, &HotkeyScheduler::BigPictureSetting, this,
          [this](Common::Settings::Request request, quint64 generation, quint64 input_epoch) {
            m_big_picture_host->RequestSetting(generation, std::move(request), input_epoch);
          });
  connect(
      m_hotkey_scheduler, &HotkeyScheduler::BigPictureOptions, this,
      [this](bool swap, double scale, double safe_area, quint64 generation, quint64 input_epoch) {
        const auto snapshot = m_big_picture_host->GetSnapshot();
        if (snapshot->generation != generation || snapshot->mode == BigPicture::Mode::Desktop ||
            !BigPicture::GetPresentationState().IsInputCurrent(input_epoch))
          return;
        BigPictureInput::SetOptions(
            {swap, static_cast<float>(scale), static_cast<float>(safe_area)});
        BigPicture::GetPresentationState().RequestFrames();
        VideoCommon::BigPicture::Wakeup();
      });
  connect(m_hotkey_scheduler, &HotkeyScheduler::Open, this, &MainWindow::Open);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ChangeDisc, this, &MainWindow::ChangeDisc);
  connect(m_hotkey_scheduler, &HotkeyScheduler::EjectDisc, this, &MainWindow::EjectDisc);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ExitHotkey, this, &MainWindow::close);
  connect(m_hotkey_scheduler, &HotkeyScheduler::UnlockCursor, this, &MainWindow::UnlockCursor);
  connect(m_hotkey_scheduler, &HotkeyScheduler::TogglePauseHotkey, this, &MainWindow::TogglePause);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ActivateChat, this, &MainWindow::OnActivateChat);
  connect(m_hotkey_scheduler, &HotkeyScheduler::CollapseChat, this, &MainWindow::OnCollapseChat);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ExpandChat, this, &MainWindow::OnExpandChat);
  connect(m_hotkey_scheduler, &HotkeyScheduler::RequestGolfControl, this,
          &MainWindow::OnRequestGolfControl);
  connect(m_hotkey_scheduler, &HotkeyScheduler::RefreshGameListHotkey, this,
          &MainWindow::RefreshGameList);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StopHotkey, this, &MainWindow::RequestStop);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ResetHotkey, this, &MainWindow::Reset);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ScreenShotHotkey, this, &MainWindow::ScreenShot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::FullScreenHotkey, this, &MainWindow::FullScreen);

  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadSlot, this, &MainWindow::StateLoadSlotAt);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveSlot, this, &MainWindow::StateSaveSlotAt);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadLastSaved, this,
          &MainWindow::StateLoadLastSavedAt);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadUndo, this, &MainWindow::StateLoadUndo);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveUndo, this, &MainWindow::StateSaveUndo);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveOldest, this,
          &MainWindow::StateSaveOldest);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveFile, this, &MainWindow::StateSave);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadFile, this, &MainWindow::StateLoad);

  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadSlotHotkey, this,
          &MainWindow::StateLoadSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveSlotHotkey, this,
          &MainWindow::StateSaveSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::SetStateSlotHotkey, this,
          &MainWindow::SetStateSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::IncrementSelectedStateSlotHotkey, this,
          &MainWindow::IncrementSelectedStateSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::DecrementSelectedStateSlotHotkey, this,
          &MainWindow::DecrementSelectedStateSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StartRecording, this,
          &MainWindow::OnStartRecording);
  connect(m_hotkey_scheduler, &HotkeyScheduler::PlayRecording, this, &MainWindow::OnPlayRecording);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ExportRecording, this,
          &MainWindow::OnExportRecording);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ConnectWiiRemote, this,
          &MainWindow::OnConnectWiiRemote);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ToggleReadOnlyMode, [this] {
    auto& movie = m_system.GetMovie();
    bool read_only = !movie.IsReadOnly();
    movie.SetReadOnly(read_only);
    emit ReadOnlyModeChanged(read_only);
  });
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_hotkey_scheduler, &HotkeyScheduler::OpenAchievements, this,
          &MainWindow::ShowAchievementsWindow, Qt::QueuedConnection);
#endif  // USE_RETRO_ACHIEVEMENTS

  connect(m_hotkey_scheduler, &HotkeyScheduler::Step, m_code_widget, &CodeWidget::Step);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StepOver, m_code_widget, &CodeWidget::StepOver);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StepOut, m_code_widget, &CodeWidget::StepOut);
  connect(m_hotkey_scheduler, &HotkeyScheduler::Skip, m_code_widget, &CodeWidget::Skip);

  connect(m_hotkey_scheduler, &HotkeyScheduler::ShowPC, m_code_widget, &CodeWidget::ShowPC);
  connect(m_hotkey_scheduler, &HotkeyScheduler::SetPC, m_code_widget, &CodeWidget::SetPC);

  connect(m_hotkey_scheduler, &HotkeyScheduler::ToggleBreakpoint, m_code_widget,
          &CodeWidget::ToggleBreakpoint);
  connect(m_hotkey_scheduler, &HotkeyScheduler::AddBreakpoint, m_code_widget,
          &CodeWidget::AddBreakpoint);

  connect(m_hotkey_scheduler, &HotkeyScheduler::SkylandersPortalHotkey, this,
          &MainWindow::ShowSkylanderPortal);
  connect(m_hotkey_scheduler, &HotkeyScheduler::InfinityBaseHotkey, this,
          &MainWindow::ShowInfinityBase);
}

void MainWindow::ConnectToolBar()
{
  connect(m_tool_bar, &ToolBar::OpenPressed, this, &MainWindow::Open);
  connect(m_tool_bar, &ToolBar::RefreshPressed, this, &MainWindow::RefreshGameList);

  connect(m_tool_bar, &ToolBar::PlayPressed, this, [this] { Play(); });
  connect(m_tool_bar, &ToolBar::PausePressed, this, &MainWindow::Pause);
  connect(m_tool_bar, &ToolBar::StopPressed, this, &MainWindow::RequestStop);
  connect(m_tool_bar, &ToolBar::FullScreenPressed, this, &MainWindow::FullScreen);
  connect(m_tool_bar, &ToolBar::ScreenShotPressed, this, &MainWindow::ScreenShot);
  connect(m_tool_bar, &ToolBar::NetPlaySetupDialogPressed, this, &MainWindow::ShowNetPlaySetupDialog);
  connect(m_tool_bar, &ToolBar::SettingsPressed, this, &MainWindow::ShowSettingsWindow);
  connect(m_tool_bar, &ToolBar::ControllersPressed, this, &MainWindow::ShowControllersWindow);
  connect(m_tool_bar, &ToolBar::GraphicsPressed, this, &MainWindow::ShowGraphicsWindow);
  #ifdef SHOW_UPDATER
  connect(m_tool_bar, &ToolBar::InstallUpdateManuallyPressed, this, &MainWindow::ShowUpdateDialog);
#endif  // SHOW_UPDATER
  connect(m_tool_bar, &ToolBar::CloudSavesPressed, this, &MainWindow::ShowCloudSavesWindow);

  connect(m_tool_bar, &ToolBar::StepPressed, m_code_widget, &CodeWidget::Step);
  connect(m_tool_bar, &ToolBar::StepOverPressed, m_code_widget, &CodeWidget::StepOver);
  connect(m_tool_bar, &ToolBar::StepOutPressed, m_code_widget, &CodeWidget::StepOut);
  connect(m_tool_bar, &ToolBar::SkipPressed, m_code_widget, &CodeWidget::Skip);
  connect(m_tool_bar, &ToolBar::ShowPCPressed, m_code_widget, &CodeWidget::ShowPC);
  connect(m_tool_bar, &ToolBar::SetPCPressed, m_code_widget, &CodeWidget::SetPC);
}

void MainWindow::ConnectGameList()
{
  connect(m_game_list, &GameList::GameSelected, this, [this] { Play(); });
  connect(m_game_list, &GameList::NetPlayHost, this, &MainWindow::NetPlayHost);
  connect(m_game_list, &GameList::OnStartWithRiivolution, this,
          &MainWindow::ShowRiivolutionBootWidget);

  connect(m_game_list, &GameList::OpenGeneralSettings, this, &MainWindow::ShowGeneralWindow);
  connect(m_game_list, &GameList::OpenGraphicsSettings, this, &MainWindow::ShowGraphicsWindow);
}

void MainWindow::ConnectRenderWidget()
{
  m_rendering_to_main = false;
  m_render_widget->hide();
  connect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);
  connect(m_render_widget, &RenderWidget::FullscreenRequested, this, &MainWindow::FullScreen);
  connect(m_render_widget, &RenderWidget::FocusChanged, this, [this](bool focus) {
    if (!focus)
      m_big_picture_host->InvalidatePauseOwnership();
    if (m_render_widget->isFullScreen())
      SetFullScreenResolution(focus);
  });
}

void MainWindow::ConnectHost()
{
  connect(Host::GetInstance(), &Host::RequestStop, this, &MainWindow::RequestStop);
}

void MainWindow::ConnectStack()
{
  m_state_result_hook = State::AddResultCallback([this](const State::OperationResult& result) {
    QueueOnObject(this, [this, result] { OnBigPictureStateResult(result); });
  });
  m_state_slot_worker.Reset("BigPictureStateSlots", [this](StateSlotJob job) {
    // Not "slots": Qt defines it as a keyword macro.
    std::array<BigPicture::StateSlot, BigPicture::STATE_SLOT_COUNT> read;
    for (std::size_t slot = 0; slot < read.size(); ++slot)
    {
      const auto info = State::GetSlotInfo(job.game_id, static_cast<u32>(slot + 1));
      read[slot] = {info.exists, info.readable, info.description, info.preview};
    }
    QueueOnObject(this, [this, revision = job.revision, read = std::move(read)] {
      if (m_shutting_down || revision != m_big_picture_state_slots_revision)
        return;
      m_big_picture_state_slots = read;
      m_big_picture_state_slots_ready = true;
      m_big_picture_host->Refresh();
    });
  });
  m_state_save_hook = State::AddAfterSaveCallback([this] {
    QueueOnObject(this, [this] {
      if (m_shutting_down || !Core::IsRunning(m_system))
        return;
      m_big_picture_state_slots_ready = false;
      if (m_big_picture_host->GetSnapshot()->mode == BigPicture::Mode::InGame)
      {
        RefreshBigPictureStateSlots();
        m_big_picture_host->Refresh();
      }
    });
  });
  m_ui->gameListLayout->addWidget(m_game_list);
  connect(m_game_list, &GameList::GameCountUpdated, this,
          [this](const int total_games, const int visible_games) {
            m_total_games = total_games;
            m_visible_games = visible_games;
            UpdateStatusBar();
          });

  m_game_list->UpdateGameCount();

  connect(&Settings::Instance(), &Settings::GameCountVisibilityChanged, this,
          [this] { UpdateStatusBar(); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this] { UpdateStatusBar(); });

  auto* const status_timer = new QTimer(this);
  connect(status_timer, &QTimer::timeout, this, [this] {
    UpdateStatusBar();
    const auto snapshot = m_big_picture_host->GetSnapshot();
    // Refresh play time and asynchronously arriving RetroAchievements metadata.
    if (snapshot->mode == BigPicture::Mode::InGame &&
        (snapshot->host.lifecycle == BigPicture::Lifecycle::Running ||
         snapshot->host.lifecycle == BigPicture::Lifecycle::Paused))
      m_big_picture_host->Refresh();
  });
  status_timer->start(500);

  setDockOptions(DockOption::AllowNestedDocks | DockOption::AllowTabbedDocks);
  setTabPosition(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea, QTabWidget::North);
  addDockWidget(Qt::LeftDockWidgetArea, m_log_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_log_config_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_code_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_register_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_thread_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_watch_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_breakpoint_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_memory_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_network_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_jit_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_assembler_widget);

  tabifyDockWidget(m_log_widget, m_log_config_widget);
  tabifyDockWidget(m_log_widget, m_code_widget);
  tabifyDockWidget(m_log_widget, m_register_widget);
  tabifyDockWidget(m_log_widget, m_thread_widget);
  tabifyDockWidget(m_log_widget, m_watch_widget);
  tabifyDockWidget(m_log_widget, m_breakpoint_widget);
  tabifyDockWidget(m_log_widget, m_memory_widget);
  tabifyDockWidget(m_log_widget, m_network_widget);
  tabifyDockWidget(m_log_widget, m_jit_widget);
  tabifyDockWidget(m_log_widget, m_assembler_widget);
}

void MainWindow::UpdateStatusBar()
{
  const Core::State state = Core::GetState(m_system);
  const bool is_emulating = state == Core::State::Running || state == Core::State::Paused;

  if (!is_emulating)
  {
    m_emulation_status->SetStatus({});

    if (Settings::Instance().IsGameCountVisible())
    {
      m_ui->statusBar->OnGameCountUpdated(m_total_games, m_visible_games);
      m_ui->statusBar->show();
    }
    else
    {
      m_ui->statusBar->clearMessage();
      m_ui->statusBar->hide();
    }
    return;
  }

  EmulationStatus status;
  status.active = true;
  if (g_video_backend)
    status.renderer = tr(g_video_backend->GetDisplayName().c_str());

  auto& performance_metrics = m_system.GetPerfMetrics();
  const auto [width, height] = performance_metrics.GetLatestFrameBufferSize();
  status.width = width;
  status.height = height;
  status.fps = performance_metrics.GetFPS();
  status.vps = performance_metrics.GetVPS();
  status.speed = performance_metrics.GetSpeed();
  status.volume = Config::Get(Config::MAIN_AUDIO_VOLUME);
  status.muted = Config::Get(Config::MAIN_AUDIO_MUTED);

  m_ui->statusBar->clearMessage();
  m_ui->statusBar->show();
  m_emulation_status->SetStatus(status);
}

void MainWindow::RefreshGameList()
{
  Settings::Instance().ReloadTitleDB();
  Settings::Instance().RefreshGameList();
}

QStringList MainWindow::PromptFileNames()
{
  auto& settings = Settings::Instance().GetQSettings();
  QStringList paths = DolphinFileDialog::getOpenFileNames(
      this, tr("Select a File"),
      settings.value(QStringLiteral("mainwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.elf *.dol *.gcm *.bin *.iso *.tgc *.wbfs *.ciso *.gcz *.wia *.rvz "
                     "hif_000000.nfs *.wad *.dff *.m3u *.json);;%2 (*)")
          .arg(tr("All GC/Wii files"))
          .arg(tr("All Files")));

  if (!paths.isEmpty())
  {
    settings.setValue(QStringLiteral("mainwindow/lastdir"),
                      QFileInfo(paths.front()).absoluteDir().absolutePath());
  }

  return paths;
}

void MainWindow::ChangeDisc()
{
  std::vector<std::string> paths = StringListToStdVector(PromptFileNames());

  if (paths.empty())
    return;

  m_system.GetDVDInterface().ChangeDisc(Core::CPUThreadGuard{m_system}, paths);
}

void MainWindow::EjectDisc()
{
  m_system.GetDVDInterface().EjectDisc(Core::CPUThreadGuard{m_system}, DVD::EjectCause::User);
}

void MainWindow::OpenUserFolder()
{
  std::string path = File::GetUserPath(D_USER_IDX);

  QUrl url = QUrl::fromLocalFile(QString::fromStdString(path));
  QDesktopServices::openUrl(url);
}

void MainWindow::OpenConfigFolder()
{
  std::string path = File::GetUserPath(D_CONFIG_IDX);

  QUrl url = QUrl::fromLocalFile(QString::fromStdString(path));
  QDesktopServices::openUrl(url);
}

void MainWindow::OpenCacheFolder()
{
  std::string path = File::GetUserPath(D_CACHE_IDX);

  QUrl url = QUrl::fromLocalFile(QString::fromStdString(path));
  QDesktopServices::openUrl(url);
}

void MainWindow::Open()
{
  QStringList files = PromptFileNames();
  if (!files.isEmpty())
    StartGame(StringListToStdVector(files));
}

void MainWindow::Play(const std::optional<std::string>& savestate_path)
{
  // If we're in a paused game, start it up again.
  // Otherwise, play the selected game, if there is one.
  // Otherwise, play the default game.
  // Otherwise, play the last played game, if there is one.
  // Otherwise, prompt for a new game.
  if (Core::GetState(m_system) == Core::State::Paused)
  {
    Core::SetState(m_system, Core::State::Running);
  }
  else
  {
    std::shared_ptr<const UICommon::GameFile> selection = m_game_list->GetSelectedGame();
    if (selection)
    {
      const auto disc = savestate_path ? std::optional{selection->GetFilePath()} :
                                         ChooseLibraryDisc(selection->GetFilePath());
      if (disc)
      {
        StartGame(*disc, ScanForSecondDisc::Yes,
                  std::make_unique<BootSessionData>(savestate_path, DeleteSavestateAfterBoot::No));
      }
    }
    else
    {
      const QString default_path = QString::fromStdString(Config::Get(Config::MAIN_DEFAULT_ISO));
      if (!default_path.isEmpty() && QFile::exists(default_path))
      {
        StartGame(default_path, ScanForSecondDisc::Yes,
                  std::make_unique<BootSessionData>(savestate_path, DeleteSavestateAfterBoot::No));
      }
      else
      {
        Open();
      }
    }
  }
}

void MainWindow::Pause()
{
  Core::SetState(m_system, Core::State::Paused);
}

void MainWindow::TogglePause()
{
  if (Core::GetState(m_system) == Core::State::Paused)
  {
    Play();
  }
  else
  {
    Pause();
  }
}

void MainWindow::OnStopComplete()
{
  m_continuation_path.clear();
  m_big_picture_achievements.Clear();
  ClearBigPictureStateSlots();
  m_big_picture_play_timer.invalidate();
  m_big_picture_session_ms = 0;
  m_big_picture_previous_play_ms = 0;
  m_big_picture_play_time_started = false;
  m_graceful_shutdown_timer->stop();
  m_stop_requested = false;
  m_big_picture_host->FinishStopToDesktop();
  const bool return_to_library =
      m_big_picture_requested && !m_exit_requested && !Settings::Instance().IsBatchModeEnabled();
  if (!return_to_library)
    HideRenderWidget(!m_exit_requested, m_exit_requested);
#ifdef USE_DISCORD_PRESENCE
  if (!m_netplay_dialog->isVisible())
    Discord::UpdateDiscordPresence();
#endif

  if (!return_to_library)
    SetFullScreenResolution(false);

  if (m_exit_requested || Settings::Instance().IsBatchModeEnabled())
  {
    if (m_assembler_widget->ApplicationCloseRequest())
    {
      QGuiApplication::exit(0);
    }
    else
    {
      m_exit_requested = false;
    }
  }

  // If the current emulation prevented the booting of another, do that now
  if (m_pending_boot != nullptr)
  {
    StartGame(std::move(m_pending_boot));
    m_pending_boot.reset();
    return;
  }

  // Check if a forwarder channel requested booting a disc image
  const std::string forwarder_boot_path = WiiForwarder::ConsumePendingForwarderBoot();
  if (!forwarder_boot_path.empty())
  {
    StartGame(BootParameters::GenerateFromFile(forwarder_boot_path));
  }
  else if (m_big_picture_requested && !m_exit_requested &&
           !Settings::Instance().IsBatchModeEnabled())
    QTimer::singleShot(0, this, &MainWindow::EnterBigPicture);
}

bool MainWindow::RequestStop()
{
  if (m_big_picture_requested && !Core::IsUninitialized(m_system) && g_video_backend->SupportsUI())
  {
    const auto action = m_exit_requested ? BigPicture::Action::Exit : BigPicture::Action::Stop;
    m_exit_requested = false;
    const auto snapshot = m_big_picture_host->GetSnapshot();
    if (snapshot->mode == BigPicture::Mode::Desktop)
      m_big_picture_host->RequestAction(snapshot->generation, BigPicture::Action::OpenMenu);
    m_big_picture_host->RequestAction(snapshot->generation, action);
    return false;  // Completion belongs to the Big Picture confirmation.
  }
  return RequestStopWithConfirmation(false);
}

bool MainWindow::CanSaveContinuation() const
{
  const auto state = Core::GetState(m_system);
  return (state == Core::State::Running || state == Core::State::Paused) && !m_stop_requested &&
         !m_continuation_path.empty() && !NetPlay::IsNetPlayRunning() &&
         !m_system.GetMovie().IsMovieActive();
}

bool MainWindow::RequestStopWithConfirmation(bool confirmed, bool save_continuation)
{
  if (Core::IsUninitialized(m_system))
  {
    Core::QueueHostJob([this](Core::System&) { OnStopComplete(); }, true);
    return true;
  }

  const bool rendered_widget_was_active =
      Settings::Instance().IsKeepWindowOnTopEnabled() ||
      (m_render_widget->isActiveWindow() && !m_render_widget->isFullScreen());
  QWidget* confirm_parent = (!m_rendering_to_main && rendered_widget_was_active) ?
                                m_render_widget :
                                static_cast<QWidget*>(this);
  const bool was_cursor_locked = m_render_widget->IsCursorLocked();

  if (!m_render_widget->isFullScreen())
    m_render_widget_geometry = m_render_widget->saveGeometry();
  else if (!m_big_picture_requested)
    FullScreen();

  const auto original_state = Core::GetState(m_system);
  bool confirm_on_stop =
      !confirmed && (Config::Get(Config::MAIN_CONFIRM_ON_STOP) || CanSaveContinuation());
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  confirm_on_stop = confirm_on_stop || AchievementManager::GetInstance().CheckForModifications();
#endif  // RC_CLIENT_SUPPORTS_RAINTEGRATION

  // Skip confirmation when a forwarder channel is launching a disc image —
  // the user already chose to play the game from the Wii Menu.
  if (WiiForwarder::HasPendingForwarderBoot())
    confirm_on_stop = false;

  if (confirm_on_stop)
  {
    if (std::exchange(m_stop_confirm_showing, true))
      return true;

    Common::ScopeGuard confirm_lock([this] { m_stop_confirm_showing = false; });

    const Core::State state = Core::GetState(m_system);

    // Only pause the game, if NetPlay is not running
    bool pause = !Settings::Instance().GetNetPlayClient();

    if (pause)
      Core::SetState(m_system, Core::State::Paused);

    if (rendered_widget_was_active)
    {
      // We have to do this before creating the message box, otherwise we might receive the window
      // activation event before we know we need to lock the cursor again.
      m_render_widget->SetCursorLockedOnNextActivation(was_cursor_locked);
    }

    // This is to avoid any "race conditions" between the "Window Activate" message and the
    // message box returning, which could break cursor locking depending on the order
    m_render_widget->SetWaitingForMessageBox(true);
    QString message;
    if (m_stop_requested)
    {
      message = tr("A shutdown is already in progress. Unsaved data "
                   "may be lost if you stop the current emulation "
                   "before it completes. Force stop?");
    }
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
    else if (AchievementManager::GetInstance().CheckForModifications())
    {
      message = tr(
          "Do you want to stop the current emulation? Unsaved achievement modifications detected.");
    }
#endif  // RC_CLIENT_SUPPORTS_RAINTEGRATION
    else
    {
      message = m_exit_requested ? tr("Do you want to stop emulation and exit Dolphin?") :
                                   tr("Do you want to stop the current emulation?");
    }
    ModalMessageBox dialog(confirm_parent, Qt::ApplicationModal);
    dialog.setWindowTitle(m_exit_requested ? tr("Exit Dolphin") : tr("Stop Emulation"));
    dialog.setIcon(QMessageBox::Question);
    dialog.setText(message);
    dialog.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    dialog.setDefaultButton(QMessageBox::No);
    auto* continuation = new QCheckBox(tr("Create a continuation save state"), &dialog);
    continuation->setEnabled(CanSaveContinuation());
    continuation->setToolTip(tr("Keep a separate state to continue this game next time. "
                                "Unavailable during NetPlay, movies or shutdown."));
    dialog.setCheckBox(continuation);
    const auto confirm = dialog.exec();
    save_continuation = continuation->isEnabled() && continuation->isChecked();

    // If a user confirmed stopping the emulation, we do not capture the cursor again,
    // even if the render widget will stay alive for a while.
    // If a used rejected stopping the emulation, we instead capture the cursor again,
    // and let them continue playing as if nothing had happened
    // (assuming cursor locking is on).
    if (confirm != QMessageBox::Yes)
    {
      m_render_widget->SetWaitingForMessageBox(false);

      if (pause)
        Core::SetState(m_system, state);

      return false;
    }
    else
    {
      m_render_widget->SetCursorLockedOnNextActivation(false);
      // This needs to be after SetCursorLockedOnNextActivation(false) as it depends on it
      m_render_widget->SetWaitingForMessageBox(false);
    }
  }

  // Return to the native library before synchronous state capture/compression blocks Qt.
  // Keep the renderer alive until saving and core shutdown finish, and restore it if saving fails.
  const bool show_desktop =
      !m_big_picture_requested && !m_exit_requested && !Settings::Instance().IsBatchModeEnabled();
  const bool render_was_visible = m_render_widget->isVisible();
  if (show_desktop)
  {
    if (m_rendering_to_main)
      m_stack->setCurrentWidget(m_ui->gameListPage);
    m_render_widget->hide();
    show();
    m_stack->repaint();
  }
  if (save_continuation &&
      (!CanSaveContinuation() || !State::SaveContinuation(m_system, m_continuation_path)))
  {
    if (show_desktop && render_was_visible)
    {
      if (m_rendering_to_main)
        m_stack->setCurrentWidget(m_render_widget);
      m_render_widget->show();
      FocusRenderWidget();
    }
    Core::SetState(m_system, original_state);
    if (!m_big_picture_requested)
    {
      m_render_widget->SetCursorLockedOnNextActivation(was_cursor_locked);
      ModalMessageBox::critical(confirm_parent, tr("Continuation Save Failed"),
                                tr("Could not create the continuation state. The game is still "
                                   "open. Try again or choose to stop without saving."));
    }
    return false;
  }

  OnStopRecording();
  // TODO: Add Debugger shutdown

  // A continuation is already durably written. Stop at that captured state rather than
  // resuming the Wii for its power-button shutdown, which can wait for the ten-second timeout.
  if (!save_continuation && !m_stop_requested && UICommon::TriggerSTMPowerEvent())
  {
    m_stop_requested = true;

    // Unpause because gracefully shutting down needs the game to actually request a shutdown.
    // TODO: Do not unpause in debug mode to allow debugging until the complete shutdown.
    if (Core::GetState(m_system) == Core::State::Paused)
      Core::SetState(m_system, Core::State::Running);

    // Tell NetPlay about the power event
    if (NetPlay::IsNetPlayRunning())
      NetPlay::SendPowerButtonEvent();

    m_graceful_shutdown_timer->start(GRACEFUL_SHUTDOWN_TIMEOUT_MS);
    return true;
  }

  ForceStop();
#ifdef Q_OS_WIN
  // Allow windows to idle or turn off display again
  SetThreadExecutionState(ES_CONTINUOUS);
#endif
  return true;
}

void MainWindow::ForceStop()
{
  m_graceful_shutdown_timer->stop();
  Core::Stop(m_system);
}

void MainWindow::Reset()
{
  auto& movie = m_system.GetMovie();
  if (movie.IsRecordingInput())
    movie.SetReset(true);
  m_system.GetProcessorInterface().ResetButton_Tap();
}

void MainWindow::FrameAdvance()
{
  Core::DoFrameStep(m_system);
}

void MainWindow::FullScreen()
{
  // If the render widget is fullscreen we want to reset it to whatever is in
  // settings. If it's set to be fullscreen then it just remakes the window,
  // which probably isn't ideal.
  bool was_fullscreen = m_render_widget->isFullScreen();

  if (!was_fullscreen)
    m_render_widget_geometry = m_render_widget->saveGeometry();

  // Removing an embedded renderer selects the desktop page in the stack. Keep that
  // window hidden throughout the move to a separate fullscreen presentation.
  if (m_big_picture_requested)
    hide();
  HideRenderWidget(false);
  SetFullScreenResolution(!was_fullscreen);

  if (was_fullscreen)
  {
    ShowRenderWidget();
  }
  else
  {
    m_render_widget->showFullScreen();
  }

  FocusRenderWidget();
}

void MainWindow::FocusRenderWidget()
{
  // Reparenting an embedded renderer deactivates its old top-level window. Qt does not
  // necessarily activate the new owner, so restore both window and keyboard focus explicitly.
  m_render_widget->activateWindow();
  m_render_widget->setFocus();
}

void MainWindow::UnlockCursor()
{
  if (!m_render_widget->isFullScreen())
    m_render_widget->SetCursorLocked(false);
}

void MainWindow::ScreenShot()
{
  Core::SaveScreenShot();
}

std::optional<std::string> MainWindow::ChooseLibraryDisc(const std::string& path)
{
  const auto& model = m_game_list->GetGameListModel();
  const auto game = model.FindGame(path);
  if (!game)
    return path;

  // Copy the choices: the library can be rescanned while the modal dialog is open.
  const auto discs = model.GetDiscChoices(*game);
  if (discs.empty())
    return path;

  QStringList labels;
  for (const auto& disc : discs)
  {
    labels.append(tr("Disc %1 — %2")
                      .arg(disc->GetDiscNumber() + 1)
                      .arg(QString::fromStdString(disc->GetFileName())));
  }
  const QStringList short_labels = labels;
  for (qsizetype i = 0; i < labels.size(); ++i)
  {
    if (short_labels.count(short_labels[i]) > 1)
    {
      labels[i] = tr("Disc %1 — %2")
                      .arg(discs[i]->GetDiscNumber() + 1)
                      .arg(QString::fromStdString(discs[i]->GetFilePath()));
    }
  }
  QWidget* parent = m_render_widget->isVisible() ? m_render_widget->window() : this;
  QInputDialog dialog(parent);
  dialog.setWindowTitle(tr("Select Disc"));
  dialog.setLabelText(tr("Which disc would you like to start?"));
  dialog.setComboBoxItems(labels);
  dialog.setComboBoxEditable(false);
  dialog.setWindowModality(Qt::WindowModal);
  if (dialog.exec() != QDialog::Accepted)
    return std::nullopt;

  const int choice = labels.indexOf(dialog.textValue());
  return choice < 0 ? std::nullopt : std::optional{discs[choice]->GetFilePath()};
}

void MainWindow::ScanForSecondDiscAndStartGame(const UICommon::GameFile& game,
                                               std::unique_ptr<BootSessionData> boot_session_data)
{
  std::vector<std::string> paths = {game.GetFilePath()};
  const auto& discs = m_game_list->GetGameListModel().GetDiscChoices(game);
  if (!discs.empty())
  {
    std::set<u8> added_discs{game.GetDiscNumber()};
    for (const auto& disc : discs)
    {
      if (added_discs.insert(disc->GetDiscNumber()).second)
        paths.push_back(disc->GetFilePath());
    }
  }
  else if (const auto second_game = m_game_list->FindSecondDisc(game))
  {
    paths.push_back(second_game->GetFilePath());
  }

  StartGame(paths, std::move(boot_session_data));
}

void MainWindow::StartGame(const QString& path, ScanForSecondDisc scan,
                           std::unique_ptr<BootSessionData> boot_session_data)
{
  StartGame(path.toStdString(), scan, std::move(boot_session_data));
}

void MainWindow::StartGame(const std::string& path, ScanForSecondDisc scan,
                           std::unique_ptr<BootSessionData> boot_session_data)
{
  if (scan == ScanForSecondDisc::Yes)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list->FindGame(path);
    if (game != nullptr)
    {
      ScanForSecondDiscAndStartGame(*game, std::move(boot_session_data));
      return;
    }
  }

  StartGame(BootParameters::GenerateFromFile(
      path, boot_session_data ? std::move(*boot_session_data) : BootSessionData()));
}

void MainWindow::StartGame(const std::vector<std::string>& paths,
                           std::unique_ptr<BootSessionData> boot_session_data)
{
  StartGame(BootParameters::GenerateFromFile(
      paths, boot_session_data ? std::move(*boot_session_data) : BootSessionData()));
}

void MainWindow::StartGame(std::unique_ptr<BootParameters>&& parameters, bool continuation_checked)
{
  if (m_shutting_down || m_continuation_boot)
    return;
  if (parameters && Core::IsUninitialized(m_system) && !continuation_checked &&
      !parameters->boot_session_data.GetSavestatePath() &&
      !parameters->boot_session_data.GetNetplaySettings() && !NetPlay::IsNetPlayRunning() &&
      !m_system.GetMovie().IsMovieActive())
  {
    const auto path = ContinuationPathForBoot(*parameters);
    if (!path.empty() && File::Exists(path))
    {
      if (m_big_picture_requested)
      {
        m_continuation_boot_path = path;
        m_continuation_boot = std::move(parameters);
        QTimer::singleShot(0, this, [this] {
          if (m_shutting_down || !m_continuation_boot)
            return;
          if (!m_standalone)
            EnterBigPicture();
          if (!m_big_picture_requested)
          {
            auto boot = std::move(m_continuation_boot);
            m_continuation_boot_path.clear();
            if (boot)
              StartGame(std::move(boot));
            return;
          }
          m_big_picture_host->Refresh();
        });
        return;
      }
      ModalMessageBox dialog(this, Qt::ApplicationModal);
      dialog.setWindowTitle(tr("Continue Game?"));
      dialog.setIcon(QMessageBox::Question);
      dialog.setText(tr("A continuation save state is available for this game."));
      dialog.setInformativeText(tr("Start Fresh keeps the continuation for later."));
      auto* resume = dialog.addButton(tr("Continue from Save State"), QMessageBox::AcceptRole);
      resume->setEnabled(ContinuationLoadingAllowed());
      if (!resume->isEnabled())
        resume->setToolTip(tr("Loading save states is unavailable in Hardcore mode or NetPlay."));
      auto* fresh = dialog.addButton(tr("Start Fresh"), QMessageBox::AcceptRole);
      auto* remove =
          dialog.addButton(tr("Delete Continuation and Start Fresh"), QMessageBox::DestructiveRole);
      dialog.addButton(QMessageBox::Cancel);
      dialog.setDefaultButton(resume->isEnabled() ? resume : fresh);
      dialog.exec();
      if (dialog.clickedButton() == resume && resume->isEnabled())
        parameters->boot_session_data.SetSavestateData(path, DeleteSavestateAfterBoot::No);
      else if (dialog.clickedButton() == remove)
      {
        if (!State::DeleteContinuation(path))
        {
          ModalMessageBox::critical(this, tr("Error"),
                                    tr("Could not delete the continuation state."));
          return;
        }
      }
      else if (dialog.clickedButton() != fresh)
        return;
      continuation_checked = true;
    }
  }
  if (Core::IsUninitialized(m_system) && m_standalone)
  {
    m_standalone_boot = std::move(parameters);
    m_standalone_continuation_checked = continuation_checked;
    StopStandalone(StandaloneStop::Boot);
    m_big_picture_host->Refresh();
    return;
  }
  if (parameters && std::holds_alternative<BootParameters::Disc>(parameters->parameters))
  {
    if (std::get<BootParameters::Disc>(parameters->parameters).volume->IsNKit())
    {
      if (!NKitWarningDialog::ShowUnlessDisabled())
        return;
    }

    const auto volume_type =
        std::get<BootParameters::Disc>(parameters->parameters).volume->GetVolumeType();
    if (volume_type != DiscIO::Platform::Triforce)
    {
      const bool triforce_hardware_sp1 =
          Config::Get(Config::MAIN_SERIAL_PORT_1) == ExpansionInterface::EXIDeviceType::Baseboard;
      const bool triforce_hardware_port_1 = Config::Get(Config::GetInfoForSIDevice(0)) ==
                                            SerialInterface::SIDevices::SIDEVICE_AM_BASEBOARD;

      // Some Triforce tools don't include a boot.id file, but they can still be launched.
      if (triforce_hardware_sp1)
      {
        ModalMessageBox::warning(this, tr("Warning"),
                                 tr("Non-Triforce games cannot be booted with Triforce hardware "
                                    "attached.\nPlease remove the Triforce Baseboard from SP1."),
                                 QMessageBox::Ok);
      }
      if (triforce_hardware_port_1)
      {
        ModalMessageBox::warning(this, tr("Warning"),
                                 tr("Non-Triforce games cannot be booted with Triforce hardware "
                                    "attached.\nPlease remove the Triforce Baseboard from Port 1."),
                                 QMessageBox::Ok);
      }
    }
  }

  // If we're running, only start a new game once we've stopped the last.
  if (!Core::IsUninitialized(m_system))
  {
    if (!RequestStop())
      return;

    // As long as the shutdown isn't complete, we can't boot, so let's boot later
    m_pending_boot = std::move(parameters);
    return;
  }

  // We need the render widget before booting.
  if (m_big_picture_requested)
  {
    Core::Shutdown(m_system);
    PrepareBigPictureSurface();
  }
  ShowRenderWidget(true);

  // Boot up, show an error if it fails to load the game.
  m_continuation_path = parameters ? ContinuationPathForBoot(*parameters) : std::string{};
  ClearBigPictureStateSlots();
  m_big_picture_disc_game =
      parameters && std::holds_alternative<BootParameters::Disc>(parameters->parameters);
  m_big_picture_disc_path = m_big_picture_disc_game ?
                                std::get<BootParameters::Disc>(parameters->parameters).path :
                                std::string{};
  const auto snapshot = m_big_picture_host->GetSnapshot();
  const bool menu_boot = snapshot->pending && BigPicture::IsBootAction(snapshot->pending->action);
  m_big_picture_host->PrepareForBoot();
  if (!BootManager::BootCore(m_system, std::move(parameters), GetWindowSystemInfo(), [this] {
        // Game INIs are loaded by BootCore. Recheck compatibility after those settings
        // are active, while still on the GUI thread and before the GPU owner starts.
        if (m_big_picture_requested)
          PrepareBigPictureSurface();
        return GetWindowSystemInfo();
      }))
  {
    if (!menu_boot)
      ModalMessageBox::critical(this, tr("Error"), tr("Failed to init core"), QMessageBox::Ok);
    if (!m_big_picture_requested)
      HideRenderWidget();
    m_big_picture_host->Refresh();
    if (m_big_picture_requested)
      QTimer::singleShot(0, this, &MainWindow::EnterBigPicture);
    return;
  }

#ifdef USE_DISCORD_PRESENCE
  if (!NetPlay::IsNetPlayRunning())
    Discord::UpdateDiscordPresence();
#endif

  if (Config::Get(Config::MAIN_FULLSCREEN) && !m_render_widget->isFullScreen())
    m_fullscreen_requested = true;
}

void MainWindow::SetFullScreenResolution(bool fullscreen)
{
  if (Config::Get(Config::MAIN_FULLSCREEN_DISPLAY_RES) == "Auto")
    return;
#ifdef _WIN32

  if (!fullscreen)
  {
    ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
    return;
  }

  DEVMODE screen_settings;
  memset(&screen_settings, 0, sizeof(screen_settings));
  screen_settings.dmSize = sizeof(screen_settings);
  sscanf(Config::Get(Config::MAIN_FULLSCREEN_DISPLAY_RES).c_str(), "%lux%lu",
         &screen_settings.dmPelsWidth, &screen_settings.dmPelsHeight);
  screen_settings.dmBitsPerPel = 32;
  screen_settings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

  // Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
  ChangeDisplaySettings(&screen_settings, CDS_FULLSCREEN);
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
  if (m_xrr_config)
    m_xrr_config->ToggleDisplayMode(fullscreen);
#endif
}

void MainWindow::ShowRenderWidget(bool preserve_window)
{
  // Boot, stop and idle backend changes retain the visible Big Picture shell, its native
  // window, placement and fullscreen state. Only the graphics child is recreated.
  if (preserve_window && m_big_picture_requested && m_render_widget->isVisible())
  {
    if (!m_rendering_to_main)
      hide();
    return;
  }
  SetFullScreenResolution(false);
  Host::GetInstance()->SetRenderFullscreen(false);

  if (Config::Get(Config::MAIN_RENDER_TO_MAIN))
  {
    // If we're rendering to main, add it to the stack and update our title when necessary.
    m_rendering_to_main = true;

    m_stack->setCurrentIndex(m_stack->addWidget(m_render_widget));
    connect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
    m_stack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    if (m_big_picture_requested)
      show();
    m_stack->repaint();

    Host::GetInstance()->SetRenderFocus(isActiveWindow());
  }
  else
  {
    // Otherwise, just show it.
    m_rendering_to_main = false;

    // The desktop must not be available behind Big Picture during focus changes or
    // graphics teardown. Explicit desktop/settings handoffs restore it when needed.
    if (m_big_picture_requested)
      hide();
    m_render_widget->showNormal();
    m_render_widget->restoreGeometry(m_render_widget_geometry);
  }
}

void MainWindow::HideRenderWidget(bool reinit, bool is_exit)
{
  if (m_rendering_to_main)
  {
    // Remove the widget from the stack and reparent it to nullptr, so that it can draw
    // itself in a new window if it wants. Disconnect the title updates.
    m_stack->removeWidget(m_render_widget);
    m_render_widget->setParent(nullptr);
    m_rendering_to_main = false;
    m_stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    disconnect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
    setWindowTitle(QString::fromStdString(Common::GetScmRevStr()));
  }

  // The following code works around a driver bug that would lead to Dolphin crashing when changing
  // graphics backends (e.g. OpenGL to Vulkan). To avoid this the render widget is (safely)
  // recreated
  if (reinit)
  {
    m_render_widget->hide();
    disconnect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);

    m_render_widget->removeEventFilter(this);
    m_render_widget->deleteLater();

    m_render_widget = new RenderWidget;

    m_render_widget->installEventFilter(this);
    ConnectRenderWidget();

    // The controller interface will still be registered to the old render widget, if the core
    // has booted. Therefore, we should re-bind it to the main window for now. When the core
    // is next started, it will be swapped back to the new render widget.
    g_controller_interface.ChangeWindow(::GetWindowSystemInfo(windowHandle()).render_window,
                                        is_exit ? ControllerInterface::WindowChangeReason::Exit :
                                                  ControllerInterface::WindowChangeReason::Other);
  }
}

void MainWindow::ShowControllersWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::Controllers);
}

void MainWindow::ShowCloudSavesWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::CloudSaves);
}

void MainWindow::ShowTriforceWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::Triforce);
}

void MainWindow::ShowFreeLookWindow()
{
  if (!m_freelook_window)
  {
    m_freelook_window = new FreeLookWindow(this);
    InstallHotkeyFilter(m_freelook_window);

#ifdef USE_RETRO_ACHIEVEMENTS
    connect(m_freelook_window, &FreeLookWindow::OpenAchievementSettings, this,
            &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS
  }

  m_freelook_window->show();
  m_freelook_window->raise();
  m_freelook_window->activateWindow();
}

void MainWindow::ShowSettingsWindow()
{
  if (!m_settings_window)
  {
#ifdef HAVE_XRANDR
    if (GetWindowSystemType() == WindowSystemType::X11)
    {
      m_xrr_config = std::make_unique<X11Utils::XRRConfiguration>(
          static_cast<Display*>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
              "display", windowHandle())),
          winId());
    }
#endif
    m_settings_window = new SettingsWindow(this);
    connect(m_settings_window, &SettingsWindow::ConfigureBigPictureInput, this,
            &MainWindow::ShowBigPictureInputDialog);
    InstallHotkeyFilter(m_settings_window);
  }

  m_settings_window->show();
  m_settings_window->raise();
  m_settings_window->activateWindow();
}

void MainWindow::ShowAudioWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::Audio);
}

void MainWindow::ShowGeneralWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::General);
}

void MainWindow::ShowOSDWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::OnScreenDisplay);
}

void MainWindow::ShowAboutDialog()
{
  AboutDialog about{this};
  about.exec();
}

// P+ change: New updater; credit to RainbowTabitha and the Mario Party Netplay team for the base code!

#ifdef SHOW_UPDATER
void MainWindow::ShowUpdateDialog()
{
    Common::HttpRequest httpRequest;

    // Make the GET request
    auto response = httpRequest.Get("https://api.github.com/repos/sharath-5br2r/Dolphin-Extra/releases/latest");

    if (response)
    {
        // Access the underlying vector and convert it to QByteArray
        QByteArray responseData(reinterpret_cast<const char*>(response->data()), response->size());

        // Parse the JSON response
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonDoc.object();
      
        QString currentVersion = QString::fromStdString(SCM_DESC_STR);
        QString latestVersion = jsonObject.value(QStringLiteral("tag_name")).toString();

        if (currentVersion != latestVersion)
        {
          // Create and show the UpdateDialog with the fetched data
          bool forced = false; // Set this based on your logic
          UserInterface::Dialog::UpdateDialog updater(this, jsonObject, forced);
          updater.exec();
        } else {
          QMessageBox::information(this, tr("Info"), tr("You are already up to date."));
        }
    }
    else
    {
        // Handle error
        QMessageBox::critical(this, tr("Error"), tr("Failed to fetch update information."));
    }
}

void MainWindow::CheckForUpdatesAuto()
{
    Common::HttpRequest httpRequest;

    // Make the GET request
    auto response = httpRequest.Get("https://api.github.com/repos/sharath-5br2r/Dolphin-Extra/releases/latest");

    if (response)
    {
        // Access the underlying vector and convert it to QByteArray
        QByteArray responseData(reinterpret_cast<const char*>(response->data()), response->size());

        // Parse the JSON response
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonDoc.object();
      
        QString currentVersion = QString::fromStdString(SCM_DESC_STR);
        QString latestVersion = jsonObject.value(QStringLiteral("tag_name")).toString();

        if (currentVersion != latestVersion)
        {
          // Create and show the UpdateDialog with the fetched data
          bool forced = false; // Set this based on your logic
          UserInterface::Dialog::UpdateDialog updater(this, jsonObject, forced);
          updater.exec();
        }
    }
    else
    {
        // Handle error
        QMessageBox::critical(this, tr("Error"), tr("Failed to fetch update information."));
    }
}
#endif  // SHOW_UPDATER

void MainWindow::ShowHotkeyDialog()
{
  if (!m_hotkey_window)
  {
    m_hotkey_window = new MappingWindow(this, MappingWindow::Type::MAPPING_HOTKEYS, 0);
    InstallHotkeyFilter(m_hotkey_window);
  }

  m_hotkey_window->show();
  m_hotkey_window->raise();
  m_hotkey_window->activateWindow();
}

void MainWindow::ShowBigPictureInputDialog(int controller)
{
  if (controller < 0 || controller >= static_cast<int>(m_big_picture_input_windows.size()))
    return;
  auto*& window = m_big_picture_input_windows[controller];
  if (!window)
  {
    window = new MappingWindow(this, MappingWindow::Type::MAPPING_BIG_PICTURE, controller);
    InstallHotkeyFilter(window);
  }
  window->show();
  window->raise();
  window->activateWindow();
}

void MainWindow::ShowGraphicsWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectPane(SettingsWindowPaneIndex::Graphics);
}

void MainWindow::ShowNetPlaySetupDialog()
{
  m_netplay_setup_dialog->show();
  m_netplay_setup_dialog->raise();
  m_netplay_setup_dialog->activateWindow();
}

void MainWindow::ShowNetPlayBrowser()
{
  auto* browser = new NetPlayBrowser(this);
  browser->setAttribute(Qt::WA_DeleteOnClose, true);
  connect(browser, &NetPlayBrowser::Join, this, &MainWindow::NetPlayJoin);
  browser->exec();
}

void MainWindow::ShowFIFOPlayer()
{
  if (!m_fifo_window)
  {
    m_fifo_window.reset(new FIFOPlayerWindow(m_system.GetFifoPlayer(), m_system.GetFifoRecorder()));
    connect(m_fifo_window.get(), &FIFOPlayerWindow::LoadFIFORequested, this,
            [this](const QString& path) { StartGame(path, ScanForSecondDisc::No); });
  }

  m_fifo_window->show();
  m_fifo_window->raise();
  m_fifo_window->activateWindow();
}

void MainWindow::ShowSkylanderPortal()
{
  if (!m_skylander_window)
  {
    m_skylander_window = new SkylanderPortalWindow();
  }

  m_skylander_window->show();
  m_skylander_window->raise();
  m_skylander_window->activateWindow();
}

void MainWindow::ShowInfinityBase()
{
  if (!m_infinity_window)
  {
    m_infinity_window = new InfinityBaseWindow();
  }

  m_infinity_window->show();
  m_infinity_window->raise();
  m_infinity_window->activateWindow();
}

void MainWindow::ShowWiiSpeakWindow()
{
  if (!m_wii_speak_window)
  {
    m_wii_speak_window = new WiiSpeakWindow();
  }

  m_wii_speak_window->show();
  m_wii_speak_window->raise();
  m_wii_speak_window->activateWindow();
}

void MainWindow::ShowLogitechMicWindow()
{
  if (!m_logitech_mic_window)
  {
    m_logitech_mic_window = new LogitechMicWindow();
  }

  m_logitech_mic_window->show();
  m_logitech_mic_window->raise();
  m_logitech_mic_window->activateWindow();
}

void MainWindow::StateLoad()
{
  QString dialog_path = (Config::Get(Config::MAIN_CURRENT_STATE_PATH).empty()) ?
                            QDir::currentPath() :
                            QString::fromStdString(Config::Get(Config::MAIN_CURRENT_STATE_PATH));
  QString path = DolphinFileDialog::getOpenFileName(
      this, tr("Select a File"), dialog_path, tr("All Save States (*.sav *.s??);; All Files (*)"));
  Config::SetBase(Config::MAIN_CURRENT_STATE_PATH, QFileInfo(path).dir().path().toStdString());
  if (!path.isEmpty())
    State::LoadAs(m_system, path.toStdString());
}

void MainWindow::StateSave()
{
  QString dialog_path = (Config::Get(Config::MAIN_CURRENT_STATE_PATH).empty()) ?
                            QDir::currentPath() :
                            QString::fromStdString(Config::Get(Config::MAIN_CURRENT_STATE_PATH));
  QString path = DolphinFileDialog::getSaveFileName(
      this, tr("Select a File"), dialog_path, tr("All Save States (*.sav *.s??);; All Files (*)"));
  Config::SetBase(Config::MAIN_CURRENT_STATE_PATH, QFileInfo(path).dir().path().toStdString());
  if (!path.isEmpty())
    State::SaveAs(m_system, path.toStdString());
}

void MainWindow::StateLoadSlot()
{
  State::LoadSelected(m_system);
}

void MainWindow::StateSaveSlot()
{
  State::SaveSelected(m_system);
}

void MainWindow::StateLoadSlotAt(int slot)
{
  State::Load(m_system, slot);
}

void MainWindow::StateLoadLastSavedAt(int slot)
{
  State::LoadLastSaved(m_system, slot);
}

void MainWindow::StateSaveSlotAt(int slot)
{
  State::Save(m_system, slot);
}

void MainWindow::StateLoadUndo()
{
  State::UndoLoadState(m_system);
}

void MainWindow::StateSaveUndo()
{
  State::UndoSaveState(m_system);
}

void MainWindow::StateSaveOldest()
{
  State::SaveFirstSaved(m_system);
}

void MainWindow::SetStateSlot(int slot)
{
  Settings::Instance().SetStateSlot(slot);
  State::SelectSlot(slot);
}

void MainWindow::IncrementSelectedStateSlot()
{
  m_menu_bar->SetStateSlot(State::GetNextSlot());
}

void MainWindow::DecrementSelectedStateSlot()
{
  m_menu_bar->SetStateSlot(State::GetPreviousSlot());
}

void MainWindow::PerformOnlineUpdate(const std::string& region)
{
  WiiUpdate::PerformOnlineUpdate(region, this);
  // Since the update may have installed a newer system menu, trigger a refresh.
  Settings::Instance().NANDRefresh();
}

void MainWindow::BootWiiSystemMenu()
{
  StartGame(std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU}));
}

void MainWindow::NetPlayInit()
{
  const auto& game_list_model = m_game_list->GetGameListModel();
  m_netplay_setup_dialog = new NetPlaySetupDialog(game_list_model, this);
  m_netplay_dialog = new NetPlayDialog(
      game_list_model,
      [this](const std::string& path, std::unique_ptr<BootSessionData> boot_session_data) {
        StartGame(path, ScanForSecondDisc::Yes, std::move(boot_session_data));
      });
#ifdef USE_DISCORD_PRESENCE
  m_netplay_discord = new DiscordHandler(this);
#endif

  connect(m_netplay_dialog, &NetPlayDialog::Stop, this, &MainWindow::ForceStop);
  connect(m_netplay_dialog, &NetPlayDialog::rejected, this, &MainWindow::NetPlayQuit);
  connect(m_netplay_setup_dialog, &NetPlaySetupDialog::Join, this, &MainWindow::NetPlayJoin);
  connect(m_netplay_setup_dialog, &NetPlaySetupDialog::Host, this, &MainWindow::NetPlayHost);
#ifdef USE_DISCORD_PRESENCE
  connect(m_netplay_discord, &DiscordHandler::Join, this, &MainWindow::NetPlayJoin);

  Discord::InitNetPlayFunctionality(*m_netplay_discord);
  m_netplay_discord->Start();
#endif
  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &MainWindow::UpdateScreenSaverInhibition);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &MainWindow::UpdateScreenSaverInhibition);
}

bool MainWindow::NetPlayJoin()
{
  if (!Core::IsUninitialized(m_system))
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("Can't start a NetPlay Session while a game is still running!"));
    return false;
  }

  if (m_netplay_dialog->isVisible())
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("A NetPlay Session is already in progress!"));
    return false;
  }

  auto server = Settings::Instance().GetNetPlayServer();

  // Settings
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";

  std::string host_ip;
  u16 host_port;
  if (server)
  {
    host_ip = "127.0.0.1";
    host_port = server->GetPort();
  }
  else
  {
    host_ip = is_traversal ? Config::Get(Config::NETPLAY_HOST_CODE) :
                             Config::Get(Config::NETPLAY_ADDRESS);
    host_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  }

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
  const std::string network_mode = Config::Get(Config::NETPLAY_NETWORK_MODE);
  const bool host_input_authority = network_mode == "hostinputauthority" || network_mode == "golf";

  if (server)
  {
    server->SetHostInputAuthority(host_input_authority);
    server->AdjustMinimumPadBufferSize(Config::Get(Config::NETPLAY_MINIMUM_BUFFER_SIZE));
  }

  // Create Client
  const bool is_hosting_netplay = server != nullptr;
  Settings::Instance().ResetNetPlayClient(new NetPlay::NetPlayClient(
      host_ip, host_port, m_netplay_dialog, nickname,
      NetPlay::NetTraversalConfig{is_hosting_netplay ? false : is_traversal, traversal_host,
                                  traversal_port}));

  if (!Settings::Instance().GetNetPlayClient()->IsConnected())
  {
    NetPlayQuit();
    return false;
  }

  m_netplay_setup_dialog->close();
  m_netplay_dialog->show(nickname, is_traversal);

  return true;
}

bool MainWindow::NetPlayHost(const UICommon::GameFile& game)
{
  if (!Core::IsUninitialized(m_system))
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("Can't start a NetPlay Session while a game is still running!"));
    return false;
  }

  if (m_netplay_dialog->isVisible())
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("A NetPlay Session is already in progress!"));
    return false;
  }

  // Settings
  u16 host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";
  const bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const u16 traversal_port_alt = Config::Get(Config::NETPLAY_TRAVERSAL_PORT_ALT);

  if (is_traversal)
    host_port = Config::Get(Config::NETPLAY_LISTEN_PORT);

  // Create Server
  Settings::Instance().ResetNetPlayServer(
      new NetPlay::NetPlayServer(host_port, use_upnp, m_netplay_dialog,
                                 NetPlay::NetTraversalConfig{is_traversal, traversal_host,
                                                             traversal_port, traversal_port_alt}));

  if (!Settings::Instance().GetNetPlayServer()->is_connected)
  {
    ModalMessageBox::critical(
        nullptr, tr("Failed to open server"),
        tr("Failed to listen on port %1. Is another instance of the NetPlay server running?")
            .arg(host_port));
    NetPlayQuit();
    return false;
  }

  Settings::Instance().GetNetPlayServer()->ChangeGame(game.GetSyncIdentifier(),
                                                      m_game_list->GetNetPlayName(game));

  // Join our local server
  return NetPlayJoin();
}

void MainWindow::NetPlayQuit()
{
  Settings::Instance().ResetNetPlayClient();
  Settings::Instance().ResetNetPlayServer();
#ifdef USE_DISCORD_PRESENCE
  Discord::UpdateDiscordPresence();
#endif
}

void MainWindow::UpdateScreenSaverInhibition()
{
  const bool inhibit = Config::Get(Config::MAIN_DISABLE_SCREENSAVER) &&
                       (Core::GetState(m_system) == Core::State::Running);

  if (inhibit == m_is_screensaver_inhibited)
    return;

  m_is_screensaver_inhibited = inhibit;

  UICommon::InhibitScreenSaver(inhibit);
}

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::Close)
  {
    if (m_standalone)
    {
      StopStandalone(object == this ? StandaloneStop::Exit : StandaloneStop::Desktop);
      static_cast<QCloseEvent*>(event)->ignore();
      return true;
    }
    m_exit_requested = object == this;
    if (!RequestStop())
      m_exit_requested = false;

    static_cast<QCloseEvent*>(event)->ignore();
    return true;
  }

  return false;
}

QMenu* MainWindow::createPopupMenu()
{
  // Disable the default popup menu as it exposes the debugger UI even when the debugger UI is
  // disabled, which can lead to user confusion (see e.g. https://bugs.dolphin-emu.org/issues/13306)
  return nullptr;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
  const QList<QUrl>& urls = event->mimeData()->urls();
  if (urls.empty())
    return;

  QStringList files;
  QStringList folders;

  for (const QUrl& url : urls)
  {
    QFileInfo file_info(url.toLocalFile());
    QString path = file_info.filePath();

    if (!file_info.exists() || !file_info.isReadable())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("Failed to open '%1'").arg(path));
      return;
    }

    (file_info.isFile() ? files : folders).append(path);
  }

  if (!files.isEmpty())
  {
    StartGame(StringListToStdVector(files));
  }
  else
  {
    Settings& settings = Settings::Instance();
    const bool show_confirm = !settings.GetPaths().empty();

    for (const QString& folder : folders)
    {
      if (show_confirm)
      {
        if (ModalMessageBox::question(
                this, tr("Confirm"),
                tr("Do you want to add \"%1\" to the list of Game Paths?").arg(folder)) !=
            QMessageBox::Yes)
          return;
      }
      settings.AddPath(folder);
    }
  }
}

QSize MainWindow::sizeHint() const
{
  if (!m_game_list)
    return QSize(800, 600);

  QSize size = m_game_list->GetSizeForGrid(DEFAULT_GRID_COLUMNS, DEFAULT_GRID_ROWS);

  if (!m_ui->toolbar->isHidden())
  {
    size.setWidth(std::max(size.width(), m_ui->toolbar->sizeHint().width()));
    size.rheight() += m_ui->toolbar->sizeHint().height();
  }
  if (!m_ui->menubar->isNativeMenuBar() && !m_ui->menubar->isHidden())
    size.rheight() += m_ui->menubar->sizeHint().height();
  if (!m_ui->statusBar->isHidden())
    size.rheight() += m_ui->statusBar->sizeHint().height();

  return size;
}

void MainWindow::OnBootGameCubeIPL(DiscIO::Region region)
{
  StartGame(std::make_unique<BootParameters>(BootParameters::IPL{region}));
}

void MainWindow::OnImportNANDBackup()
{
  auto response = ModalMessageBox::question(
      this, tr("Question"),
      tr("Merging a new NAND over your currently selected NAND will overwrite any channels "
         "and savegames that already exist. This process is not reversible, so it is "
         "recommended that you keep backups of both NANDs. Are you sure you want to "
         "continue?"));

  if (response == QMessageBox::No)
    return;

  QString file =
      DolphinFileDialog::getOpenFileName(this, tr("Select NAND Backup"), QDir::currentPath(),
                                         tr("BootMii NAND backup file (*.bin);;"
                                            "All Files (*)"));

  if (file.isEmpty())
    return;

  ParallelProgressDialog dialog(this);
  dialog.GetRaw()->setWindowTitle(tr("Importing NAND backup"));

  std::future<void> result = std::async(std::launch::async, [&] {
    DiscIO::NANDImporter().ImportNANDBin(
        file.toStdString(),
        [&dialog](DiscIO::NANDImporter::Step step, u32 cur, u32 max) {
          switch (step)
          {
          case DiscIO::NANDImporter::Step::Loading:
            dialog.SetLabelText(tr("Loading NAND..."));
            break;
          case DiscIO::NANDImporter::Step::Extracting:
            dialog.SetLabelText(tr("Extracting NAND..."));
            break;
          }
          dialog.SetValue(cur);
          dialog.SetMaximum(max);
          return dialog.WasCanceled();
        },
        [this] {
          std::optional<std::string> keys_file = RunOnObject(this, [this] {
            return DolphinFileDialog::getOpenFileName(
                       this, tr("Select Keys File (OTP/SEEPROM Dump)"), QDir::currentPath(),
                       tr("BootMii keys file (*.bin);;"
                          "All Files (*)"))
                .toStdString();
          });
          if (keys_file)
            return *keys_file;
          return std::string("");
        });
    dialog.Reset();
  });

  dialog.GetRaw()->exec();

  result.wait();

  m_menu_bar->UpdateToolsMenu(Core::State::Uninitialized);
}

void MainWindow::OnPlayRecording()
{
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
  {
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("Playback of input recordings is disabled in RetroAchievements hardcore mode."));
    return;
  }

  QString dtm_file = DolphinFileDialog::getOpenFileName(
      this, tr("Select the Recording File to Play"), QString(), tr("Dolphin TAS Movies (*.dtm)"));

  if (dtm_file.isEmpty())
    return;

  auto& movie = m_system.GetMovie();
  if (!movie.IsReadOnly())
  {
    // let's make the read-only flag consistent at the start of a movie.
    movie.SetReadOnly(true);
    emit ReadOnlyModeChanged(true);
  }

  std::optional<std::string> savestate_path;
  if (movie.PlayInput(dtm_file.toStdString(), &savestate_path))
  {
    emit RecordingStatusChanged(true);

    Play(savestate_path);
  }
}

void MainWindow::OnStartRecording()
{
  auto& movie = m_system.GetMovie();
  if (Core::GetState(m_system) == Core::State::Starting ||
      Core::GetState(m_system) == Core::State::Stopping || movie.IsRecordingInput() ||
      movie.IsPlayingInput())
  {
    return;
  }

  if (movie.IsReadOnly())
  {
    // The user just chose to record a movie, so that should take precedence
    movie.SetReadOnly(false);
    emit ReadOnlyModeChanged(true);
  }

  Movie::ControllerTypeArray controllers{};
  Movie::WiimoteEnabledArray wiimotes{};

  for (int i = 0; i < 4; i++)
  {
    const SerialInterface::SIDevices si_device = Config::Get(Config::GetInfoForSIDevice(i));
    if (si_device == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
      controllers[i] = Movie::ControllerType::GBA;
    else if (SerialInterface::SIDevice_IsGCController(si_device))
      controllers[i] = Movie::ControllerType::GC;
    else
      controllers[i] = Movie::ControllerType::None;
    wiimotes[i] = Config::Get(Config::GetInfoForWiimoteSource(i)) != WiimoteSource::None;
  }

  if (movie.BeginRecordingInput(controllers, wiimotes))
  {
    emit RecordingStatusChanged(true);

    if (Core::IsUninitialized(m_system))
      Play();
  }
}

void MainWindow::OnStopRecording()
{
  auto& movie = m_system.GetMovie();
  if (movie.IsRecordingInput())
    OnExportRecording();
  if (movie.IsMovieActive())
    movie.EndPlayInput(false);
  emit RecordingStatusChanged(false);
}

void MainWindow::OnExportRecording()
{
  const Core::CPUThreadGuard guard(m_system);

  QString dtm_file = DolphinFileDialog::getSaveFileName(
      this, tr("Save Recording File As"), QString(), tr("Dolphin TAS Movies (*.dtm)"));
  if (!dtm_file.isEmpty())
    m_system.GetMovie().SaveRecording(dtm_file.toStdString());
}

void MainWindow::OnActivateChat()
{
  if (const auto chat = GetNetPlayChatUI())
    chat->Activate();
}

void MainWindow::OnCollapseChat()
{
  if (const auto chat = GetNetPlayChatUI())
    chat->Collapse();
}

void MainWindow::OnExpandChat()
{
  if (const auto chat = GetNetPlayChatUI())
    chat->Expand();
}

void MainWindow::OnRequestGolfControl()
{
  auto client = Settings::Instance().GetNetPlayClient();
  if (client)
    client->RequestGolfControl();
}

void MainWindow::ShowTASInput()
{
  for (int i = 0; i < num_gc_controllers; i++)
  {
    const auto si_device = Config::Get(Config::GetInfoForSIDevice(i));
    if (si_device == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
    {
      m_gba_tas_input_windows[i]->show();
      m_gba_tas_input_windows[i]->raise();
      m_gba_tas_input_windows[i]->activateWindow();
    }
    else if (si_device != SerialInterface::SIDEVICE_NONE &&
             si_device != SerialInterface::SIDEVICE_GC_GBA)
    {
      m_gc_tas_input_windows[i]->show();
      m_gc_tas_input_windows[i]->raise();
      m_gc_tas_input_windows[i]->activateWindow();
    }
  }

  for (int i = 0; i < num_wii_controllers; i++)
  {
    if (Config::Get(Config::GetInfoForWiimoteSource(i)) == WiimoteSource::Emulated &&
        (!Core::IsRunning(m_system) || m_system.IsWii()))
    {
      m_wii_tas_input_windows[i]->show();
      m_wii_tas_input_windows[i]->raise();
      m_wii_tas_input_windows[i]->activateWindow();
    }
  }
}

void MainWindow::OnConnectWiiRemote(int id)
{
  const Core::CPUThreadGuard guard(m_system);
  if (const auto bt = WiiUtils::GetBluetoothEmuDevice())
  {
    const auto wm = bt->AccessWiimoteByIndex(id);
    wm->Activate(!wm->IsConnected());
  }
}

#ifdef USE_RETRO_ACHIEVEMENTS
void MainWindow::ShowAchievementsWindow()
{
  if (!m_achievements_window)
  {
    m_achievements_window = new AchievementsWindow(this);
  }

  m_achievements_window->show();
  m_achievements_window->raise();
  m_achievements_window->activateWindow();
  m_achievements_window->UpdateData(AchievementManager::UpdatedItems{.all = true});
}

void MainWindow::ShowAchievementSettings()
{
  ShowAchievementsWindow();
  m_achievements_window->ForceSettingsTab();
}

void MainWindow::OnHardcoreChanged()
{
  bool hardcore_active = AchievementManager::GetInstance().IsHardcoreModeActive();
  // EmulationStateChanged causes several dialogs to redraw, including anything affected by hardcore
  // mode. Every dialog that depends on hardcore mode is redrawn by EmulationStateChanged.
  if (hardcore_active != m_former_hardcore_setting)
    emit Settings::Instance().EmulationStateChanged(Core::GetState(Core::System::GetInstance()));
  m_former_hardcore_setting = hardcore_active;
}
#endif  // USE_RETRO_ACHIEVEMENTS

void MainWindow::ShowMemcardManager()
{
  GCMemcardManager manager(this);

  manager.exec();
}

void MainWindow::ShowResourcePackManager()
{
  ResourcePackManager manager(this);

  manager.exec();
}

void MainWindow::ShowCheatsManager()
{
  if (!m_cheats_manager)
  {
    m_cheats_manager = new CheatsManager(m_system, this);

    connect(m_cheats_manager, &CheatsManager::ShowMemory, m_memory_widget,
            &MemoryWidget::SetAddress);
    connect(m_cheats_manager, &CheatsManager::RequestWatch, m_watch_widget, &WatchWidget::AddWatch);
    connect(m_cheats_manager, &CheatsManager::OpenGeneralSettings, this,
            &MainWindow::ShowGeneralWindow);

#ifdef USE_RETRO_ACHIEVEMENTS
    connect(m_cheats_manager, &CheatsManager::OpenAchievementSettings, this,
            &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS
  }

  m_cheats_manager->show();
}

void MainWindow::ShowRiivolutionBootWidget(const UICommon::GameFile& game)
{
  auto second_game = m_game_list->FindSecondDisc(game);
  std::vector<std::string> paths = {game.GetFilePath()};
  if (second_game != nullptr)
    paths.push_back(second_game->GetFilePath());
  std::unique_ptr<BootParameters> boot_params = BootParameters::GenerateFromFile(paths);
  if (!boot_params)
    return;
  if (!std::holds_alternative<BootParameters::Disc>(boot_params->parameters))
    return;

  auto& disc = std::get<BootParameters::Disc>(boot_params->parameters);
  RiivolutionBootWidget w(disc.volume->GetGameID(), disc.volume->GetRevision(),
                          disc.volume->GetDiscNumber(), game.GetFilePath(), this);

#ifdef USE_RETRO_ACHIEVEMENTS
  connect(&w, &RiivolutionBootWidget::OpenAchievementSettings, this,
          &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  w.exec();
  if (!w.ShouldBoot())
    return;

  AddRiivolutionPatches(boot_params.get(), std::move(w.GetPatches()));
  StartGame(std::move(boot_params));
}
