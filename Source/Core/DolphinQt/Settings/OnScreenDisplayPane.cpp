// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/OnScreenDisplayPane.h"

#include <memory>

#include <QCheckBox>
#include <QWidget>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

#include "ui_OnScreenDisplayPane.h"

OnScreenDisplayPane::OnScreenDisplayPane(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::OnScreenDisplayPane>())
{
  m_ui->setupUi(this);
  BindSettings();
  ConnectLayout();
  AddDescriptions();
}

OnScreenDisplayPane::~OnScreenDisplayPane() = default;

void OnScreenDisplayPane::BindSettings()
{
  ConfigWidget::Bind(m_ui->showMessagesCheckBox, Config::MAIN_OSD_MESSAGES);
  ConfigWidget::Bind(m_ui->fontSizeSpinBox, Config::MAIN_OSD_FONT_SIZE);

  ConfigWidget::Bind(m_ui->showFpsCheckBox, Config::GFX_SHOW_FPS);
  ConfigWidget::Bind(m_ui->showFrameTimesCheckBox, Config::GFX_SHOW_FTIMES);
  ConfigWidget::Bind(m_ui->showVpsCheckBox, Config::GFX_SHOW_VPS);
  ConfigWidget::Bind(m_ui->showVblankTimesCheckBox, Config::GFX_SHOW_VTIMES);
  ConfigWidget::Bind(m_ui->showSpeedCheckBox, Config::GFX_SHOW_SPEED);
  ConfigWidget::Bind(m_ui->showGraphsCheckBox, Config::GFX_SHOW_GRAPHS);
  ConfigWidget::Bind(m_ui->showSpeedColorsCheckBox, Config::GFX_SHOW_SPEED_COLORS);
  ConfigWidget::Bind(m_ui->performanceSampleWindowSpinBox, Config::GFX_PERF_SAMP_WINDOW);

  ConfigWidget::Bind(m_ui->showMovieWindowCheckBox, Config::MAIN_MOVIE_SHOW_OSD);
  ConfigWidget::Bind(m_ui->showRerecordCounterCheckBox, Config::MAIN_MOVIE_SHOW_RERECORD);
  ConfigWidget::Bind(m_ui->showLagCounterCheckBox, Config::MAIN_SHOW_LAG);
  ConfigWidget::Bind(m_ui->showFrameCounterCheckBox, Config::MAIN_SHOW_FRAME_COUNT);
  ConfigWidget::Bind(m_ui->showInputDisplayCheckBox, Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY);
  ConfigWidget::Bind(m_ui->showSystemClockCheckBox, Config::MAIN_MOVIE_SHOW_RTC);

  ConfigWidget::Bind(m_ui->showNetplayPingCheckBox, Config::GFX_SHOW_NETPLAY_PING);
  ConfigWidget::Bind(m_ui->showNetplayChatCheckBox, Config::GFX_SHOW_NETPLAY_MESSAGES);

  ConfigWidget::Bind(m_ui->showStatisticsCheckBox, Config::GFX_OVERLAY_STATS);
  ConfigWidget::Bind(m_ui->showProjectionStatisticsCheckBox, Config::GFX_OVERLAY_PROJ_STATS);
  ConfigWidget::Bind(m_ui->showXfbResolutionCheckBox, Config::GFX_SHOW_INTERNAL_RESOLUTION);
}

void OnScreenDisplayPane::ConnectLayout()
{
  // Disable movie window options when window is closed.
  auto enable_movie_items = [this](bool checked) {
    for (auto* widget : {m_ui->showRerecordCounterCheckBox, m_ui->showFrameCounterCheckBox,
                         m_ui->showLagCounterCheckBox, m_ui->showSystemClockCheckBox,
                         m_ui->showInputDisplayCheckBox})
    {
      widget->setEnabled(checked);
    }
  };

  enable_movie_items(m_ui->showMovieWindowCheckBox->isChecked());
  connect(m_ui->showMovieWindowCheckBox, &QCheckBox::toggled, this, enable_movie_items);
}

void OnScreenDisplayPane::AddDescriptions()
{
  static constexpr char TR_ENABLE_OSD_DESCRIPTION[] =
      QT_TR_NOOP("Shows on-screen display messages over the render window. These messages "
                 "disappear after several seconds."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_OSD_FONT_SIZE_DESCRIPTION[] = QT_TR_NOOP(
      "Changes the font size of the On-Screen Display. Affects features such as performance "
      "statistics, frame counter, and netplay chat."
      "<br><br>The font can be changed by placing a TTF font file into Dolphin's User/Load "
      "folder, and renaming it OSD_Font.ttf."
      "<br><br><dolphin_emphasis>If unsure, leave this at 13.</dolphin_emphasis>");

  static const char TR_SHOW_FPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of distinct frames rendered per second as a measure of "
                 "visual smoothness.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_FTIMES_DESCRIPTION[] =
      QT_TR_NOOP("Shows the average time in ms between each distinct rendered frame alongside "
                 "the standard deviation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_VPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of frames rendered per second as a measure of "
                 "emulation speed.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_VTIMES_DESCRIPTION[] =
      QT_TR_NOOP("Shows the average time in ms between each rendered frame alongside "
                 "the standard deviation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_GRAPHS_DESCRIPTION[] =
      QT_TR_NOOP("Shows frametime graph along with statistics as a representation of "
                 "emulation performance.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_SPEED_DESCRIPTION[] =
      QT_TR_NOOP("Shows the % speed of emulation compared to full speed."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_SPEED_COLORS_DESCRIPTION[] =
      QT_TR_NOOP("Changes the color of the FPS counter depending on emulation speed."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "checked.</dolphin_emphasis>");
  static const char TR_PERF_SAMP_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("The amount of time the FPS and VPS counters will sample over."
                 "<br><br>The higher the value, the more stable the FPS/VPS counter will be, "
                 "but the slower it will be to update."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "at 1000ms.</dolphin_emphasis>");

  static const char TR_SHOW_NETPLAY_PING_DESCRIPTION[] = QT_TR_NOOP(
      "Shows the player's maximum ping while playing on "
      "NetPlay.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION[] =
      QT_TR_NOOP("Shows chat messages, buffer changes, and desync alerts "
                 "while playing NetPlay.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");

  static const char TR_MOVIE_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Shows a window that can be filled with information related to movie recordings. "
                 "The other options in this group determine what appears in the window. "
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_RERECORD_COUNTER_DESCRIPTION[] =
      QT_TR_NOOP("Shows how many times the input recording has been overwritten by using "
                 "savestates.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_LAG_COUNTER_DESCRIPTION[] = QT_TR_NOOP(
      "Shows how many frames have occurred without controller inputs being checked. Resets to 1 "
      "when inputs are processed. <br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
  static const char TR_FRAME_COUNTER_DESCRIPTION[] =
      QT_TR_NOOP("Shows how many frames have passed. <br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_INPUT_DISPLAY_DESCRIPTION[] = QT_TR_NOOP(
      "Shows the controls currently being input.<br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
  static const char TR_SYSTEM_CLOCK_DESCRIPTION[] =
      QT_TR_NOOP("Shows current system time.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");

  static const char TR_SHOW_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various rendering statistics.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_PROJ_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various projection statistics.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_INTERNAL_RESOLUTION_DESCRIPTION[] =
      QT_TR_NOOP("Shows the size of the emulated external frame buffer (XFB) in pixels, as a "
                 "product of width and height.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->showMessagesCheckBox, QString{},
                               tr(TR_ENABLE_OSD_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->fontSizeSpinBox, tr("Font Size"),
                               tr(TR_OSD_FONT_SIZE_DESCRIPTION));

  ConfigWidget::SetDescription(m_ui->showFpsCheckBox, QString{}, tr(TR_SHOW_FPS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showFrameTimesCheckBox, QString{},
                               tr(TR_SHOW_FTIMES_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showVpsCheckBox, QString{}, tr(TR_SHOW_VPS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showVblankTimesCheckBox, QString{},
                               tr(TR_SHOW_VTIMES_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showGraphsCheckBox, QString{}, tr(TR_SHOW_GRAPHS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showSpeedCheckBox, QString{}, tr(TR_SHOW_SPEED_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->performanceSampleWindowSpinBox,
                               tr("Performance Sample Window (ms)"),
                               tr(TR_PERF_SAMP_WINDOW_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showSpeedColorsCheckBox, QString{},
                               tr(TR_SHOW_SPEED_COLORS_DESCRIPTION));

  ConfigWidget::SetDescription(m_ui->showNetplayPingCheckBox, QString{},
                               tr(TR_SHOW_NETPLAY_PING_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showNetplayChatCheckBox, QString{},
                               tr(TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION));

  ConfigWidget::SetDescription(m_ui->showMovieWindowCheckBox, QString{},
                               tr(TR_MOVIE_WINDOW_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showRerecordCounterCheckBox, QString{},
                               tr(TR_RERECORD_COUNTER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showLagCounterCheckBox, QString{},
                               tr(TR_LAG_COUNTER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showFrameCounterCheckBox, QString{},
                               tr(TR_FRAME_COUNTER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showInputDisplayCheckBox, QString{},
                               tr(TR_INPUT_DISPLAY_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showSystemClockCheckBox, QString{},
                               tr(TR_SYSTEM_CLOCK_DESCRIPTION));

  ConfigWidget::SetDescription(m_ui->showStatisticsCheckBox, QString{},
                               tr(TR_SHOW_STATS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showProjectionStatisticsCheckBox, QString{},
                               tr(TR_SHOW_PROJ_STATS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->showXfbResolutionCheckBox, QString{},
                               tr(TR_SHOW_INTERNAL_RESOLUTION_DESCRIPTION));
}
