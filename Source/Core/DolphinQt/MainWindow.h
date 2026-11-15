// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QStringList>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "Common/BigPicture/Contracts.h"
#include "Common/HookableEvent.h"
#include "Common/WorkQueueThread.h"
#include "DolphinQt/BigPictureAchievements.h"
#include "DolphinQt/BigPictureGraphicsConfig.h"
#include "DolphinQt/BigPictureSettings.h"

#ifdef USE_RETRO_ACHIEVEMENTS
#include "Common/Config/Config.h"
#endif  // USE_RETRO_ACHIEVEMENTS

#include "Core/Boot/Boot.h"
#include "DolphinQt/FIFO/FIFOPlayerWindow.h"

class QMenu;
class QStackedWidget;
class QString;
class QTimer;

namespace State
{
struct OperationResult;
}

class AchievementsWindow;
class AssemblerWidget;
class BreakpointWidget;
class BigPictureHost;
class BigPictureLibrary;
class BigPictureTextInput;
struct BootParameters;
class CheatsManager;
class CodeWidget;
class DiscordHandler;
class DragEnterEvent;
class EmulationStatusWidget;
class FreeLookWindow;
class GameList;
class GBATASInputWindow;
class GCTASInputWindow;
class HotkeyScheduler;
class InfinityBaseWindow;
class JITWidget;
class LogConfigWidget;
class LogWidget;
class MappingWindow;
class MemoryWidget;
class MenuBar;
class NetPlayDialog;
class NetPlaySetupDialog;
class NetworkWidget;
class RegisterWidget;
class RenderWidget;
class SettingsWindow;
class SkylanderPortalWindow;
class ThreadWidget;
class ToolBar;
class WatchWidget;
class WiiTASInputWindow;
class WiiSpeakWindow;
class LogitechMicWindow;
struct WindowSystemInfo;

namespace Core
{
class System;
}

namespace Ui
{
class MainWindow;
}

namespace DiscIO
{
enum class Region;
}

namespace UICommon
{
class GameFile;
}

namespace VideoCommon::BigPicture
{
class StandaloneSession;
}

namespace X11Utils
{
class XRRConfiguration;
}

class MainWindow final : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(Core::System& system, std::unique_ptr<BootParameters> boot_parameters,
                      const std::string& movie_path, bool start_big_picture = false);
  ~MainWindow() override;

  WindowSystemInfo GetWindowSystemInfo() const;

  bool eventFilter(QObject* object, QEvent* event) override;
  QMenu* createPopupMenu() override;

  void ShowControllersWindow();
  void ShowTriforceWindow();
  BigPictureHost* GetBigPictureHost() const { return m_big_picture_host; }
  void EnterBigPicture();

signals:
  void ReadOnlyModeChanged(bool read_only);
  void RecordingStatusChanged(bool recording);

private:
  void Open();
  void RefreshGameList();
  void Play(const std::optional<std::string>& savestate_path = {});
  void Pause();
  void TogglePause();

  // May ask for confirmation. Returns whether or not it actually stopped.
  bool RequestStop();
  bool RequestStopWithConfirmation(bool confirmed, bool save_continuation = false);
  bool CanSaveContinuation() const;
  void ForceStop();
  void Reset();
  void FrameAdvance();
  void StateLoad();
  void StateSave();
  void StateLoadSlot();
  void StateSaveSlot();
  void StateLoadSlotAt(int slot);
  void StateSaveSlotAt(int slot);
  void StateLoadLastSavedAt(int slot);
  void StateLoadUndo();
  void StateSaveUndo();
  void StateSaveOldest();
  void SetStateSlot(int slot);
  void IncrementSelectedStateSlot();
  void DecrementSelectedStateSlot();
  void BootWiiSystemMenu();

  void PerformOnlineUpdate(const std::string& region);

  void SetFullScreenResolution(bool fullscreen);

  void FullScreen();
  void FocusRenderWidget();
  void UnlockCursor();
  void ScreenShot();

  void CreateComponents();

  void ConnectGameList();
  void ConnectHost();
  void ConnectHotkeys();
  void ConnectMenuBar();
  void ConnectRenderWidget();
  void ConnectStack();
  void ConnectToolBar();
  void UpdateStatusBar();

  void InitControllers();
  void ShutdownControllers();

  void InitCoreCallbacks();
  void RefreshBigPictureStateSlots();
  // Reads a game's slot headers and previews off the UI thread; the browser shows its loading
  // text until they arrive.
  void RequestBigPictureStateSlots(std::string game_id);
  void ClearBigPictureStateSlots();
  // Big Picture save and load commands stay pending until the core reports their outcome.
  void BeginBigPictureStateRequest(std::uint64_t command_id, bool save, int slot);
  void OnBigPictureStateResult(const State::OperationResult& result);
  void RefreshBigPictureLibrary();
  void PrepareBigPictureSurface();
  bool LeaveBigPictureForDesktop(std::uint64_t generation);
  enum class StandaloneStop
  {
    None,
    Desktop,
    Boot,
    Restart,
    Exit,
    OpenGame,
    Settings
  };
  void StopStandalone(StandaloneStop next);
  BigPicture::HostState ReadBigPictureState();
  BigPicture::Result ExecuteBigPictureCommand(const BigPicture::Command& command);
  void ClearContinuationBoot();
  void PollStandalone();
  void CompleteStandaloneStop(StandaloneStop next, bool failed);

  enum class ScanForSecondDisc
  {
    Yes,
    No,
  };

  void ScanForSecondDiscAndStartGame(const UICommon::GameFile& game,
                                     std::unique_ptr<BootSessionData> boot_session_data = nullptr);
  std::optional<std::string> ChooseLibraryDisc(const std::string& path);
  void StartGame(const QString& path, ScanForSecondDisc scan,
                 std::unique_ptr<BootSessionData> boot_session_data = nullptr);
  void StartGame(const std::string& path, ScanForSecondDisc scan,
                 std::unique_ptr<BootSessionData> boot_session_data = nullptr);
  void StartGame(const std::vector<std::string>& paths,
                 std::unique_ptr<BootSessionData> boot_session_data = nullptr);
  void StartGame(std::unique_ptr<BootParameters>&& parameters, bool continuation_checked = false);
  void ShowRenderWidget(bool preserve_window = false);
  void HideRenderWidget(bool reinit = true, bool is_exit = false);

  void ShowSettingsWindow();
  void ShowGeneralWindow();
  void ShowAudioWindow();
  void ShowGraphicsWindow();
  void ShowCloudSavesWindow();
  void ShowFreeLookWindow();
  void ShowAboutDialog();
  #ifdef SHOW_UPDATER
  void ShowUpdateDialog();
  void CheckForUpdatesAuto();
#endif  // SHOW_UPDATER
  void ShowHotkeyDialog();
  void ShowBigPictureInputDialog(int controller);
  void ShowNetPlaySetupDialog();
  void ShowNetPlayBrowser();
  void ShowFIFOPlayer();
  void ShowSkylanderPortal();
  void ShowInfinityBase();
  void ShowWiiSpeakWindow();
  void ShowLogitechMicWindow();
  void ShowMemcardManager();
  void ShowResourcePackManager();
  void ShowCheatsManager();
  void ShowRiivolutionBootWidget(const UICommon::GameFile& game);

#ifdef USE_RETRO_ACHIEVEMENTS
  void ShowAchievementsWindow();
  void ShowAchievementSettings();
  void OnHardcoreChanged();
#endif  // USE_RETRO_ACHIEVEMENTS

  void NetPlayInit();
  bool NetPlayJoin();
  bool NetPlayHost(const UICommon::GameFile& game);
  void NetPlayQuit();

  void OnBootGameCubeIPL(DiscIO::Region region);
  void OnImportNANDBackup();
  void OnConnectWiiRemote(int id);

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
  void OnSignal();
#endif

  void OnPlayRecording();
  void OnStartRecording();
  void OnStopRecording();
  void OnExportRecording();
  void OnActivateChat();
  void OnCollapseChat();
  void OnExpandChat();
  void OnRequestGolfControl();
  void ShowTASInput();
  void ShowOSDWindow();

  void ChangeDisc();
  void EjectDisc();

  void OpenUserFolder();
  void OpenConfigFolder();
  void OpenCacheFolder();

  QStringList PromptFileNames();

  void UpdateScreenSaverInhibition();

  void OnStopComplete();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  QSize sizeHint() const override;

  Core::System& m_system;
  std::unique_ptr<Ui::MainWindow> m_ui;
  BigPictureHost* m_big_picture_host = nullptr;
  BigPictureAchievements m_big_picture_achievements;
  std::shared_ptr<const BigPicture::LibraryImage> m_big_picture_avatar;
  std::optional<BigPictureSettings::Game> m_big_picture_properties_game;
  std::unique_ptr<BigPictureSettings> m_big_picture_settings;
  bool m_settings_post_processing = false;
  BigPictureLibrary* m_big_picture_library = nullptr;
  // The game files last published to Big Picture, used to detect actual cover changes.
  std::map<std::string, std::shared_ptr<const UICommon::GameFile>> m_big_picture_cover_sources;
  bool m_big_picture_refresh_queued = false;
  QPointer<BigPictureTextInput> m_big_picture_text_input;
  std::uint64_t m_big_picture_text_generation = 0;
  bool m_big_picture_text_queued = false;
  QTimer* m_library_timer = nullptr;
  bool m_big_picture_system_menu_available = false;
  std::array<BigPicture::StateSlot, BigPicture::STATE_SLOT_COUNT> m_big_picture_state_slots;
  bool m_big_picture_state_slots_ready = false;
  Common::EventHook m_state_save_hook;
  Common::EventHook m_state_result_hook;
  struct StateSlotJob
  {
    std::uint64_t revision = 0;
    std::string game_id;
  };
  std::uint64_t m_big_picture_state_slots_revision = 0;
  Common::WorkQueueThread<StateSlotJob> m_state_slot_worker;
  struct BigPictureStateRequest
  {
    std::uint64_t command_id = 0;
    bool save = false;
    std::string filename;
  };
  std::optional<BigPictureStateRequest> m_big_picture_state_request;
  bool m_big_picture_disc_game = false;
  std::string m_big_picture_disc_path;
  QElapsedTimer m_big_picture_play_timer;
  qint64 m_big_picture_session_ms = 0;
  qint64 m_big_picture_previous_play_ms = 0;
  bool m_big_picture_play_time_started = false;
  void ShowBigPictureTextInput(std::uint64_t generation);
  std::unique_ptr<VideoCommon::BigPicture::StandaloneSession> m_standalone;
  QTimer* m_standalone_timer = nullptr;
  StandaloneStop m_standalone_stop = StandaloneStop::None;
  bool m_big_picture_requested = false;
  bool m_standalone_ready = false;
  bool m_standalone_restart_queued = false;
  Config::LayerMap m_standalone_graphics_config;
  Config::LayerMap m_big_picture_surface_config;
  bool m_shutting_down = false;
  std::uint64_t m_standalone_serial = 0;
  std::unique_ptr<BootParameters> m_standalone_boot;
  bool m_standalone_continuation_checked = false;
  std::unique_ptr<BootParameters> m_continuation_boot;
  std::string m_continuation_path;
  std::string m_continuation_boot_path;

#ifdef HAVE_XRANDR
  std::unique_ptr<X11Utils::XRRConfiguration> m_xrr_config;
#endif

  QStackedWidget* m_stack;
  ToolBar* m_tool_bar;
  MenuBar* m_menu_bar;
  GameList* m_game_list = nullptr;
  EmulationStatusWidget* m_emulation_status;
  RenderWidget* m_render_widget = nullptr;
  int m_total_games = 0;
  int m_visible_games = 0;
  bool m_rendering_to_main;
  QTimer* m_graceful_shutdown_timer = nullptr;
  bool m_stop_confirm_showing = false;
  bool m_stop_requested = false;
  bool m_exit_requested = false;
  bool m_fullscreen_requested = false;
  bool m_is_screensaver_inhibited = false;
  std::unique_ptr<BootParameters> m_pending_boot;

  SettingsWindow* m_settings_window = nullptr;
  // m_fifo_window doesn't set MainWindow as its parent so that the fifo can be focused without
  // raising the main window, so use a unique_ptr to make sure it gets destroyed.
  std::unique_ptr<FIFOPlayerWindow> m_fifo_window = nullptr;
  SkylanderPortalWindow* m_skylander_window = nullptr;
  InfinityBaseWindow* m_infinity_window = nullptr;
  WiiSpeakWindow* m_wii_speak_window = nullptr;
  LogitechMicWindow* m_logitech_mic_window = nullptr;
  MappingWindow* m_hotkey_window = nullptr;
  std::array<MappingWindow*, 4> m_big_picture_input_windows{};
  FreeLookWindow* m_freelook_window = nullptr;

  HotkeyScheduler* m_hotkey_scheduler;
  NetPlayDialog* m_netplay_dialog;
  DiscordHandler* m_netplay_discord;
  NetPlaySetupDialog* m_netplay_setup_dialog;
  static constexpr int num_gc_controllers = 4;
  std::array<GCTASInputWindow*, num_gc_controllers> m_gc_tas_input_windows{};
  std::array<GBATASInputWindow*, num_gc_controllers> m_gba_tas_input_windows{};
  static constexpr int num_wii_controllers = 4;
  std::array<WiiTASInputWindow*, num_wii_controllers> m_wii_tas_input_windows{};

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementsWindow* m_achievements_window = nullptr;
  Config::ConfigChangedCallbackID m_config_changed_callback_id;
  bool m_former_hardcore_setting = false;
#endif  // USE_RETRO_ACHIEVEMENTS

  AssemblerWidget* m_assembler_widget;
  BreakpointWidget* m_breakpoint_widget;
  CodeWidget* m_code_widget;
  JITWidget* m_jit_widget;
  LogWidget* m_log_widget;
  LogConfigWidget* m_log_config_widget;
  MemoryWidget* m_memory_widget;
  NetworkWidget* m_network_widget;
  RegisterWidget* m_register_widget;
  ThreadWidget* m_thread_widget;
  WatchWidget* m_watch_widget;
  CheatsManager* m_cheats_manager{};
  QByteArray m_render_widget_geometry;
};
