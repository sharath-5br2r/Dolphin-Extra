// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings.h"

#include <atomic>
#include <memory>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPalette>
#include <QPixmapCache>
#include <QRadioButton>
#include <QSize>
#include <QStyle>
#include <QStyleHints>
#include <QThread>
#include <QWidget>

#include "AudioCommon/AudioCommon.h"

#include "Common/Config/Config.h"
#include "Common/Contains.h"
#include "Common/FileUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/IOS.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ToolTipStyle.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"

static std::unique_ptr<QPalette> s_default_palette;

Settings::Settings()
{
  qRegisterMetaType<Core::State>();
  m_core_state_changed_hook = Core::AddOnStateChangedCallback([this](Core::State new_state) {
    QueueOnObject(this, [this, new_state] {
      // Avoid signal spam while continuously frame stepping. Will still send a signal for the first
      // and last framestep.
      if (!m_continuously_frame_stepping)
        emit EmulationStateChanged(new_state);
    });
  });

  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] {
    static std::atomic<bool> do_once{true};
    if (do_once.exchange(false))
    {
      // Calling ConfigChanged() with a "delay" can have risks, for example, if from
      // code we change some configs that result in Qt greying out some setting, we could
      // end up editing that setting before its greyed out, sending out an event,
      // which might not be expected or handled by the code, potentially crashing.
      // The only safe option would be to wait on the Qt thread to have finished executing this.
      QueueOnObject(this, [this] {
        do_once = true;
        emit ConfigChanged();
      });
    }
  });

  // Drive config-bound widgets from the same coalesced, GUI-thread-marshalled signal the rest of
  // DolphinQt uses, so the binder library needs no dependency on Settings.
  connect(this, &Settings::ConfigChanged, &ConfigWidget::ConfigChangeBroadcaster::Instance(),
          &ConfigWidget::ConfigChangeBroadcaster::Broadcast);

  m_hotplug_event_hook = g_controller_interface.RegisterDevicesChangedCallback([this] {
    if (qApp->thread() == QThread::currentThread())
    {
      emit DevicesChanged();
    }
    else
    {
      // Any device shared_ptr in the host thread needs to be released immediately as otherwise
      // they'd continue living until the queued event has run, but some devices can't be recreated
      // until they are destroyed.
      // This is safe from any thread. Devices will be refreshed and re-acquired and in
      // DevicesChanged(). Calling it without queueing shouldn't cause any deadlocks but is slow.
      emit ReleaseDevices();

      QueueOnObject(this, [this] { emit DevicesChanged(); });
    }
  });

  connect(this, &Settings::HardcoreModeChanged, this, [this](bool hardcore) {
    if (hardcore)
      emit DebugModeToggled(false);
    else if (Config::IsDebuggingEnabled())
      emit DebugModeToggled(true);
  });
}

Settings::~Settings()
{
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
}

void Settings::UnregisterDevicesChangedCallback()
{
  m_hotplug_event_hook.reset();
}

Settings& Settings::Instance()
{
  static Settings settings;
  return settings;
}

QSettings& Settings::GetQSettings()
{
  static QSettings settings(
      QStringLiteral("%1/Qt.ini").arg(QString::fromStdString(File::GetUserPath(D_CONFIG_IDX))),
      QSettings::IniFormat);
  return settings;
}

void Settings::TriggerThemeChanged()
{
  emit ThemeChanged();
}

QString Settings::GetUserStyleName() const
{
  if (GetQSettings().contains(QStringLiteral("userstyle/name")))
    return GetQSettings().value(QStringLiteral("userstyle/name")).toString();

  // Migration code for the old way of storing this setting
  return QFileInfo(GetQSettings().value(QStringLiteral("userstyle/path")).toString()).fileName();
}

void Settings::SetUserStyleName(const QString& stylesheet_name)
{
  GetQSettings().setValue(QStringLiteral("userstyle/name"), stylesheet_name);
}

void Settings::InitDefaultPalette()
{
  s_default_palette = std::make_unique<QPalette>(qApp->palette());
}

bool Settings::IsSystemDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
  return false;
#endif
}

bool Settings::IsThemeDark()
{
  return ApplicationTheme::IsDark(qApp->palette());
}

// Calling this before the main window has been created breaks the style of some widgets.
void Settings::ApplyStyle()
{
  const StyleType style_type = GetStyleType();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Clear a previously forced scheme before asking the platform what System currently means.
  if (style_type == StyleType::System || style_type == StyleType::User)
    qApp->styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif

  bool use_system_dark_fallback = false;
#ifdef _WIN32
  use_system_dark_fallback = style_type == StyleType::System && IsSystemDark();
#endif
  const ApplicationTheme::Definition definition =
      ApplicationTheme::GetDefinition(style_type, use_system_dark_fallback);

  static const QString s_initial_style_name{QApplication::style()->name()};
  const QString style_name{definition.use_fusion ? QStringLiteral("fusion") : s_initial_style_name};
  if (QApplication::style()->name() != style_name)
    QApplication::setStyle(style_name);

  const QString stylesheet_name = GetUserStyleName();
  QString stylesheet_contents;

  // If we haven't found one, we continue with an empty (default) style
  if (!stylesheet_name.isEmpty() && style_type == StyleType::User)
  {
    // Load custom user stylesheet
    QDir directory = QDir(QString::fromStdString(File::GetUserPath(D_STYLES_IDX)));
    QFile stylesheet(directory.filePath(stylesheet_name));

    if (stylesheet.open(QFile::ReadOnly))
      stylesheet_contents = QString::fromUtf8(stylesheet.readAll().data());
  }

  QPalette palette = s_default_palette ? *s_default_palette : qApp->style()->standardPalette();
  if (definition.palette)
    palette = *definition.palette;

  qApp->setPalette(palette);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  if (definition.color_scheme != Qt::ColorScheme::Unknown)
    qApp->styleHints()->setColorScheme(definition.color_scheme);
#endif

  // Define tooltips style if not already defined
  if (!stylesheet_contents.contains(QStringLiteral("QToolTip"), Qt::CaseSensitive))
  {
    QColor window_color;
    QColor text_color;
    QColor unused_text_emphasis_color;
    QColor border_color;
    GetToolTipStyle(window_color, text_color, unused_text_emphasis_color, border_color, palette,
                    palette);

    const int padding{QFontMetrics(QFont()).height() / 2};

    const auto tooltip_stylesheet =
        QStringLiteral("QToolTip { background-color: #%1; color: #%2; padding: %3px; "
                       "border: 1px; border-style: solid; border-color: #%4; }")
            .arg(window_color.rgba(), 0, 16)
            .arg(text_color.rgba(), 0, 16)
            .arg(padding)
            .arg(border_color.rgba(), 0, 16);
    stylesheet_contents.append(QStringLiteral("%1").arg(tooltip_stylesheet));
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  // For Fusion, define group box style if not already defined.
  if (style_name.compare(QStringLiteral("fusion"), Qt::CaseInsensitive) == 0 &&
      !stylesheet_contents.contains(QStringLiteral("QGroupBox")))
  {
    stylesheet_contents.append(QStringLiteral("QGroupBox {"
                                              " margin-top: 0.6em;"
                                              " padding-top: 0.5em;"
                                              " padding-bottom: 0;"
                                              " padding-left: 1px;"
                                              " padding-right: 1px;"
                                              "} "
                                              "QGroupBox::title {"
                                              " subcontrol-origin: margin;"
                                              " subcontrol-position: top left;"
                                              " left: 0.7em;"
                                              " padding-top: 1px;"
                                              " min-width: 0;"
                                              "}"));
  }
#endif

  qApp->setStyleSheet(stylesheet_contents);
  QPixmapCache::clear();
}

Settings::StyleType Settings::GetStyleType() const
{
  if (GetQSettings().contains(QStringLiteral("userstyle/styletype")))
  {
    bool ok = false;
    const int type_int = GetQSettings().value(QStringLiteral("userstyle/styletype")).toInt(&ok);
    if (ok && type_int >= static_cast<int>(StyleType::MinValue) &&
        type_int <= static_cast<int>(StyleType::MaxValue))
    {
      return ApplicationTheme::Canonicalize(static_cast<StyleType>(type_int));
    }
  }

  // If the style type is unset or invalid, try the old enabled flag before using the current
  // default. This keeps preferences written by older Dolphin builds meaningful.
  if (GetQSettings().contains(QStringLiteral("userstyle/enabled")))
  {
    return GetQSettings().value(QStringLiteral("userstyle/enabled")).toBool() ? StyleType::User :
                                                                                StyleType::System;
  }

  return ApplicationTheme::DEFAULT_TYPE;
}

void Settings::SetStyleType(StyleType type)
{
  type = ApplicationTheme::Canonicalize(type);
  GetQSettings().setValue(QStringLiteral("userstyle/styletype"), static_cast<int>(type));

  // also set the old setting so that the config is correctly interpreted by older Dolphin builds
  GetQSettings().setValue(QStringLiteral("userstyle/enabled"), type == StyleType::User);
}

void Settings::GetToolTipStyle(QColor& window_color, QColor& text_color,
                               QColor& emphasis_text_color, QColor& border_color,
                               const QPalette& palette, const QPalette& high_contrast_palette) const
{
  ToolTipStyle::GetToolTipStyle(window_color, text_color, emphasis_text_color, border_color,
                                palette, high_contrast_palette);
}

QStringList Settings::GetPaths() const
{
  QStringList list;
  for (const auto& path : Config::GetIsoPaths())
    list << QString::fromStdString(path);
  return list;
}

void Settings::AddPath(const QString& qpath)
{
  std::string path = qpath.toStdString();
  std::vector<std::string> paths = Config::GetIsoPaths();

  if (Common::Contains(paths, path))
    return;

  paths.emplace_back(path);
  Config::SetIsoPaths(paths);
  emit PathAdded(qpath);
}

void Settings::RemovePath(const QString& qpath)
{
  std::string path = qpath.toStdString();
  std::vector<std::string> paths = Config::GetIsoPaths();

  if (std::erase(paths, path) == 0)
    return;

  Config::SetIsoPaths(paths);
  emit PathRemoved(qpath);
}

void Settings::RefreshGameList()
{
  emit GameListRefreshRequested();
}

void Settings::RefreshMetadata()
{
  emit MetadataRefreshRequested();
}

void Settings::NotifyRefreshGameListStarted()
{
  emit GameListRefreshStarted();
}

void Settings::NotifyRefreshGameListComplete()
{
  emit GameListRefreshCompleted();
}

void Settings::NotifyMetadataRefreshComplete()
{
  emit MetadataRefreshCompleted();
}

void Settings::ReloadTitleDB()
{
  emit TitleDBReloadRequested();
}

bool Settings::IsAutoRefreshEnabled() const
{
  return GetQSettings().value(QStringLiteral("gamelist/autorefresh"), true).toBool();
}

void Settings::SetAutoRefreshEnabled(bool enabled)
{
  if (IsAutoRefreshEnabled() == enabled)
    return;

  GetQSettings().setValue(QStringLiteral("gamelist/autorefresh"), enabled);

  emit AutoRefreshToggled(enabled);
}

bool Settings::IsGameCountVisible() const
{
  return GetQSettings().value(QStringLiteral("GameCount/Visible"), true).toBool();
}

void Settings::SetGameCountVisible(const bool visible)
{
  if (IsGameCountVisible() == visible)
    return;

  GetQSettings().setValue(QStringLiteral("GameCount/Visible"), visible);

  emit GameCountVisibilityChanged(visible);
}

QString Settings::GetDefaultGame() const
{
  return QString::fromStdString(Config::Get(Config::MAIN_DEFAULT_ISO));
}

void Settings::SetDefaultGame(const QString& path)
{
  if (GetDefaultGame() != path)
  {
    Config::SetBase(Config::MAIN_DEFAULT_ISO, path.toStdString());
    emit DefaultGameChanged(path);
  }
}

QString Settings::GetLauncherPath() const
{
  return QString::fromStdString(Config::Get(Config::MAIN_LAUNCHER_PATH));
}

void Settings::SetLauncherPath(QString path)
{
  if (GetLauncherPath() != path)
  {
    Config::SetBase(Config::MAIN_LAUNCHER_PATH, path.toStdString());
    emit LauncherPathChanged(path);
  }
}

bool Settings::GetPreferredView() const
{
  return GetQSettings()
      .value(QStringLiteral("PreferredView"), DEFAULT_PREFERRED_VIEW_IS_LIST)
      .toBool();
}

void Settings::SetPreferredView(bool list)
{
  GetQSettings().setValue(QStringLiteral("PreferredView"), list);
}

int Settings::GetStateSlot() const
{
  return GetQSettings().value(QStringLiteral("Emulation/StateSlot"), 1).toInt();
}

void Settings::SetStateSlot(int slot)
{
  GetQSettings().setValue(QStringLiteral("Emulation/StateSlot"), slot);
}

Config::ShowCursor Settings::GetCursorVisibility() const
{
  return Config::Get(Config::MAIN_SHOW_CURSOR);
}

bool Settings::GetLockCursor() const
{
  return Config::Get(Config::MAIN_LOCK_CURSOR);
}

void Settings::SetKeepWindowOnTop(bool top)
{
  if (IsKeepWindowOnTopEnabled() == top)
    return;

  emit KeepWindowOnTopChanged(top);
}

bool Settings::IsKeepWindowOnTopEnabled() const
{
  return Config::Get(Config::MAIN_KEEP_WINDOW_ON_TOP);
}

bool Settings::GetGraphicModsEnabled() const
{
  return Config::Get(Config::GFX_MODS_ENABLE);
}

void Settings::SetGraphicModsEnabled(bool enabled)
{
  if (GetGraphicModsEnabled() == enabled)
  {
    return;
  }

  Config::SetBaseOrCurrent(Config::GFX_MODS_ENABLE, enabled);
  emit EnableGfxModsChanged(enabled);
}

int Settings::GetVolume() const
{
  return Config::Get(Config::MAIN_AUDIO_VOLUME);
}

void Settings::SetVolume(int volume)
{
  if (GetVolume() != volume)
    Config::SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, volume);
}

void Settings::IncreaseVolume(int volume)
{
  AudioCommon::IncreaseVolume(Core::System::GetInstance(), volume);
}

void Settings::DecreaseVolume(int volume)
{
  AudioCommon::DecreaseVolume(Core::System::GetInstance(), volume);
}

bool Settings::IsLogVisible() const
{
  return GetQSettings().value(QStringLiteral("logging/logvisible")).toBool();
}

void Settings::SetLogVisible(bool visible)
{
  if (IsLogVisible() != visible)
  {
    GetQSettings().setValue(QStringLiteral("logging/logvisible"), visible);
    emit LogVisibilityChanged(visible);
  }
}

bool Settings::IsLogConfigVisible() const
{
  return GetQSettings().value(QStringLiteral("logging/logconfigvisible")).toBool();
}

void Settings::SetLogConfigVisible(bool visible)
{
  if (IsLogConfigVisible() != visible)
  {
    GetQSettings().setValue(QStringLiteral("logging/logconfigvisible"), visible);
    emit LogConfigVisibilityChanged(visible);
  }
}

std::shared_ptr<NetPlay::NetPlayClient> Settings::GetNetPlayClient()
{
  return m_client;
}

void Settings::ResetNetPlayClient(NetPlay::NetPlayClient* client)
{
  m_client.reset(client);

  SetNetPlayChatUI(nullptr);
  SetNetPlayGolfUI(nullptr);
}

std::shared_ptr<NetPlay::NetPlayServer> Settings::GetNetPlayServer()
{
  return m_server;
}

void Settings::ResetNetPlayServer(NetPlay::NetPlayServer* server)
{
  m_server.reset(server);
}

bool Settings::GetCheatsEnabled() const
{
  return Config::Get(Config::MAIN_ENABLE_CHEATS);
}

void Settings::SetRegistersVisible(bool enabled)
{
  if (IsRegistersVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showregisters"), enabled);

    emit RegistersVisibilityChanged(enabled);
  }
}

bool Settings::IsThreadsVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showthreads")).toBool();
}

void Settings::SetThreadsVisible(bool enabled)
{
  if (IsThreadsVisible() == enabled)
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showthreads"), enabled);
  emit ThreadsVisibilityChanged(enabled);
}

bool Settings::IsRegistersVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showregisters")).toBool();
}

void Settings::SetWatchVisible(bool enabled)
{
  if (IsWatchVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showwatch"), enabled);

    emit WatchVisibilityChanged(enabled);
  }
}

bool Settings::IsWatchVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showwatch")).toBool();
}

void Settings::SetBreakpointsVisible(bool enabled)
{
  if (IsBreakpointsVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showbreakpoints"), enabled);

    emit BreakpointsVisibilityChanged(enabled);
  }
}

bool Settings::IsBreakpointsVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showbreakpoints")).toBool();
}

void Settings::SetCodeVisible(bool enabled)
{
  if (IsCodeVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showcode"), enabled);

    emit CodeVisibilityChanged(enabled);
  }
}

bool Settings::IsCodeVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showcode"), true).toBool();
}

void Settings::SetMemoryVisible(bool enabled)
{
  if (IsMemoryVisible() == enabled)
    return;
  GetQSettings().setValue(QStringLiteral("debugger/showmemory"), enabled);

  emit MemoryVisibilityChanged(enabled);
}

bool Settings::IsMemoryVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showmemory")).toBool();
}

void Settings::SetNetworkVisible(bool enabled)
{
  if (IsNetworkVisible() == enabled)
    return;

  GetQSettings().setValue(QStringLiteral("debugger/shownetwork"), enabled);
  emit NetworkVisibilityChanged(enabled);
}

bool Settings::IsNetworkVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/shownetwork")).toBool();
}

void Settings::SetJITVisible(bool enabled)
{
  if (IsJITVisible() == enabled)
    return;
  GetQSettings().setValue(QStringLiteral("debugger/showjit"), enabled);

  emit JITVisibilityChanged(enabled);
}

bool Settings::IsJITVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showjit")).toBool();
}

void Settings::SetAssemblerVisible(bool enabled)
{
  if (IsAssemblerVisible() == enabled)
    return;
  GetQSettings().setValue(QStringLiteral("debugger/showassembler"), enabled);

  emit AssemblerVisibilityChanged(enabled);
}

bool Settings::IsAssemblerVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showassembler")).toBool();
}

void Settings::RefreshWidgetVisibility()
{
  emit DebugModeToggled(Config::IsDebuggingEnabled());
  emit LogVisibilityChanged(IsLogVisible());
  emit LogConfigVisibilityChanged(IsLogConfigVisible());
  emit GameCountVisibilityChanged(IsGameCountVisible());
}

void Settings::SetDebugFont(const QFont& font)
{
  if (GetDebugFont() != font)
  {
    GetQSettings().setValue(QStringLiteral("debugger/font"), font);

    emit DebugFontChanged(font);
  }
}

QFont Settings::GetDebugFont() const
{
  QFont default_font = QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
  default_font.setPointSizeF(9.0);

  return GetQSettings().value(QStringLiteral("debugger/font"), default_font).value<QFont>();
}

void Settings::SetShowDemangledNames(bool enabled)
{
  if (IsShowDemangledNames() == enabled)
    return;
  QSettings().setValue(QStringLiteral("debugger/showdemanglednames"), enabled);

  emit ShowDemangledNamesChanged(enabled);
}

bool Settings::IsShowDemangledNames() const
{
  return QSettings().value(QStringLiteral("debugger/showdemanglednames")).toBool();
}

void Settings::SetAutoUpdateTrack(const QString& mode)
{
  if (mode == GetAutoUpdateTrack())
    return;

  Config::SetBase(Config::MAIN_AUTOUPDATE_UPDATE_TRACK, mode.toStdString());

  emit AutoUpdateTrackChanged(mode);
}

QString Settings::GetAutoUpdateTrack() const
{
  return QString::fromStdString(Config::Get(Config::MAIN_AUTOUPDATE_UPDATE_TRACK));
}

void Settings::SetAnalyticsEnabled(bool enabled)
{
  if (enabled == IsAnalyticsEnabled())
    return;

  Config::SetBase(Config::MAIN_ANALYTICS_ENABLED, enabled);

  emit AnalyticsToggled(enabled);
}

bool Settings::IsAnalyticsEnabled() const
{
  return Config::Get(Config::MAIN_ANALYTICS_ENABLED);
}

void Settings::SetToolBarVisible(bool visible)
{
  if (IsToolBarVisible() == visible)
    return;

  GetQSettings().setValue(QStringLiteral("toolbar/visible"), visible);

  emit ToolBarVisibilityChanged(visible);
}

bool Settings::IsToolBarVisible() const
{
  return GetQSettings().value(QStringLiteral("toolbar/visible"), true).toBool();
}

void Settings::SetWidgetsLocked(bool locked)
{
  if (AreWidgetsLocked() == locked)
    return;

  GetQSettings().setValue(QStringLiteral("widgets/locked"), locked);

  emit WidgetLockChanged(locked);
}

bool Settings::AreWidgetsLocked() const
{
  return GetQSettings().value(QStringLiteral("widgets/locked"), true).toBool();
}

bool Settings::IsBatchModeEnabled() const
{
  return m_batch;
}
void Settings::SetBatchModeEnabled(bool batch)
{
  m_batch = batch;
}

bool Settings::IsSDCardInserted() const
{
  return Config::Get(Config::MAIN_WII_SD_CARD);
}

void Settings::SetSDCardInserted(bool inserted)
{
  if (IsSDCardInserted() != inserted)
    Config::SetBaseOrCurrent(Config::MAIN_WII_SD_CARD, inserted);
}

bool Settings::IsUSBKeyboardConnected() const
{
  return Config::Get(Config::MAIN_WII_KEYBOARD);
}

void Settings::SetUSBKeyboardConnected(bool connected)
{
  if (IsUSBKeyboardConnected() != connected)
    Config::SetBaseOrCurrent(Config::MAIN_WII_KEYBOARD, connected);
}

bool Settings::IsWiiSpeakMuted() const
{
  return Config::Get(Config::MAIN_WII_SPEAK_MUTED);
}

void Settings::SetWiiSpeakMuted(bool muted)
{
  if (IsWiiSpeakMuted() == muted)
    return;

  Config::SetBaseOrCurrent(Config::MAIN_WII_SPEAK_MUTED, muted);
  emit WiiSpeakMuteChanged(muted);
}

void Settings::SetIsContinuouslyFrameStepping(bool is_stepping)
{
  m_continuously_frame_stepping = is_stepping;
}

bool Settings::GetIsContinuouslyFrameStepping() const
{
  return m_continuously_frame_stepping;
}
