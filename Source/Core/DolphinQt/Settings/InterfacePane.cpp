// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/InterfacePane.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QRadioButton>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/InterfaceSettingsUtils.h"

#include "ui_InterfacePane.h"

namespace
{
void ConnectCursorVisibility(QRadioButton* radio_button)
{
  QObject::connect(radio_button, &QRadioButton::toggled, radio_button, [](bool checked) {
    if (checked)
      emit Settings::Instance().CursorVisibilityChanged();
  });
}
}  // namespace

InterfacePane::InterfacePane(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::InterfacePane>())
{
  m_ui->setupUi(this);
#ifndef _WIN32
  m_ui->lockMouseCursorCheckBox->hide();
#endif

  PopulateStyleChoices();
  BindSettings();
  AddDescriptions();
  UpdateShowDebuggingCheckbox();
  LoadUserStyle();
  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &InterfacePane::UpdateShowDebuggingCheckbox);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &InterfacePane::OnEmulationStateChanged);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

InterfacePane::~InterfacePane() = default;

void InterfacePane::BindSettings()
{
  BindLanguageChoice();
  BindThemeChoice();

  ConfigWidget::Bind(m_ui->useBuiltinTitleDatabaseCheckBox,
                     Config::MAIN_USE_BUILT_IN_TITLE_DATABASE);
  ConfigWidget::Bind(m_ui->useCoversCheckBox, Config::MAIN_USE_GAME_COVERS);
  ConfigWidget::Bind(m_ui->focusedHotkeysCheckBox, Config::MAIN_FOCUSED_HOTKEYS);
  ConfigWidget::Bind(m_ui->disableScreensaverCheckBox, Config::MAIN_DISABLE_SCREENSAVER);
  ConfigWidget::Bind(m_ui->timeTrackingCheckBox, Config::MAIN_TIME_TRACKING);

  ConfigWidget::Bind(m_ui->keepWindowOnTopCheckBox, Config::MAIN_KEEP_WINDOW_ON_TOP);
  ConfigWidget::Bind(m_ui->confirmOnStopCheckBox, Config::MAIN_CONFIRM_ON_STOP);
  ConfigWidget::Bind(m_ui->usePanicHandlersCheckBox, Config::MAIN_USE_PANIC_HANDLERS);
  ConfigWidget::Bind(m_ui->showActiveTitleCheckBox, Config::MAIN_SHOW_ACTIVE_TITLE);
  ConfigWidget::Bind(m_ui->pauseOnFocusLossCheckBox, Config::MAIN_PAUSE_ON_FOCUS_LOST);
  ConfigWidget::Bind(m_ui->cursorOnMovementRadioButton, Config::MAIN_SHOW_CURSOR,
                     static_cast<int>(Config::ShowCursor::OnMovement));
  ConfigWidget::Bind(m_ui->cursorNeverRadioButton, Config::MAIN_SHOW_CURSOR,
                     static_cast<int>(Config::ShowCursor::Never));
  ConfigWidget::Bind(m_ui->cursorAlwaysRadioButton, Config::MAIN_SHOW_CURSOR,
                     static_cast<int>(Config::ShowCursor::Constantly));
  ConfigWidget::Bind(m_ui->lockMouseCursorCheckBox, Config::MAIN_LOCK_CURSOR);
}

void InterfacePane::BindLanguageChoice()
{
  const auto languages = InterfaceSettings::GetLanguageChoices();
  ConfigWidget::BindStringChoice(m_ui->languageComboBox, Config::MAIN_INTERFACE_LANGUAGE,
                                 languages);
}

void InterfacePane::BindThemeChoice()
{
  const auto theme_paths = Common::DoFileSearch(
      {{File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR}});
  std::vector<std::string> theme_names;
  theme_names.reserve(theme_paths.size());
  std::ranges::transform(theme_paths, std::back_inserter(theme_names), PathToFileName);
  ConfigWidget::BindStringChoice(m_ui->themeComboBox, Config::MAIN_THEME_NAME, theme_names);
}

void InterfacePane::PopulateStyleChoices()
{
  auto userstyle_search_results = Common::DoFileSearch(File::GetUserPath(D_STYLES_IDX));

  m_ui->styleComboBox->addItem(tr("(System)"), static_cast<int>(Settings::StyleType::System));
  m_ui->styleComboBox->addItem(tr("(Light)"), static_cast<int>(Settings::StyleType::Light));
  m_ui->styleComboBox->addItem(tr("(Dark Gray)"), static_cast<int>(Settings::StyleType::DarkGray));
  m_ui->styleComboBox->addItem(tr("(Dark)"), static_cast<int>(Settings::StyleType::Dark));

  for (const std::string& path : userstyle_search_results)
  {
    const QFileInfo file_info(QString::fromStdString(path));
    m_ui->styleComboBox->addItem(file_info.completeBaseName(), file_info.fileName());
  }
}

void InterfacePane::ConnectLayout()
{
  connect(m_ui->useBuiltinTitleDatabaseCheckBox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::GameListRefreshRequested);
  connect(m_ui->useCoversCheckBox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::RefreshMetadata);
  connect(m_ui->showDebuggingUiCheckBox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetDebugModeEnabled);
  connect(m_ui->themeComboBox, &QComboBox::currentIndexChanged, &Settings::Instance(),
          &Settings::ThemeChanged);
  connect(m_ui->styleComboBox, &QComboBox::currentIndexChanged, this,
          &InterfacePane::OnUserStyleChanged);
  connect(m_ui->languageComboBox, &QComboBox::currentIndexChanged, this,
          &InterfacePane::OnLanguageChanged);
  connect(m_ui->keepWindowOnTopCheckBox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::KeepWindowOnTopChanged);
  ConnectCursorVisibility(m_ui->cursorOnMovementRadioButton);
  ConnectCursorVisibility(m_ui->cursorNeverRadioButton);
  ConnectCursorVisibility(m_ui->cursorAlwaysRadioButton);
  connect(m_ui->lockMouseCursorCheckBox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::LockCursorChanged);
}

void InterfacePane::UpdateShowDebuggingCheckbox()
{
  SignalBlocking(m_ui->showDebuggingUiCheckBox)
      ->setChecked(Settings::Instance().IsDebugModeEnabled());

  static constexpr char TR_SHOW_DEBUGGING_UI_DESCRIPTION[] = QT_TR_NOOP(
      "Shows Dolphin's debugging user interface. This lets you view and modify a game's code and "
      "memory contents, set debugging breakpoints, examine network requests, and more."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_DISABLED_IN_HARDCORE_DESCRIPTION[] =
      QT_TR_NOOP("<dolphin_emphasis>Disabled in Hardcore Mode.</dolphin_emphasis>");

  bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
  SignalBlocking(m_ui->showDebuggingUiCheckBox)->setEnabled(!hardcore);
  if (hardcore)
  {
    ConfigWidget::SetDescription(m_ui->showDebuggingUiCheckBox, QString{},
                                 tr("%1<br><br>%2")
                                     .arg(tr(TR_SHOW_DEBUGGING_UI_DESCRIPTION))
                                     .arg(tr(TR_DISABLED_IN_HARDCORE_DESCRIPTION)));
  }
  else
  {
    ConfigWidget::SetDescription(m_ui->showDebuggingUiCheckBox, QString{},
                                 tr(TR_SHOW_DEBUGGING_UI_DESCRIPTION));
  }
}

void InterfacePane::LoadUserStyle()
{
  const Settings::StyleType style_type = Settings::Instance().GetStyleType();
  const QString userstyle = Settings::Instance().GetUserStyleName();
  const int index = style_type == Settings::StyleType::User ?
                        m_ui->styleComboBox->findData(userstyle) :
                        m_ui->styleComboBox->findData(static_cast<int>(style_type));

  if (index > 0)
    SignalBlocking(m_ui->styleComboBox)->setCurrentIndex(index);
}

void InterfacePane::OnUserStyleChanged()
{
  const auto selected_style = m_ui->styleComboBox->currentData();
  bool is_builtin_type = false;
  const int style_type_int = selected_style.toInt(&is_builtin_type);
  Settings::Instance().SetStyleType(is_builtin_type ?
                                        static_cast<Settings::StyleType>(style_type_int) :
                                        Settings::StyleType::User);
  if (!is_builtin_type)
    Settings::Instance().SetUserStyleName(selected_style.toString());
  Settings::Instance().ApplyStyle();
}

void InterfacePane::OnLanguageChanged()
{
  ModalMessageBox::information(
      this, tr("Restart Required"),
      tr("You must restart Dolphin in order for the change to take effect."));
}

void InterfacePane::OnEmulationStateChanged(Core::State state)
{
  const bool uninitialized = state == Core::State::Uninitialized;
  m_ui->timeTrackingCheckBox->setEnabled(uninitialized);
}

void InterfacePane::AddDescriptions()
{
  static constexpr char TR_TITLE_DATABASE_DESCRIPTION[] = QT_TR_NOOP(
      "Uses Dolphin's database of properly formatted names in the game list's Title column."
      "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_THEME_DESCRIPTION[] =
      QT_TR_NOOP("Changes the appearance and color of Dolphin's buttons."
                 "<br><br><dolphin_emphasis>If unsure, select Clean.</dolphin_emphasis>");
  static constexpr char TR_TOP_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Forces the render window to stay on top of other windows and applications."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_LANGUAGE_DESCRIPTION[] = QT_TR_NOOP(
      "Sets the language displayed by Dolphin's user interface."
      "<br><br>Changes to this setting only take effect once Dolphin is restarted."
      "<br><br><dolphin_emphasis>If unsure, select &lt;System Language&gt;.</dolphin_emphasis>");
  static constexpr char TR_FOCUSED_HOTKEYS_DESCRIPTION[] =
      QT_TR_NOOP("Requires the render window to be focused for hotkeys to take effect."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_USE_COVERS_DESCRIPTION[] =
      QT_TR_NOOP("Downloads full game covers from GameTDB.com to display in the game list's Grid "
                 "View. If this setting is unchecked, the game list displays a banner from the "
                 "game's save data, and if the game has no save file, displays a generic "
                 "banner instead."
                 "<br><br>List View will always use the save file banners."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_DISABLE_SCREENSAVER_DESCRIPTION[] =
      QT_TR_NOOP("Disables your screensaver while running a game."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_TIME_TRACKING[] = QT_TR_NOOP(
      "Tracks the time you spend playing games and shows it in the List View (as hours/minutes)."
      "<br><br>This setting cannot be changed while emulation is active."
      "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_CONFIRM_ON_STOP_DESCRIPTION[] =
      QT_TR_NOOP("Prompts you to confirm that you want to end emulation when you press Stop."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_USE_PANIC_HANDLERS_DESCRIPTION[] =
      QT_TR_NOOP("In the event of an error, Dolphin will halt to inform you of the error and "
                 "present choices on how to proceed. With this option disabled, Dolphin will "
                 "\"ignore\" all errors. Emulation will not be halted and you will not be notified."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_SHOW_ACTIVE_TITLE_DESCRIPTION[] =
      QT_TR_NOOP("Shows the active game title in the render window's title bar."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_PAUSE_ON_FOCUS_LOST_DESCRIPTION[] =
      QT_TR_NOOP("Pauses the game whenever the render window isn't focused."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_LOCK_MOUSE_DESCRIPTION[] =
      QT_TR_NOOP("Locks the mouse cursor to the Render Widget as long as it has focus. You can "
                 "set a hotkey to unlock it."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_CURSOR_VISIBLE_MOVEMENT_DESCRIPTION[] =
      QT_TR_NOOP("Shows the mouse cursor briefly whenever it has recently moved, then hides it."
                 "<br><br><dolphin_emphasis>If unsure, select this mode.</dolphin_emphasis>");
  static constexpr char TR_CURSOR_VISIBLE_NEVER_DESCRIPTION[] = QT_TR_NOOP(
      "Hides the mouse cursor whenever it is inside the render window and the render window is "
      "focused."
      "<br><br><dolphin_emphasis>If unsure, select &quot;On Movement&quot;.</dolphin_emphasis>");
  static constexpr char TR_CURSOR_VISIBLE_ALWAYS_DESCRIPTION[] = QT_TR_NOOP(
      "Shows the mouse cursor at all times."
      "<br><br><dolphin_emphasis>If unsure, select &quot;On Movement&quot;.</dolphin_emphasis>");
  static constexpr char TR_USER_STYLE_DESCRIPTION[] =
      QT_TR_NOOP("Sets the style of Dolphin's user interface. Any custom styles that you have "
                 "added will be presented here, allowing you to switch to them."
                 "<br><br><dolphin_emphasis>If unsure, select (System).</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->useBuiltinTitleDatabaseCheckBox, QString{},
                               tr(TR_TITLE_DATABASE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->themeComboBox, tr("Theme"), tr(TR_THEME_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->keepWindowOnTopCheckBox, QString{},
                               tr(TR_TOP_WINDOW_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->languageComboBox, tr("Language"), tr(TR_LANGUAGE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->focusedHotkeysCheckBox, QString{},
                               tr(TR_FOCUSED_HOTKEYS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->useCoversCheckBox, QString{}, tr(TR_USE_COVERS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->disableScreensaverCheckBox, QString{},
                               tr(TR_DISABLE_SCREENSAVER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->timeTrackingCheckBox, QString{}, tr(TR_TIME_TRACKING));
  ConfigWidget::SetDescription(m_ui->confirmOnStopCheckBox, QString{},
                               tr(TR_CONFIRM_ON_STOP_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->usePanicHandlersCheckBox, QString{},
                               tr(TR_USE_PANIC_HANDLERS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showActiveTitleCheckBox, QString{},
                               tr(TR_SHOW_ACTIVE_TITLE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->pauseOnFocusLossCheckBox, QString{},
                               tr(TR_PAUSE_ON_FOCUS_LOST_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->lockMouseCursorCheckBox, QString{},
                               tr(TR_LOCK_MOUSE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->cursorOnMovementRadioButton, QString{},
                               tr(TR_CURSOR_VISIBLE_MOVEMENT_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->cursorNeverRadioButton, QString{},
                               tr(TR_CURSOR_VISIBLE_NEVER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->cursorAlwaysRadioButton, QString{},
                               tr(TR_CURSOR_VISIBLE_ALWAYS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->styleComboBox, tr("Style"), tr(TR_USER_STYLE_DESCRIPTION));
}
