// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AudioPane.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QSlider>
#include <QString>
#include <QWidget>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Enums.h"
#include "AudioCommon/WASAPIStream.h"

#ifdef HAVE_CUBEB
#include "AudioCommon/CubebUtils.h"
#endif

#include "Core/Config/MainSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Settings.h"

#include "ui_AudioPane.h"

static QString GetVolumeLabelText(int volume_level)
{
  return QWidget::tr("%1%").arg(volume_level);
}

AudioPane::AudioPane(QWidget* parent) : QWidget(parent), m_ui(std::make_unique<Ui::AudioPane>())
{
  m_ui->setupUi(this);
  CheckNeedForLatencyControl();
  BindSettings();
  ConfigureLayout();
  AddDescriptions();
  ConnectWidgets();
  OnBackendChanged();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });

  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

AudioPane::~AudioPane() = default;

void AudioPane::BindSettings()
{
  auto* const dsp_binding = ConfigWidget::BindComplex(m_ui->dspEngineComboBox, Config::MAIN_DSP_HLE,
                                                      Config::MAIN_DSP_JIT);
  dsp_binding->Add(tr("HLE (recommended)"), true, true);
  dsp_binding->Add(tr("LLE Recompiler (slow)"), false, true);
  dsp_binding->Add(tr("LLE Interpreter (very slow)"), false, false);
  // The state true/false shouldn't normally happen, but is HLE (index 0) when it does.
  dsp_binding->SetDefault(0);

  ConfigWidget::Bind(m_ui->volumeSlider, Config::MAIN_AUDIO_VOLUME);

  const std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  std::vector<std::pair<QString, QString>> translated_backends;
  translated_backends.reserve(backends.size());
  for (const std::string& backend : backends)
    translated_backends.emplace_back(tr(backend.c_str()), QString::fromStdString(backend));
  ConfigWidget::BindStringChoice(m_ui->backendComboBox, Config::MAIN_AUDIO_BACKEND,
                                 translated_backends);

  ConfigWidget::Bind(m_ui->dolbyProLogicCheckBox, Config::MAIN_DPL2_DECODER);
  constexpr std::array quality_values{AudioCommon::DPL2Quality::Lowest,
                                      AudioCommon::DPL2Quality::Low, AudioCommon::DPL2Quality::High,
                                      AudioCommon::DPL2Quality::Highest};
  ConfigWidget::BindMapped(m_ui->dolbyQualityComboBox, Config::MAIN_DPL2_QUALITY,
                           std::span<const AudioCommon::DPL2Quality>{quality_values});

#ifdef _WIN32
  std::vector<std::pair<QString, QString>> wasapi_options;
  const auto default_device_config_value =
      QString::fromStdString(Config::MAIN_WASAPI_DEVICE.GetDefaultValue());
  wasapi_options.emplace_back(tr("Default Device"), default_device_config_value);

  for (auto string : WASAPIStream::GetAvailableDevices())
  {
    wasapi_options.push_back(std::pair<QString, QString>{QString::fromStdString(string),
                                                         QString::fromStdString(string)});
  }

  ConfigWidget::BindStringChoice(m_ui->wasapiDeviceComboBox, Config::MAIN_WASAPI_DEVICE,
                                 wasapi_options);
#endif

  if (m_latency_control_supported)
    ConfigWidget::Bind(m_ui->latencySlider, Config::MAIN_AUDIO_LATENCY);

  ConfigWidget::Bind(m_ui->audioBufferSizeSlider, Config::MAIN_AUDIO_BUFFER_SIZE);
  ConfigWidget::MirrorFont(m_ui->audioBufferSizeLabel, m_ui->audioBufferSizeSlider);
  ConfigWidget::Bind(m_ui->audioFillGapsCheckBox, Config::MAIN_AUDIO_FILL_GAPS);
  ConfigWidget::Bind(m_ui->audioPreservePitchCheckBox, Config::MAIN_AUDIO_PRESERVE_PITCH);
  ConfigWidget::Bind(m_ui->muteOnUnlimitedSpeedCheckBox,
                     Config::MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT);

#ifdef HAVE_CUBEB
  ConfigWidget::Bind(m_ui->wiimoteRoutingCheckBox, Config::MAIN_WIIMOTE_AUDIO_ROUTING_ENABLED);

  std::vector<std::pair<QString, QString>> output_devices;
  output_devices.emplace_back(tr("Default Device"), QStringLiteral(""));
  for (const auto& [id, name] : CubebUtils::ListOutputDevices())
    output_devices.emplace_back(QString::fromStdString(name), QString::fromStdString(id));

  const std::array output_checkboxes{m_ui->wiimote1CheckBox, m_ui->wiimote2CheckBox,
                                     m_ui->wiimote3CheckBox, m_ui->wiimote4CheckBox};
  const std::array output_combos{m_ui->wiimote1DeviceComboBox, m_ui->wiimote2DeviceComboBox,
                                 m_ui->wiimote3DeviceComboBox, m_ui->wiimote4DeviceComboBox};
  for (std::size_t i = 0; i < 4; ++i)
  {
    output_checkboxes[i]->setText(tr("Wii Remote %1").arg(i + 1));
    ConfigWidget::Bind(output_checkboxes[i], Config::MAIN_WIIMOTE_AUDIO_OUTPUT_ENABLED[i]);
    ConfigWidget::BindStringChoice(output_combos[i], Config::MAIN_WIIMOTE_AUDIO_OUTPUT_DEVICE[i],
                                   output_devices);
  }

  ConfigWidget::Bind(m_ui->gbaRoutingCheckBox, Config::MAIN_GBA_AUDIO_ROUTING_ENABLED);

  const std::array gba_checkboxes{m_ui->gba1CheckBox, m_ui->gba2CheckBox, m_ui->gba3CheckBox,
                                  m_ui->gba4CheckBox};
  const std::array gba_combos{m_ui->gba1DeviceComboBox, m_ui->gba2DeviceComboBox,
                              m_ui->gba3DeviceComboBox, m_ui->gba4DeviceComboBox};
  static_assert(gba_checkboxes.size() == Config::GBA_SPEAKER_COUNT);
  for (std::size_t i = 0; i < Config::GBA_SPEAKER_COUNT; ++i)
  {
    gba_checkboxes[i]->setText(tr("GBA %1").arg(i + 1));
    ConfigWidget::Bind(gba_checkboxes[i], Config::MAIN_GBA_AUDIO_OUTPUT_ENABLED[i]);
    ConfigWidget::BindStringChoice(gba_combos[i], Config::MAIN_GBA_AUDIO_OUTPUT_DEVICE[i],
                                   output_devices);
  }
#endif
}

void AudioPane::ConfigureLayout()
{
  m_ui->dspGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_ui->playbackGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  const QFontMetrics font_metrics{font()};
  const int volume_label_width = font_metrics.boundingRect(GetVolumeLabelText(100)).width();
  // Ensure the label is at least as wide as the QGroupBox title.
  // This prevents [-Volume] title ugliness on Windows.
  const int volume_title_width = font_metrics.boundingRect(m_ui->volumeGroup->title()).width();
  m_ui->volumeValueLabel->setFixedWidth(std::max(volume_label_width, volume_title_width));
  m_ui->volumeValueLabel->setText(GetVolumeLabelText(m_ui->volumeSlider->value()));

  m_ui->audioBufferSizeValueLabel->setFixedWidth(font_metrics.boundingRect(tr(" 000 ms")).width());
  m_ui->audioBufferSizeValueLabel->setText(tr("%1 ms").arg(m_ui->audioBufferSizeSlider->value()));

  m_ui->latencyLabel->setFixedWidth(font_metrics.boundingRect(tr("Latency:  000 ms")).width());
  m_ui->latencyLabel->setText(tr("Latency: %1 ms").arg(m_ui->latencySlider->value()));
  if (!m_latency_control_supported)
  {
    m_ui->latencyLabel->hide();
    m_ui->latencySlider->hide();
  }

#ifndef _WIN32
  m_ui->wasapiDeviceLabel->hide();
  m_ui->wasapiDeviceComboBox->hide();
#endif

#ifndef HAVE_CUBEB
  m_ui->wiimoteRoutingGroup->hide();
  m_ui->gbaRoutingGroup->hide();
#endif
}

void AudioPane::ConnectWidgets()
{
  connect(m_ui->backendComboBox, &QComboBox::currentIndexChanged, this,
          &AudioPane::OnBackendChanged);
  connect(m_ui->dolbyProLogicCheckBox, &QCheckBox::toggled, this, &AudioPane::OnDspChanged);
  connect(m_ui->dspEngineComboBox, &QComboBox::currentIndexChanged, this, &AudioPane::OnDspChanged);
  connect(m_ui->volumeSlider, &QSlider::valueChanged, this, [this](int value) {
    m_ui->volumeValueLabel->setText(GetVolumeLabelText(value));
    AudioCommon::UpdateSoundStream(Core::System::GetInstance());
  });

  connect(m_ui->audioBufferSizeSlider, &QSlider::valueChanged, this, [this](int value) {
    const int stepped_value = (value / 8) * 8;
    m_ui->audioBufferSizeSlider->setValue(stepped_value);
    m_ui->audioBufferSizeValueLabel->setText(tr("%1 ms").arg(stepped_value));
  });

  if (m_latency_control_supported)
  {
    connect(m_ui->latencySlider, &QSlider::valueChanged, this,
            [this](int value) { m_ui->latencyLabel->setText(tr("Latency: %1 ms").arg(value)); });
  }

#ifdef HAVE_CUBEB
  connect(m_ui->wiimoteRoutingCheckBox, &QCheckBox::toggled, this,
          [this](bool) { UpdateWiimoteRoutingEnabled(); });
  for (QCheckBox* checkbox : {m_ui->wiimote1CheckBox, m_ui->wiimote2CheckBox,
                              m_ui->wiimote3CheckBox, m_ui->wiimote4CheckBox})
  {
    connect(checkbox, &QCheckBox::toggled, this, [this](bool) { UpdateWiimoteRoutingEnabled(); });
  }
  connect(m_ui->gbaRoutingCheckBox, &QCheckBox::toggled, this,
          [this](bool) { UpdateGBARoutingEnabled(); });
  for (QCheckBox* checkbox :
       {m_ui->gba1CheckBox, m_ui->gba2CheckBox, m_ui->gba3CheckBox, m_ui->gba4CheckBox})
  {
    connect(checkbox, &QCheckBox::toggled, this, [this](bool) { UpdateGBARoutingEnabled(); });
  }
  // Also react to external config changes: wiimote source type, speaker data, BT passthrough.
  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &AudioPane::UpdateWiimoteRoutingEnabled);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &AudioPane::UpdateGBARoutingEnabled);
  UpdateWiimoteRoutingEnabled();
  UpdateGBARoutingEnabled();
#endif
}

void AudioPane::OnDspChanged()
{
  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);
  const bool enabled =
      AudioCommon::SupportsDPL2Decoder(backend) && !Config::Get(Config::MAIN_DSP_HLE);
  m_ui->dolbyProLogicCheckBox->setEnabled(enabled);
  m_ui->dolbyQualityLabel->setEnabled(enabled && m_ui->dolbyProLogicCheckBox->isChecked());
  m_ui->dolbyQualityComboBox->setEnabled(enabled && m_ui->dolbyProLogicCheckBox->isChecked());
}

void AudioPane::OnBackendChanged()
{
  OnDspChanged();

  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);

  if (m_latency_control_supported)
  {
    m_ui->latencyLabel->setEnabled(AudioCommon::SupportsLatencyControl(backend));
    m_ui->latencySlider->setEnabled(AudioCommon::SupportsLatencyControl(backend));
  }

#ifdef _WIN32
  const bool is_wasapi = backend == BACKEND_WASAPI;
  m_ui->wasapiDeviceLabel->setHidden(!is_wasapi);
  m_ui->wasapiDeviceComboBox->setHidden(!is_wasapi);
#endif

  m_ui->volumeSlider->setEnabled(AudioCommon::SupportsVolumeChanges(backend));
  m_ui->volumeValueLabel->setEnabled(AudioCommon::SupportsVolumeChanges(backend));

#ifdef HAVE_CUBEB
  UpdateWiimoteRoutingEnabled();
  UpdateGBARoutingEnabled();
#endif
}

void AudioPane::OnEmulationStateChanged(bool running)
{
  m_ui->dspEngineComboBox->setEnabled(!running);
  m_ui->backendLabel->setEnabled(!running);
  m_ui->backendComboBox->setEnabled(!running);
  if (AudioCommon::SupportsDPL2Decoder(Config::Get(Config::MAIN_AUDIO_BACKEND)) &&
      !Config::Get(Config::MAIN_DSP_HLE))
  {
    m_ui->dolbyProLogicCheckBox->setEnabled(!running);
    m_ui->dolbyQualityLabel->setEnabled(!running && m_ui->dolbyProLogicCheckBox->isChecked());
    m_ui->dolbyQualityComboBox->setEnabled(!running && m_ui->dolbyProLogicCheckBox->isChecked());
  }
  if (m_latency_control_supported &&
      AudioCommon::SupportsLatencyControl(Config::Get(Config::MAIN_AUDIO_BACKEND)))
  {
    m_ui->latencyLabel->setEnabled(!running);
    m_ui->latencySlider->setEnabled(!running);
  }

#ifdef _WIN32
  m_ui->wasapiDeviceComboBox->setEnabled(!running);
#endif

#ifdef HAVE_CUBEB
  UpdateWiimoteRoutingEnabled();
  UpdateGBARoutingEnabled();
#endif
}

void AudioPane::UpdateWiimoteRoutingEnabled()
{
#ifdef HAVE_CUBEB
  const bool running = Core::GetState(Core::System::GetInstance()) != Core::State::Uninitialized;
  const bool is_cubeb = Config::Get(Config::MAIN_AUDIO_BACKEND) == BACKEND_CUBEB;
  const bool speaker_enabled = Config::Get(Config::MAIN_WIIMOTE_ENABLE_SPEAKER);
  const bool bt_passthrough = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED);

  // The entire group requires Cubeb backend, speaker data enabled, no BT passthrough, and
  // emulation not running.
  const bool group_usable = !running && is_cubeb && speaker_enabled && !bt_passthrough;

  m_ui->wiimoteRoutingCheckBox->setEnabled(group_usable);

  const bool routing_on = group_usable && m_ui->wiimoteRoutingCheckBox->isChecked();

  const std::array output_checkboxes{m_ui->wiimote1CheckBox, m_ui->wiimote2CheckBox,
                                     m_ui->wiimote3CheckBox, m_ui->wiimote4CheckBox};
  const std::array output_combos{m_ui->wiimote1DeviceComboBox, m_ui->wiimote2DeviceComboBox,
                                 m_ui->wiimote3DeviceComboBox, m_ui->wiimote4DeviceComboBox};
  for (std::size_t i = 0; i < 4; ++i)
  {
    const WiimoteSource source = Config::Get(Config::GetInfoForWiimoteSource(static_cast<int>(i)));
    const bool is_emulated = source == WiimoteSource::Emulated;

    output_checkboxes[i]->setEnabled(routing_on && is_emulated);
    output_combos[i]->setEnabled(routing_on && is_emulated && output_checkboxes[i]->isChecked());
  }
#endif
}

void AudioPane::UpdateGBARoutingEnabled()
{
#ifdef HAVE_CUBEB
  const bool running = Core::GetState(Core::System::GetInstance()) != Core::State::Uninitialized;
  const bool is_cubeb = Config::Get(Config::MAIN_AUDIO_BACKEND) == BACKEND_CUBEB;
  const bool group_usable = !running && is_cubeb;

  m_ui->gbaRoutingCheckBox->setEnabled(group_usable);

  const bool routing_on = group_usable && m_ui->gbaRoutingCheckBox->isChecked();

  const std::array gba_checkboxes{m_ui->gba1CheckBox, m_ui->gba2CheckBox, m_ui->gba3CheckBox,
                                  m_ui->gba4CheckBox};
  const std::array gba_combos{m_ui->gba1DeviceComboBox, m_ui->gba2DeviceComboBox,
                              m_ui->gba3DeviceComboBox, m_ui->gba4DeviceComboBox};
  for (std::size_t i = 0; i < Config::GBA_SPEAKER_COUNT; ++i)
  {
    const u32 device_type = Config::Get(Config::GetInfoForSIDevice(static_cast<u32>(i)));
    const bool is_gba_connected = static_cast<SerialInterface::SIDevices>(device_type) ==
                                  SerialInterface::SIDEVICE_GC_GBA_EMULATED;

    gba_checkboxes[i]->setEnabled(routing_on && is_gba_connected);
    gba_combos[i]->setEnabled(routing_on && is_gba_connected && gba_checkboxes[i]->isChecked());
  }
#endif
}

void AudioPane::CheckNeedForLatencyControl()
{
  std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  m_latency_control_supported = std::ranges::any_of(backends, AudioCommon::SupportsLatencyControl);
}

void AudioPane::AddDescriptions()
{
  static const char TR_DSP_DESCRIPTION[] = QT_TR_NOOP(
      "Selects how the Digital Signal Processor (DSP) is emulated. Determines how the audio is "
      "processed and what system features are available.<br><br>"
      "<b>HLE</b> - High Level Emulation of the DSP. Fast, but not always accurate. Lacks Dolby "
      "Pro Logic II decoding.<br><br>"
      "<b>LLE Recompiler</b> - Low Level Emulation of the DSP, via a recompiler. Slower, but more "
      "accurate. Enables Dolby Pro Logic II decoding on certain audio backends.<br><br>"
      "<b>LLE Interpreter</b> - Low Level Emulation of the DSP, via an interpreter. Slowest, for "
      "debugging purposes only. Not recommended.<br><br><dolphin_emphasis>If unsure, select "
      "HLE.</dolphin_emphasis>");
  static const char TR_AUDIO_BACKEND_DESCRIPTION[] =
      QT_TR_NOOP("Selects which audio API to use internally.<br><br><dolphin_emphasis>If unsure, "
                 "select %1.</dolphin_emphasis>");
  static const char TR_LATENCY_SLIDER_DESCRIPTION[] = QT_TR_NOOP(
      "Sets the audio latency in milliseconds. Higher values may reduce audio crackling. Certain "
      "backends only.<br><br><dolphin_emphasis>If unsure, leave this at 20 ms.</dolphin_emphasis>");
  static const char TR_DOLBY_DESCRIPTION[] =
      QT_TR_NOOP("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain backends only. "
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DOLBY_OPTIONS_DESCRIPTION[] = QT_TR_NOOP(
      "Adjusts the quality setting of the Dolby Pro Logic II decoder. Higher presets increases "
      "audio latency.<br><br><dolphin_emphasis>If unsure, select High.</dolphin_emphasis>");
  static const char TR_VOLUME_DESCRIPTION[] =
      QT_TR_NOOP("Adjusts audio output volume.<br><br><dolphin_emphasis>If unsure, leave this at "
                 "100%.</dolphin_emphasis>");
  static const char TR_AUDIO_BUFFER_SIZE_DESCRIPTION[] = QT_TR_NOOP(
      "Controls the number of audio samples buffered. Lower values reduce latency but may cause "
      "more crackling or stuttering.<br><br><dolphin_emphasis>If unsure, set this to 80 "
      "ms.</dolphin_emphasis>");
  static const char TR_FILL_AUDIO_GAPS_DESCRIPTION[] = QT_TR_NOOP(
      "Repeat existing audio during lag spikes to prevent stuttering.<br><br><dolphin_emphasis>If "
      "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_PRESERVE_AUDIO_PITCH_DESCRIPTION[] = QT_TR_NOOP(
      "Keeps audio at normal pitch when changing emulation speed. Without this, audio pitch "
      "changes proportionally with speed.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_SPEED_UP_MUTE_DESCRIPTION[] =
      QT_TR_NOOP("Mutes the audio when overriding the emulation speed limit (default hotkey: Tab). "
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->dspEngineComboBox, tr("DSP Emulation Engine"),
                               tr(TR_DSP_DESCRIPTION));

  ConfigWidget::SetDescription(
      m_ui->backendComboBox, tr("Audio Backend"),
      tr(TR_AUDIO_BACKEND_DESCRIPTION)
          .arg(QString::fromStdString(AudioCommon::GetDefaultSoundBackend())));

  ConfigWidget::SetDescription(m_ui->dolbyProLogicCheckBox, tr("Dolby Pro Logic II Decoder"),
                               tr(TR_DOLBY_DESCRIPTION));

  ConfigWidget::SetDescription(m_ui->dolbyQualityComboBox, tr("Decoding Quality"),
                               tr(TR_DOLBY_OPTIONS_DESCRIPTION));

#ifdef _WIN32
  static const char TR_WASAPI_DEVICE_DESCRIPTION[] =
      QT_TR_NOOP("Selects an output device to use.<br><br><dolphin_emphasis>If unsure, select "
                 "Default Device.</dolphin_emphasis>");
  ConfigWidget::SetDescription(m_ui->wasapiDeviceComboBox, tr("Output Device"),
                               tr(TR_WASAPI_DEVICE_DESCRIPTION));
#endif

  ConfigWidget::SetDescription(m_ui->volumeSlider, tr("Volume"), tr(TR_VOLUME_DESCRIPTION));

  if (m_latency_control_supported)
  {
    ConfigWidget::SetDescription(m_ui->latencySlider, tr("Latency"),
                                 tr(TR_LATENCY_SLIDER_DESCRIPTION));
  }

  ConfigWidget::SetDescription(m_ui->audioBufferSizeSlider, QString{},
                               tr(TR_AUDIO_BUFFER_SIZE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->muteOnUnlimitedSpeedCheckBox,
                               tr("Mute When Disabling Speed Limit"),
                               tr(TR_SPEED_UP_MUTE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->audioFillGapsCheckBox, tr("Fill Audio Gaps"),
                               tr(TR_FILL_AUDIO_GAPS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->audioPreservePitchCheckBox, tr("Preserve Audio Pitch"),
                               tr(TR_PRESERVE_AUDIO_PITCH_DESCRIPTION));

#ifdef HAVE_CUBEB
  static const char TR_WIIMOTE_ROUTING_DESCRIPTION[] =
      QT_TR_NOOP("Routes each Wii Remote's speaker audio to a separate audio output device. "
                 "This setting cannot be changed while emulation is active."
                 "<br><br>This setting is disabled when Enable Speaker Data is disabled, "
                 "Passthrough a Bluetooth adapter is selected, or an audio backend other than "
                 "Cubeb is selected."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_WIIMOTE_OUTPUT_ENABLE_DESCRIPTION[] =
      QT_TR_NOOP("Enables routing this Wii Remote's speaker audio to a separate output device.");
  static const char TR_WIIMOTE_OUTPUT_DEVICE_DESCRIPTION[] =
      QT_TR_NOOP("Selects the audio output device for this Wii Remote's speaker audio."
                 "<br><br>This setting is disabled when this Wii Remote is not set to Emulated "
                 "Wii Remote.");

  ConfigWidget::SetDescription(m_ui->wiimoteRoutingCheckBox, tr("Enable Wii Remote Audio Routing"),
                               tr(TR_WIIMOTE_ROUTING_DESCRIPTION));
  const std::array output_checkboxes{m_ui->wiimote1CheckBox, m_ui->wiimote2CheckBox,
                                     m_ui->wiimote3CheckBox, m_ui->wiimote4CheckBox};
  const std::array output_combos{m_ui->wiimote1DeviceComboBox, m_ui->wiimote2DeviceComboBox,
                                 m_ui->wiimote3DeviceComboBox, m_ui->wiimote4DeviceComboBox};
  for (std::size_t i = 0; i < 4; ++i)
  {
    ConfigWidget::SetDescription(output_checkboxes[i], QString{},
                                 tr(TR_WIIMOTE_OUTPUT_ENABLE_DESCRIPTION));
    ConfigWidget::SetDescription(output_combos[i], tr("Wii Remote %1").arg(i + 1),
                                 tr(TR_WIIMOTE_OUTPUT_DEVICE_DESCRIPTION));
  }

  static const char TR_GBA_ROUTING_DESCRIPTION[] =
      QT_TR_NOOP("Routes each GBA's speaker audio to a separate audio output device. "
                 "This setting cannot be changed while emulation is active."
                 "<br><br>This setting is disabled when an audio backend other than "
                 "Cubeb is selected."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_GBA_OUTPUT_ENABLE_DESCRIPTION[] =
      QT_TR_NOOP("Enables routing this GBA's speaker audio to a separate output device.");
  static const char TR_GBA_OUTPUT_DEVICE_DESCRIPTION[] =
      QT_TR_NOOP("Selects the audio output device for this GBA's speaker audio."
                 "<br><br>This setting is disabled when this port is not set to GBA "
                 "(Integrated).");

  ConfigWidget::SetDescription(m_ui->gbaRoutingCheckBox, tr("Enable GBA Audio Routing"),
                               tr(TR_GBA_ROUTING_DESCRIPTION));
  const std::array gba_checkboxes{m_ui->gba1CheckBox, m_ui->gba2CheckBox, m_ui->gba3CheckBox,
                                  m_ui->gba4CheckBox};
  const std::array gba_combos{m_ui->gba1DeviceComboBox, m_ui->gba2DeviceComboBox,
                              m_ui->gba3DeviceComboBox, m_ui->gba4DeviceComboBox};
  for (std::size_t i = 0; i < Config::GBA_SPEAKER_COUNT; ++i)
  {
    ConfigWidget::SetDescription(gba_checkboxes[i], QString{},
                                 tr(TR_GBA_OUTPUT_ENABLE_DESCRIPTION));
    ConfigWidget::SetDescription(gba_combos[i], tr("GBA %1").arg(i + 1),
                                 tr(TR_GBA_OUTPUT_DEVICE_DESCRIPTION));
  }
#endif
}
