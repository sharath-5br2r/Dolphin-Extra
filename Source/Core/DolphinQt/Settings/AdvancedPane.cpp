// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AdvancedPane.h"

#include <cmath>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimeZone>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/QtUtils/AnalyticsPrompt.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "UICommon/UICommon.h"

#include "ui_AdvancedPane.h"

static const std::map<PowerPC::CPUCore, const char*> CPU_CORE_NAMES = {
    {PowerPC::CPUCore::Interpreter, QT_TR_NOOP("Interpreter (slowest)")},
    {PowerPC::CPUCore::CachedInterpreter, QT_TR_NOOP("Cached Interpreter (slower)")},
    {PowerPC::CPUCore::JIT64, QT_TR_NOOP("JIT Recompiler for x86-64 (recommended)")},
    {PowerPC::CPUCore::JITARM64, QT_TR_NOOP("JIT Recompiler for ARM64 (recommended)")},
};

AdvancedPane::AdvancedPane(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::AdvancedPane>())
{
  m_ui->setupUi(this);
  ConfigureWidgets();
  BindSettings();
  ConnectWidgets();
  AddDescriptions();
  Update();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &AdvancedPane::Update);
}

AdvancedPane::~AdvancedPane() = default;

void AdvancedPane::ConfigureWidgets()
{
  const QFontMetrics font_metrics{font()};
  const int label_width = font_metrics.boundingRect(QStringLiteral(" 500% (000.00 VPS)")).width();
  for (QLabel* const label :
       {m_ui->cpuClockLabel, m_ui->vbiOverrideLabel, m_ui->mem1Label, m_ui->mem2Label})
  {
    label->setFixedWidth(label_width);
  }

  // Show seconds.
  m_ui->customRtcDateTimeEdit->setDisplayFormat(
      m_ui->customRtcDateTimeEdit->displayFormat().replace(QStringLiteral("mm"),
                                                           QStringLiteral("mm:ss")));

  QtUtils::ShowFourDigitYear(m_ui->customRtcDateTimeEdit);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  m_ui->customRtcDateTimeEdit->setDateTimeRange(
      QDateTime({2000, 1, 1}, {0, 0, 0}, QTimeZone::UTC),
      QDateTime({2099, 12, 31}, {23, 59, 59}, QTimeZone::UTC));
#else
  m_ui->customRtcDateTimeEdit->setDateTimeRange(QDateTime({2000, 1, 1}, {0, 0, 0}, Qt::UTC),
                                                QDateTime({2099, 12, 31}, {23, 59, 59}, Qt::UTC));
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  m_ui->customRtcDateTimeEdit->setTimeZone(QTimeZone::UTC);
#else
  m_ui->customRtcDateTimeEdit->setTimeSpec(Qt::UTC);
#endif
}

void AdvancedPane::BindSettings()
{
  std::vector<std::pair<QString, PowerPC::CPUCore>> emulation_engine_choices;
  for (PowerPC::CPUCore cpu_core : PowerPC::AvailableCPUCores())
    emulation_engine_choices.emplace_back(tr(CPU_CORE_NAMES.at(cpu_core)), cpu_core);
  ConfigWidget::BindMapped(
      m_ui->cpuEngineComboBox, Config::MAIN_CPU_CORE,
      std::span<const std::pair<QString, PowerPC::CPUCore>>{emulation_engine_choices});

  ConfigWidget::Bind(m_ui->enableMmuCheckBox, Config::MAIN_MMU);
  ConfigWidget::Bind(m_ui->pauseOnPanicCheckBox, Config::MAIN_PAUSE_ON_PANIC);
  ConfigWidget::Bind(m_ui->accurateCpuCacheCheckBox, Config::MAIN_ACCURATE_CPU_CACHE);
  ConfigWidget::Bind(m_ui->correctTimeDriftCheckBox, Config::MAIN_CORRECT_TIME_DRIFT);
  ConfigWidget::Bind(m_ui->rushFramePresentationCheckBox, Config::MAIN_RUSH_FRAME_PRESENTATION);
  ConfigWidget::Bind(m_ui->smoothEarlyPresentationCheckBox, Config::MAIN_SMOOTH_EARLY_PRESENTATION);

  ConfigWidget::Bind(m_ui->cpuClockOverrideCheckBox, Config::MAIN_OVERCLOCK_ENABLE);
  ConfigWidget::BindFloat(m_ui->cpuClockSlider, Config::MAIN_OVERCLOCK, 0.01f, 5.0f, 0.01f);

  ConfigWidget::Bind(m_ui->vbiOverrideCheckBox, Config::MAIN_VI_OVERCLOCK_ENABLE);
  ConfigWidget::BindFloat(m_ui->vbiOverrideSlider, Config::MAIN_VI_OVERCLOCK, 0.01f, 5.0f, 0.01f);

  ConfigWidget::Bind(m_ui->memoryOverrideCheckBox, Config::MAIN_RAM_OVERRIDE_ENABLE);
  ConfigWidget::BindScaled(m_ui->mem1Slider, Config::MAIN_MEM1_SIZE, 0x100000);
  ConfigWidget::BindScaled(m_ui->mem2Slider, Config::MAIN_MEM2_SIZE, 0x100000);

  ConfigWidget::Bind(m_ui->customRtcCheckBox, Config::MAIN_CUSTOM_RTC_ENABLE);
}

void AdvancedPane::ConnectWidgets()
{
  connect(m_ui->cpuClockOverrideCheckBox, &QCheckBox::toggled, this, &AdvancedPane::Update);
  connect(m_ui->vbiOverrideCheckBox, &QCheckBox::toggled, this, &AdvancedPane::Update);
  connect(m_ui->memoryOverrideCheckBox, &QCheckBox::toggled, this, &AdvancedPane::Update);
  connect(m_ui->customRtcCheckBox, &QCheckBox::toggled, this, &AdvancedPane::Update);

  connect(m_ui->cpuClockSlider, &QSlider::valueChanged, this, &AdvancedPane::UpdateCpuClockLabel);
  connect(m_ui->vbiOverrideSlider, &QSlider::valueChanged, this, &AdvancedPane::UpdateVbiLabel);
  connect(m_ui->mem1Slider, &QSlider::valueChanged, this, [this](int value) {
    m_ui->mem1Label->setText(tr("%1 MB (MEM1)").arg(QString::number(value)));
  });
  connect(m_ui->mem2Slider, &QSlider::valueChanged, this, [this](int value) {
    m_ui->mem2Label->setText(tr("%1 MB (MEM2)").arg(QString::number(value)));
  });

  connect(m_ui->customRtcDateTimeEdit, &QDateTimeEdit::dateTimeChanged, this,
          [this](const QDateTime& date_time) {
            Config::SetBaseOrCurrent(Config::MAIN_CUSTOM_RTC_VALUE,
                                     static_cast<u32>(date_time.toSecsSinceEpoch()));
            Update();
          });

  connect(m_ui->resetSettingsButton, &QPushButton::clicked, this,
          &AdvancedPane::OnResetButtonClicked);

  UpdateCpuClockLabel();
  UpdateVbiLabel();
  m_ui->mem1Label->setText(tr("%1 MB (MEM1)").arg(QString::number(m_ui->mem1Slider->value())));
  m_ui->mem2Label->setText(tr("%1 MB (MEM2)").arg(QString::number(m_ui->mem2Slider->value())));
}

void AdvancedPane::AddDescriptions()
{
  ConfigWidget::SetDescription(
      m_ui->enableMmuCheckBox, QString{},
      tr("Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = "
         "Fast)<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->pauseOnPanicCheckBox, QString{},
      tr("Pauses the emulation if a Read/Write or Unknown Instruction panic occurs.<br>Enabling "
         "will affect performance.<br>The performance impact is the same as having Enable MMU "
         "on.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->accurateCpuCacheCheckBox, QString{},
      tr("Enables emulation of the CPU write-back cache.<br>Enabling will have a significant "
         "impact on performance.<br>This should be left disabled unless absolutely "
         "needed.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));

  ConfigWidget::SetDescription(
      m_ui->correctTimeDriftCheckBox, QString{},
      // i18n: Internet play refers to services like Wiimmfi, not the NetPlay feature in Dolphin
      tr("Allow the emulated console to run fast after stutters,"
         "<br>pursuing accurate overall elapsed time unless paused or speed-adjusted."
         "<br><br>This may be useful for internet play."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->rushFramePresentationCheckBox, QString{},
      tr("Limits throttling between input and frame output,"
         " speeding through emulation to reach presentation,"
         " displaying sooner, and thus reducing input latency."
         "<br><br>This will generally make frame pacing worse."
         "<br>This setting can work either with or without Immediately Present XFB."
         "<br>An Audio Buffer Size of at least 80 ms is recommended to ensure full effect."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->smoothEarlyPresentationCheckBox, QString{},
      tr("Adaptively adjusts the timing of early frame presentation."
         "<br><br>This can improve frame pacing with Immediately Present XFB"
         " and/or Rush Frame Presentation,"
         " while still maintaining most of the input latency benefits."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));

  ConfigWidget::SetDescription(
      m_ui->cpuClockOverrideCheckBox, QString{},
      tr("Adjusts the emulated CPU's clock rate.<br><br>"
         "On games that have an unstable frame rate despite full emulation speed, "
         "higher values can improve their performance, requiring a powerful device. "
         "Lower values reduce the emulated console's performance, but improve the "
         "emulation speed.<br><br>"
         "WARNING: Changing this from the default (100%) can and will "
         "break games and cause glitches. Do so at your own risk. "
         "Please do not report bugs that occur with a non-default clock."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->vbiOverrideCheckBox, QString{},
      tr("Adjusts the VBI frequency. Also adjusts the emulated CPU's "
         "clock rate, to keep it relatively the same.<br><br>"
         "Makes games run at a different frame rate, making the emulation less "
         "demanding when lowered, or improving smoothness when increased. This may "
         "affect gameplay speed, as it is often tied to the frame rate.<br><br>"
         "WARNING: Changing this from the default (100%) can and will "
         "break games and cause glitches. Do so at your own risk. "
         "Please do not report bugs that occur with a non-default frequency."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->memoryOverrideCheckBox, QString{},
      tr("Sets the amount of RAM in the emulated console to the values provided.<br><br>"
         "<b>WARNING</b>: Enabling this will completely break many games. By default, Dolphin "
         "determines what value is required based on the game information."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->customRtcCheckBox, QString{},
      tr("This setting allows you to set a custom real time clock (RTC) separate from "
         "your current system time."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
}

void AdvancedPane::UpdateCpuClockLabel()
{
  const float multiplier = Config::Get(Config::MAIN_OVERCLOCK);
  const int percent = static_cast<int>(std::round(multiplier * 100.f));
  const int core_clock =
      Core::System::GetInstance().GetSystemTimers().GetTicksPerSecond() / std::pow(10, 6);
  const int clock = static_cast<int>(std::round(multiplier * core_clock));
  m_ui->cpuClockLabel->setText(
      tr("%1% (%2 MHz)").arg(QString::number(percent), QString::number(clock)));
}

void AdvancedPane::UpdateVbiLabel()
{
  const float multiplier = Config::Get(Config::MAIN_VI_OVERCLOCK);
  const int percent = static_cast<int>(std::round(multiplier * 100.f));
  float vps =
      static_cast<float>(Core::System::GetInstance().GetVideoInterface().GetTargetRefreshRate());
  if (vps == 0.0f || !Config::Get(Config::MAIN_VI_OVERCLOCK_ENABLE))
    vps = 59.94f * multiplier;
  m_ui->vbiOverrideLabel->setText(
      tr("%1% (%2 VPS)").arg(QString::number(percent), QString::number(vps, 'f', 2)));
}

void AdvancedPane::Update()
{
  const bool is_uninitialized = Core::IsUninitialized(Core::System::GetInstance());
  const bool enable_cpu_clock_override_widgets = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  const bool enable_vi_rate_override_widgets = Config::Get(Config::MAIN_VI_OVERCLOCK_ENABLE);
  const bool enable_ram_override_widgets = Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE);
  const bool enable_custom_rtc_widgets =
      Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE) && is_uninitialized;

  m_ui->cpuEngineComboBox->setEnabled(is_uninitialized);
  m_ui->enableMmuCheckBox->setEnabled(is_uninitialized);
  m_ui->pauseOnPanicCheckBox->setEnabled(is_uninitialized);

  m_ui->cpuClockSlider->setEnabled(enable_cpu_clock_override_widgets);
  m_ui->cpuClockLabel->setEnabled(enable_cpu_clock_override_widgets);
  m_ui->vbiOverrideSlider->setEnabled(enable_vi_rate_override_widgets);
  m_ui->vbiOverrideLabel->setEnabled(enable_vi_rate_override_widgets);

  m_ui->memoryOverrideCheckBox->setEnabled(is_uninitialized);
  m_ui->mem1Slider->setEnabled(enable_ram_override_widgets && is_uninitialized);
  m_ui->mem1Label->setEnabled(enable_ram_override_widgets && is_uninitialized);
  m_ui->mem2Slider->setEnabled(enable_ram_override_widgets && is_uninitialized);
  m_ui->mem2Label->setEnabled(enable_ram_override_widgets && is_uninitialized);

  m_ui->customRtcCheckBox->setEnabled(is_uninitialized);
  QDateTime initial_date_time;
  initial_date_time.setSecsSinceEpoch(Config::Get(Config::MAIN_CUSTOM_RTC_VALUE));
  m_ui->customRtcDateTimeEdit->setEnabled(enable_custom_rtc_widgets);
  SignalBlocking(m_ui->customRtcDateTimeEdit)->setDateTime(initial_date_time);

  m_ui->resetSettingsButton->setEnabled(is_uninitialized);
}

void AdvancedPane::OnResetButtonClicked()
{
  if (ModalMessageBox::question(
          this, tr("Reset Dolphin Settings"),
          tr("Are you sure you want to restore all Dolphin settings to their default "
             "values? This action cannot be undone!\n"
             "All customizations or changes you have made will be lost.\n\n"
             "Do you want to proceed?"),
          ModalMessageBox::StandardButtons(ModalMessageBox::Yes | ModalMessageBox::No),
          ModalMessageBox::No, Qt::WindowModality::WindowModal) == ModalMessageBox::No)
  {
    return;
  }

  SConfig::ResetAllSettings();
  UICommon::SetUserDirectory(File::GetUserPath(D_USER_IDX));

  emit Settings::Instance().ConfigChanged();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  ShowAnalyticsPrompt(this);
#endif
}
