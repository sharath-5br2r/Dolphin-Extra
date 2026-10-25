// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GeneralWidget.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QSignalBlocker>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#include "ui_GeneralWidget.h"

GeneralWidget::GeneralWidget(GraphicsPane* gfx_pane)
    : m_ui{std::make_unique<Ui::GeneralWidget>()}, m_game_layer{gfx_pane->GetConfigLayer()}
{
  m_ui->setupUi(this);
  BindSettings();
  m_previous_backend = m_ui->backendComboBox->currentIndex();
  ToggleCustomAspectRatio(m_ui->aspectRatioComboBox->currentIndex());
  ConnectWidgets();
  AddDescriptions();

  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &GeneralWidget::OnBackendChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

GeneralWidget::~GeneralWidget() = default;

void GeneralWidget::BindSettings()
{
  std::vector<std::pair<QString, QString>> options;
  for (auto& backend : VideoBackendBase::GetAvailableBackends())
  {
    options.push_back(std::make_pair(tr(backend->GetDisplayName().c_str()),
                                     QString::fromStdString(backend->GetConfigName())));
  }
  ConfigWidget::BindStringChoice(m_ui->backendComboBox, Config::MAIN_GFX_BACKEND, options,
                                 m_game_layer);
  ConfigWidget::Bind(m_ui->aspectRatioComboBox, Config::GFX_ASPECT_RATIO, m_game_layer);
  ConfigWidget::Bind(m_ui->customAspectWidthSpinBox, Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->customAspectHeightSpinBox, Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->integerScalingCheckBox, Config::GFX_INTEGER_SCALING, m_game_layer);
  ConfigWidget::Bind(m_ui->vsyncCheckBox, Config::GFX_VSYNC, m_game_layer);
  ConfigWidget::Bind(m_ui->fullscreenCheckBox, Config::MAIN_FULLSCREEN, m_game_layer);
  ConfigWidget::Bind(m_ui->precisionFrameTimingCheckBox, Config::MAIN_PRECISION_FRAME_TIMING);
  ConfigWidget::Bind(m_ui->autoAdjustWindowSizeCheckBox, Config::MAIN_RENDER_WINDOW_AUTOSIZE,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->renderToMainWindowCheckBox, Config::MAIN_RENDER_TO_MAIN, m_game_layer);
  ConfigWidget::Bind(m_ui->specializedShaderRadioButton, Config::GFX_SHADER_COMPILATION_MODE, 0,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->exclusiveUbershadersRadioButton, Config::GFX_SHADER_COMPILATION_MODE, 1,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->hybridUbershadersRadioButton, Config::GFX_SHADER_COMPILATION_MODE, 2,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->skipDrawingRadioButton, Config::GFX_SHADER_COMPILATION_MODE, 3,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->waitForShadersCheckBox, Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING,
                     m_game_layer);
}

void GeneralWidget::ConnectWidgets()
{
  // Video Backend
  connect(m_ui->backendComboBox, &QComboBox::currentIndexChanged, this,
          &GeneralWidget::BackendWarning);
  connect(m_ui->adapterComboBox, &QComboBox::currentIndexChanged, this, [&](int index) {
    Config::SetBaseOrCurrent(Config::GFX_ADAPTER, index);
    emit BackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
  });
  connect(m_ui->aspectRatioComboBox, &QComboBox::currentIndexChanged, this,
          &GeneralWidget::ToggleCustomAspectRatio);
}

void GeneralWidget::ToggleCustomAspectRatio(int index)
{
  const AspectMode aspect_mode =
      m_game_layer == nullptr ?
          static_cast<AspectMode>(index) :
          ConfigWidget::Logic::ReadValue(Config::GFX_ASPECT_RATIO, m_game_layer);
  const bool is_custom_aspect_ratio =
      aspect_mode == AspectMode::Custom || aspect_mode == AspectMode::CustomStretch;
  m_ui->customAspectRatioLabel->setHidden(!is_custom_aspect_ratio);
  m_ui->customAspectWidthSpinBox->setHidden(!is_custom_aspect_ratio);
  m_ui->customAspectHeightSpinBox->setHidden(!is_custom_aspect_ratio);
}

void GeneralWidget::BackendWarning()
{
  const std::string configured_backend =
      ConfigWidget::Logic::ReadValue(Config::MAIN_GFX_BACKEND, m_game_layer);
  if (!ConfigWidget::IsInherited(m_ui->backendComboBox) &&
      Config::GetActiveLayerForConfig(Config::MAIN_GFX_BACKEND) == Config::LayerType::Base)
  {
    const auto& backends = VideoBackendBase::GetAvailableBackends();
    if (backends.empty())
    {
      return;
    }

    const auto backend_it =
        std::ranges::find_if(backends, [&configured_backend](const auto& backend) {
          return backend->GetConfigName() == configured_backend;
        });
    if (backend_it == backends.end())
    {
      // Don't attempt to get the current backend if it doesn't match any available backends.
      return;
    }

    auto warningMessage = (*backend_it)->GetWarningMessage();
    if (warningMessage)
    {
      ModalMessageBox confirm_sw(this);

      confirm_sw.setIcon(QMessageBox::Warning);
      confirm_sw.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      confirm_sw.setWindowTitle(tr("Confirm backend change"));
      confirm_sw.setText(tr(warningMessage->c_str()));

      if (confirm_sw.exec() != QMessageBox::Yes)
      {
        m_ui->backendComboBox->setCurrentIndex(m_previous_backend);
        return;
      }
    }
  }

  m_previous_backend = m_ui->backendComboBox->currentIndex();
  emit BackendChanged(QString::fromStdString(configured_backend));
}

void GeneralWidget::OnEmulationStateChanged(bool running)
{
  m_ui->backendComboBox->setEnabled(!running);
  m_ui->renderToMainWindowCheckBox->setEnabled(!running);
  m_ui->fullscreenCheckBox->setEnabled(!running);

  const bool supports_adapters = !g_backend_info.Adapters.empty();
  m_ui->adapterComboBox->setEnabled(!running && supports_adapters);

  const std::string configured_backend =
      ConfigWidget::Logic::ReadValue(Config::MAIN_GFX_BACKEND, m_game_layer);
  if (Config::Get(Config::MAIN_GFX_BACKEND) != configured_backend)
  {
    emit BackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
  }
}

void GeneralWidget::AddDescriptions()
{
  // We need QObject::tr
  static const char TR_BACKEND_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which graphics API to use internally.<br><br>The software renderer is extremely "
      "slow and only useful for debugging, so any of the other backends are "
      "recommended. Different games and different GPUs will behave differently on each "
      "backend, so for the best emulation experience it is recommended to try each and "
      "select the backend that is least problematic.<br><br><dolphin_emphasis>If unsure, "
      "select %1.</dolphin_emphasis>");
  static const char TR_FULLSCREEN_DESCRIPTION[] =
      QT_TR_NOOP("Uses the entire screen for rendering.<br><br>If disabled, a "
                 "render window will be created instead.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_AUTOSIZE_DESCRIPTION[] =
      QT_TR_NOOP("Automatically adjusts the window size to the internal resolution.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_RENDER_TO_MAINWINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Uses the main Dolphin window for rendering rather than "
                 "a separate render window.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_ASPECT_RATIO_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which aspect ratio to use for displaying the game."
      "<br><br>The aspect ratio of the image sent out by the original consoles varied depending on "
      "the game and rarely exactly matched 4:3 or 16:9. Some of the image would be cut off by the "
      "edges of the TV, or the image wouldn't fill the TV entirely. By default, Dolphin shows the "
      "whole image without distorting its proportions, which means it's normal for the image to "
      "not entirely fill your display."
      "<br><br><b>Auto</b>: Mimics a TV with either a 4:3 or 16:9 aspect ratio, depending on which "
      "type of TV the game seems to be targeting."
      "<br><br><b>Force 16:9</b>: Mimics a TV with a 16:9 (widescreen) aspect ratio."
      "<br><br><b>Force 4:3</b>: Mimics a TV with a 4:3 aspect ratio."
      "<br><br><b>Stretch to Window</b>: Stretches the image to the window size. "
      "This will usually distort the image's proportions."
      "<br><br><b>Custom</b>: Mimics a TV with the specified aspect ratio. "
      "This is mostly intended to be used with aspect ratio cheats/mods."
      "<br><br><b>Custom (Stretch)</b>: Similar to `Custom`, but stretches the image to the "
      "specified aspect ratio. This will usually distort the image's proportions, and should not "
      "be used under normal circumstances."
      "<br><br><dolphin_emphasis>If unsure, select Auto.</dolphin_emphasis>");
  static const char TR_VSYNC_DESCRIPTION[] = QT_TR_NOOP(
      "Waits for vertical blanks in order to prevent tearing.<br><br>Decreases performance "
      "if emulation speed is below 100%.<br><br><dolphin_emphasis>If unsure, leave "
      "this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_INTEGER_SCALING_DESCRIPTION[] = QT_TR_NOOP(
      "Scales the game image by a whole-number multiple of its rendered resolution whenever the "
      "window is large enough. This produces more uniform pixels and gives post-processing "
      "shaders an integer-sized output, at the cost of larger black borders."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHADER_COMPILE_SPECIALIZED_DESCRIPTION[] =
      QT_TR_NOOP("Ubershaders are never used. Stuttering will occur during shader "
                 "compilation, but GPU demands are low.<br><br>Recommended for low-end hardware. "
                 "<br><br><dolphin_emphasis>If unsure, select this mode.</dolphin_emphasis>");
  // The "very powerful GPU" mention below is by 2021 PC GPU standards
  static const char TR_SHADER_COMPILE_EXCLUSIVE_UBER_DESCRIPTION[] = QT_TR_NOOP(
      "Ubershaders will always be used. Provides a near stutter-free experience at the cost of "
      "very high GPU performance requirements.<br><br><dolphin_emphasis>Don't use this unless you "
      "encountered stuttering with Hybrid Ubershaders and have a very powerful "
      "GPU.</dolphin_emphasis>");
  static const char TR_SHADER_COMPILE_HYBRID_UBER_DESCRIPTION[] = QT_TR_NOOP(
      "Ubershaders will be used to prevent stuttering during shader compilation, but "
      "specialized shaders will be used when they will not cause stuttering.<br><br>In the "
      "best case it eliminates shader compilation stuttering while having minimal "
      "performance impact, but results depend on video driver behavior.");
  static const char TR_SHADER_COMPILE_SKIP_DRAWING_DESCRIPTION[] = QT_TR_NOOP(
      "Prevents shader compilation stuttering by not rendering waiting objects. Can work in "
      "scenarios where Ubershaders doesn't, at the cost of introducing visual glitches and broken "
      "effects.<br><br><dolphin_emphasis>Not recommended, only use if the other "
      "options give poor results.</dolphin_emphasis>");
  static const char TR_SHADER_COMPILE_BEFORE_START_DESCRIPTION[] =
      QT_TR_NOOP("Waits for all shaders to finish compiling before starting a game. Enabling this "
                 "option may reduce stuttering or hitching for a short time after the game is "
                 "started, at the cost of a longer delay before the game starts. For systems with "
                 "two or fewer cores, it is recommended to enable this option, as a large shader "
                 "queue may reduce frame rates.<br><br><dolphin_emphasis>Otherwise, if "
                 "unsure, leave this unchecked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(
      m_ui->backendComboBox, tr("Backend"),
      tr(TR_BACKEND_DESCRIPTION)
          .arg(QString::fromStdString(VideoBackendBase::GetDefaultBackendDisplayName())));

  ConfigWidget::SetDescription(m_ui->adapterComboBox, tr("Adapter"), {});
  ConfigWidget::SetDescription(m_ui->aspectRatioComboBox, tr("Aspect Ratio"),
                               tr(TR_ASPECT_RATIO_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->customAspectWidthSpinBox, tr("Custom Aspect Ratio Width"), {});
  ConfigWidget::SetDescription(m_ui->customAspectHeightSpinBox, tr("Custom Aspect Ratio Height"),
                               {});
  ConfigWidget::SetDescription(m_ui->integerScalingCheckBox, {},
                               tr(TR_INTEGER_SCALING_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->vsyncCheckBox, {}, tr(TR_VSYNC_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->fullscreenCheckBox, {}, tr(TR_FULLSCREEN_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->precisionFrameTimingCheckBox, {},
                               tr("Uses high resolution timers and \"busy waiting\" for improved "
                                  "frame pacing.<br><br>This will marginally increase power usage."
                                  "<br><br><dolphin_emphasis>If unsure, leave this "
                                  "checked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(m_ui->autoAdjustWindowSizeCheckBox, {}, tr(TR_AUTOSIZE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->renderToMainWindowCheckBox, {},
                               tr(TR_RENDER_TO_MAINWINDOW_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->specializedShaderRadioButton, {},
                               tr(TR_SHADER_COMPILE_SPECIALIZED_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->exclusiveUbershadersRadioButton, {},
                               tr(TR_SHADER_COMPILE_EXCLUSIVE_UBER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->hybridUbershadersRadioButton, {},
                               tr(TR_SHADER_COMPILE_HYBRID_UBER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->skipDrawingRadioButton, {},
                               tr(TR_SHADER_COMPILE_SKIP_DRAWING_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->waitForShadersCheckBox, {},
                               tr(TR_SHADER_COMPILE_BEFORE_START_DESCRIPTION));
}

void GeneralWidget::OnBackendChanged(const QString& backend_name)
{
  const QSignalBlocker blocker(m_ui->adapterComboBox);

  m_ui->adapterComboBox->clear();

  const auto& adapters = g_backend_info.Adapters;

  for (const auto& adapter : adapters)
    m_ui->adapterComboBox->addItem(QString::fromStdString(adapter));

  const bool supports_adapters = !adapters.empty();

  const int adapter_index = Config::Get(Config::GFX_ADAPTER);
  if (adapter_index < m_ui->adapterComboBox->count())
    m_ui->adapterComboBox->setCurrentIndex(adapter_index);

  m_ui->adapterComboBox->setEnabled(supports_adapters &&
                                    Core::IsUninitialized(Core::System::GetInstance()));

  static constexpr char TR_ADAPTER_AVAILABLE_DESCRIPTION[] =
      QT_TR_NOOP("Selects a hardware adapter to use.<br><br>"
                 "<dolphin_emphasis>If unsure, select the first one.</dolphin_emphasis>");
  static constexpr char TR_ADAPTER_UNAVAILABLE_DESCRIPTION[] =
      QT_TR_NOOP("Selects a hardware adapter to use.<br><br>"
                 "<dolphin_emphasis>%1 doesn't support this feature.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->adapterComboBox, tr("Adapter"),
                               supports_adapters ?
                                   tr(TR_ADAPTER_AVAILABLE_DESCRIPTION) :
                                   tr(TR_ADAPTER_UNAVAILABLE_DESCRIPTION)
                                       .arg(tr(g_video_backend->GetDisplayName().c_str())));
}
