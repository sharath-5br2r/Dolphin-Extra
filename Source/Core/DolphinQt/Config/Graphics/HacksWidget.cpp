// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/HacksWidget.h"

#include <array>
#include <memory>

#include <QCheckBox>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"

#include "VideoCommon/VideoConfig.h"

#include "ui_HacksWidget.h"

HacksWidget::HacksWidget(GraphicsPane* gfx_pane)
    : m_ui{std::make_unique<Ui::HacksWidget>()}, m_game_layer{gfx_pane->GetConfigLayer()}
{
  m_ui->setupUi(this);
  BindSettings();
  ConnectWidgets();
  AddDescriptions();

  const auto get_backend_name = []() { return tr(Config::Get(Config::MAIN_GFX_BACKEND).data()); };

  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &HacksWidget::OnBackendChanged);
  connect(gfx_pane, &GraphicsPane::UpdateGPUTextureDecoding, this,
          [this, get_backend_name] { UpdateGPUTextureDecodingEnabled(get_backend_name()); });

  OnBackendChanged(get_backend_name());
}

HacksWidget::~HacksWidget() = default;

void HacksWidget::BindSettings()
{
  ConfigWidget::Bind(m_ui->skipEfbCpuCheckBox, Config::GFX_HACK_EFB_ACCESS_ENABLE, m_game_layer,
                     true);
  ConfigWidget::Bind(m_ui->ignoreFormatChangesCheckBox, Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES,
                     m_game_layer, true);
  ConfigWidget::Bind(m_ui->storeEfbCopiesCheckBox, Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->deferEfbCopiesCheckBox, Config::GFX_HACK_DEFER_EFB_COPIES, m_game_layer);
  constexpr std::array accuracy_values{0, 512, 128};
  ConfigWidget::BindMapped(m_ui->accuracySlider, Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES,
                           accuracy_values, m_game_layer);
  ConfigWidget::MirrorFont(m_ui->accuracyLabel, m_ui->accuracySlider);
  ConfigWidget::Bind(m_ui->gpuTextureDecodingCheckBox, Config::GFX_ENABLE_GPU_TEXTURE_DECODING,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->storeXfbCopiesCheckBox, Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->immediateXfbCheckBox, Config::GFX_HACK_IMMEDIATE_XFB, m_game_layer);
  ConfigWidget::Bind(m_ui->skipDuplicateXfbsCheckBox, Config::GFX_HACK_SKIP_DUPLICATE_XFBS,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->fastDepthCalculationCheckBox, Config::GFX_FAST_DEPTH_CALC, m_game_layer);
  ConfigWidget::Bind(m_ui->disableBoundingBoxCheckBox, Config::GFX_HACK_BBOX_ENABLE, m_game_layer,
                     true);
  ConfigWidget::Bind(m_ui->vertexRoundingCheckBox, Config::GFX_HACK_VERTEX_ROUNDING, m_game_layer);
  ConfigWidget::Bind(m_ui->saveTextureCacheStateCheckBox, Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->viSkipCheckBox, Config::GFX_HACK_VI_SKIP, m_game_layer);

  UpdateDeferEFBCopiesEnabled();
  UpdateSkipPresentingDuplicateFramesEnabled();
}

void HacksWidget::OnBackendChanged(const QString& backend_name)
{
  UpdateGPUTextureDecodingEnabled(backend_name);
  UpdateBoundingBoxEnabled(backend_name);
}

void HacksWidget::ConnectWidgets()
{
  ConfigWidget::ConnectCheckStateChanged(m_ui->storeEfbCopiesCheckBox, this,
                                         &HacksWidget::UpdateDeferEFBCopiesEnabled);
  ConfigWidget::ConnectCheckStateChanged(m_ui->storeXfbCopiesCheckBox, this,
                                         &HacksWidget::UpdateDeferEFBCopiesEnabled);
  ConfigWidget::ConnectCheckStateChanged(m_ui->immediateXfbCheckBox, this,
                                         &HacksWidget::UpdateSkipPresentingDuplicateFramesEnabled);
  ConfigWidget::ConnectCheckStateChanged(m_ui->viSkipCheckBox, this,
                                         &HacksWidget::UpdateSkipPresentingDuplicateFramesEnabled);
}

void HacksWidget::AddDescriptions()
{
  static const char TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION[] = QT_TR_NOOP(
      "Ignores any requests from the CPU to read from or write to the EFB. "
      "<br><br>Improves performance in some games, but will disable all EFB-based "
      "graphical effects or gameplay-related features.<br><br><dolphin_emphasis>If unsure, "
      "leave this checked.</dolphin_emphasis>");
  static const char TR_IGNORE_FORMAT_CHANGE_DESCRIPTION[] = QT_TR_NOOP(
      "Ignores any changes to the EFB format.<br><br>Improves performance in many games "
      "without "
      "any negative effect. Causes graphical defects in a small number of other "
      "games.<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_STORE_EFB_TO_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Stores EFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.<br><br>Enabled = EFB Copies to Texture<br>Disabled = EFB "
      "Copies to "
      "RAM (and Texture)<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_DEFER_EFB_COPIES_DESCRIPTION[] = QT_TR_NOOP(
      "Waits until the game synchronizes with the emulated GPU before writing the contents of EFB "
      "copies to RAM.<br><br>Reduces the overhead of EFB RAM copies, providing a performance "
      "boost in "
      "many games, at the risk of breaking those which do not safely synchronize with the "
      "emulated GPU.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_ACCUARCY_DESCRIPTION[] = QT_TR_NOOP(
      "Adjusts the accuracy at which the GPU receives texture updates from RAM.<br><br>"
      "The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates "
      "from RAM. Lower accuracies cause in-game text to appear garbled in certain "
      "games.<br><br><dolphin_emphasis>If unsure, select the rightmost "
      "value.</dolphin_emphasis>");
  static const char TR_STORE_XFB_TO_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Stores XFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.<br><br>Enabled = XFB Copies to "
      "Texture<br>Disabled = XFB Copies to RAM (and Texture)<br><br><dolphin_emphasis>If "
      "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_IMMEDIATE_XFB_DESCRIPTION[] = QT_TR_NOOP(
      "Displays XFB copies as soon as they are created, instead of waiting for "
      "scanout.<br><br>Can cause graphical defects in some games if the game doesn't "
      "expect all XFB copies to be displayed. However, turning this setting on reduces latency."
      "<br><br>Enabling this also forces an effect equivalent to the "
      "Skip Presenting Duplicate Frames setting."
      "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_SKIP_DUPLICATE_XFBS_DESCRIPTION[] = QT_TR_NOOP(
      "Skips presentation of duplicate frames (XFB copies) in 25fps/30fps games. "
      "This may improve performance on low-end devices, while making frame pacing less consistent."
      "<br><br>Disable this option for optimal frame pacing."
      "<br><br>This setting is unavailable when Immediately Present XFB or VBI Skip is "
      "enabled. In those cases, duplicate frames are never presented."
      "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_FAST_DEPTH_CALC_DESCRIPTION[] = QT_TR_NOOP(
      "Uses a less accurate algorithm to calculate depth values.<br><br>Causes issues in a few "
      "games, but can result in a decent speed increase depending on the game and/or "
      "GPU.<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_SAVE_TEXTURE_CACHE_TO_STATE_DESCRIPTION[] =
      QT_TR_NOOP("Includes the contents of the embedded frame buffer (EFB) and upscaled EFB copies "
                 "in save states. Fixes missing and/or non-upscaled textures/objects when loading "
                 "states at the cost of additional save/load time.<br><br><dolphin_emphasis>If "
                 "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_VERTEX_ROUNDING_DESCRIPTION[] = QT_TR_NOOP(
      "Rounds 2D vertices to whole pixels and rounds the viewport size to a whole number.<br><br>"
      "Fixes graphical problems in some games at higher internal resolutions. This setting has no "
      "effect when native internal resolution is used.<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_VI_SKIP_DESCRIPTION[] =
      QT_TR_NOOP("Skips Vertical Blank Interrupts when lag is detected, allowing for "
                 "smooth audio playback when emulation speed is not 100%. <br><br>"
                 "Enabling this also forces the effect of the"
                 " Skip Presenting Duplicate Frames setting.<br><br>"
                 "<dolphin_emphasis>WARNING: Can cause freezes and compatibility "
                 "issues.</dolphin_emphasis> <br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->skipEfbCpuCheckBox, {},
                               tr(TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->ignoreFormatChangesCheckBox, {},
                               tr(TR_IGNORE_FORMAT_CHANGE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->storeEfbCopiesCheckBox, {},
                               tr(TR_STORE_EFB_TO_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->deferEfbCopiesCheckBox, {},
                               tr(TR_DEFER_EFB_COPIES_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->accuracySlider, tr("Texture Cache Accuracy"),
                               tr(TR_ACCUARCY_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->storeXfbCopiesCheckBox, {},
                               tr(TR_STORE_XFB_TO_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->immediateXfbCheckBox, {}, tr(TR_IMMEDIATE_XFB_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->skipDuplicateXfbsCheckBox, {},
                               tr(TR_SKIP_DUPLICATE_XFBS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->fastDepthCalculationCheckBox, {},
                               tr(TR_FAST_DEPTH_CALC_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->saveTextureCacheStateCheckBox, {},
                               tr(TR_SAVE_TEXTURE_CACHE_TO_STATE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->vertexRoundingCheckBox, {},
                               tr(TR_VERTEX_ROUNDING_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->viSkipCheckBox, {}, tr(TR_VI_SKIP_DESCRIPTION));
}

void HacksWidget::UpdateGPUTextureDecodingEnabled(const QString& backend_name)
{
  static const char TR_GPU_DECODING_DESCRIPTION[] = QT_TR_NOOP(
      "Enables texture decoding using the GPU instead of the CPU.<br><br>This may result in "
      "performance gains in some scenarios, or on systems where the CPU is the bottleneck."
      "<br><br>This setting is disabled when Arbitrary Mipmap Detection is enabled.<br><br>");

  const bool gpu_texture_decoding_supported = g_backend_info.bSupportsGPUTextureDecoding;
  const bool arbitrary_mipmap_detection_enabled =
      ConfigWidget::Logic::ReadValue(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, m_game_layer);
  const bool gpu_texture_decoding_enabled =
      gpu_texture_decoding_supported && !arbitrary_mipmap_detection_enabled;
  m_ui->gpuTextureDecodingCheckBox->setEnabled(gpu_texture_decoding_enabled);

  if (!gpu_texture_decoding_supported)
  {
    ConfigWidget::SetDescription(
        m_ui->gpuTextureDecodingCheckBox, {},
        tr(TR_GPU_DECODING_DESCRIPTION) +
            tr("<dolphin_emphasis>The %1 backend doesn't support GPU Texture "
               "Decoding.</dolphin_emphasis>")
                .arg(backend_name));
  }
  else if (arbitrary_mipmap_detection_enabled)
  {
    ConfigWidget::SetDescription(
        m_ui->gpuTextureDecodingCheckBox, {},
        tr(TR_GPU_DECODING_DESCRIPTION) +
            tr("<dolphin_emphasis>GPU Texture Decoding is currently disabled by Arbitrary Mipmap "
               "Detection.</dolphin_emphasis>"));
  }
  else
  {
    ConfigWidget::SetDescription(
        m_ui->gpuTextureDecodingCheckBox, {},
        tr(TR_GPU_DECODING_DESCRIPTION) +
            tr("<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  }
}

void HacksWidget::UpdateBoundingBoxEnabled(const QString& backend_name)
{
  static const char TR_DISABLE_BOUNDINGBOX_DESCRIPTION[] =
      QT_TR_NOOP("Disables bounding box emulation.<br><br>This may improve GPU performance "
                 "significantly, but some games will break.<br><br>");

  const bool bbox = g_backend_info.bSupportsBBox;
  m_ui->disableBoundingBoxCheckBox->setEnabled(bbox);

  if (bbox)
  {
    ConfigWidget::SetDescription(
        m_ui->disableBoundingBoxCheckBox, {},
        tr(TR_DISABLE_BOUNDINGBOX_DESCRIPTION) +
            tr("<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>"));
  }
  else
  {
    ConfigWidget::SetDescription(
        m_ui->disableBoundingBoxCheckBox, {},
        tr(TR_DISABLE_BOUNDINGBOX_DESCRIPTION) +
            tr("<dolphin_emphasis>The %1 backend doesn't support Bounding Box "
               "emulation.</dolphin_emphasis>")
                .arg(backend_name));
  }
}

void HacksWidget::UpdateDeferEFBCopiesEnabled()
{
  // We disable the checkbox for defer EFB copies when both EFB and XFB copies to texture are
  // enabled.
  const bool can_defer = ConfigWidget::EffectiveChecked(m_ui->storeEfbCopiesCheckBox) &&
                         ConfigWidget::EffectiveChecked(m_ui->storeXfbCopiesCheckBox);
  m_ui->deferEfbCopiesCheckBox->setEnabled(!can_defer);
}

void HacksWidget::UpdateSkipPresentingDuplicateFramesEnabled()
{
  // If Immediate XFB is on, there's no point to skipping duplicate XFB copies as immediate presents
  // when the XFB is created, therefore all XFB copies will be unique.
  m_ui->skipDuplicateXfbsCheckBox->setDisabled(
      ConfigWidget::EffectiveChecked(m_ui->immediateXfbCheckBox) ||
      ConfigWidget::EffectiveChecked(m_ui->viSkipCheckBox));
}
