// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/AdvancedWidget.h"

#include <memory>

#include <QCheckBox>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoConfig.h"

#include "ui_AdvancedWidget.h"

AdvancedWidget::AdvancedWidget(GraphicsPane* gfx_pane)
    : m_ui{std::make_unique<Ui::AdvancedWidget>()}, m_game_layer{gfx_pane->GetConfigLayer()}
{
  m_ui->setupUi(this);
  BindSettings();
  ConnectWidgets();
  AddDescriptions();

  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &AdvancedWidget::OnBackendChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
  ConfigWidget::ConnectCheckStateChanged(m_ui->manualTextureSamplingCheckBox, gfx_pane, [gfx_pane] {
    emit gfx_pane->UseFastTextureSamplingChanged();
  });

  OnBackendChanged();
  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

AdvancedWidget::~AdvancedWidget() = default;

void AdvancedWidget::BindSettings()
{
  const bool local_edit = m_game_layer != nullptr;

  ConfigWidget::Bind(m_ui->wireframeCheckBox, Config::GFX_ENABLE_WIREFRAME, m_game_layer);
  ConfigWidget::Bind(m_ui->textureFormatOverlayCheckBox, Config::GFX_TEXFMT_OVERLAY_ENABLE,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->apiValidationCheckBox, Config::GFX_ENABLE_VALIDATION_LAYER,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->logRenderTimeCheckBox, Config::GFX_LOG_RENDER_TIME_TO_FILE,
                     m_game_layer);

  ConfigWidget::Bind(m_ui->loadCustomTexturesCheckBox, Config::GFX_HIRES_TEXTURES, m_game_layer);
  ConfigWidget::Bind(m_ui->prefetchCustomTexturesCheckBox, Config::GFX_CACHE_HIRES_TEXTURES,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->dumpEfbTargetCheckBox, Config::GFX_DUMP_EFB_TARGET);
  ConfigWidget::Bind(m_ui->dumpXfbTargetCheckBox, Config::GFX_DUMP_XFB_TARGET);
  ConfigWidget::Bind(m_ui->disableVramCopiesCheckBox, Config::GFX_HACK_DISABLE_COPY_TO_VRAM,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->enableGraphicsModsCheckBox, Config::GFX_MODS_ENABLE, m_game_layer);

  m_ui->prefetchCustomTexturesCheckBox->setEnabled(
      ConfigWidget::EffectiveChecked(m_ui->loadCustomTexturesCheckBox));

  if (local_edit)
  {
    // It's hazardous to accidentally set these in a game ini.
    m_ui->dumpEfbTargetCheckBox->setEnabled(false);
    m_ui->dumpXfbTargetCheckBox->setEnabled(false);
  }

  ConfigWidget::Bind(m_ui->dumpTexturesCheckBox, Config::GFX_DUMP_TEXTURES);
  ConfigWidget::Bind(m_ui->dumpBaseTexturesCheckBox, Config::GFX_DUMP_BASE_TEXTURES);
  ConfigWidget::Bind(m_ui->dumpMipTexturesCheckBox, Config::GFX_DUMP_MIP_TEXTURES);
  m_ui->dumpMipTexturesCheckBox->setEnabled(
      ConfigWidget::EffectiveChecked(m_ui->dumpTexturesCheckBox));
  m_ui->dumpBaseTexturesCheckBox->setEnabled(
      ConfigWidget::EffectiveChecked(m_ui->dumpTexturesCheckBox));

  if (local_edit)
  {
    // It's hazardous to accidentally set dumping in a game ini.
    m_ui->dumpTexturesCheckBox->setEnabled(false);
    m_ui->dumpBaseTexturesCheckBox->setEnabled(false);
    m_ui->dumpMipTexturesCheckBox->setEnabled(false);
  }

  ConfigWidget::Bind(m_ui->resolutionTypeComboBox, Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->pngCompressionSpinBox, Config::GFX_PNG_COMPRESSION_LEVEL, m_game_layer);

#if defined(HAVE_FFMPEG)
  ConfigWidget::Bind(m_ui->losslessCodecCheckBox, Config::GFX_USE_LOSSLESS, m_game_layer);
  ConfigWidget::Bind(m_ui->bitrateSpinBox, Config::GFX_BITRATE_KBPS, m_game_layer);
  m_ui->bitrateSpinBox->setEnabled(!ConfigWidget::EffectiveChecked(m_ui->losslessCodecCheckBox));
#else
  m_ui->losslessCodecCheckBox->hide();
  m_ui->bitrateLabel->hide();
  m_ui->bitrateSpinBox->hide();
#endif

  ConfigWidget::Bind(m_ui->cropToAspectRatioCheckBox, Config::GFX_CROP_TO_ASPECT_RATIO,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->customCropCheckBox, Config::GFX_CROP_CUSTOM, m_game_layer);
  ConfigWidget::Bind(m_ui->cropLeftSpinBox, Config::GFX_CROP_CUSTOM_LEFT, m_game_layer);
  ConfigWidget::Bind(m_ui->cropTopSpinBox, Config::GFX_CROP_CUSTOM_TOP, m_game_layer);
  ConfigWidget::Bind(m_ui->cropRightSpinBox, Config::GFX_CROP_CUSTOM_RIGHT, m_game_layer);
  ConfigWidget::Bind(m_ui->cropBottomSpinBox, Config::GFX_CROP_CUSTOM_BOTTOM, m_game_layer);
  ConfigWidget::MirrorFont(m_ui->cropLeftLabel, m_ui->cropLeftSpinBox);
  ConfigWidget::MirrorFont(m_ui->cropTopLabel, m_ui->cropTopSpinBox);
  ConfigWidget::MirrorFont(m_ui->cropRightLabel, m_ui->cropRightSpinBox);
  ConfigWidget::MirrorFont(m_ui->cropBottomLabel, m_ui->cropBottomSpinBox);
  m_ui->customCropGroup->setDisabled(!ConfigWidget::EffectiveChecked(m_ui->customCropCheckBox));

  ConfigWidget::Bind(m_ui->progressiveScanCheckBox, Config::SYSCONF_PROGRESSIVE_SCAN, m_game_layer);
  ConfigWidget::Bind(m_ui->backendMultithreadingCheckBox, Config::GFX_BACKEND_MULTITHREADING,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->preferVsExpansionCheckBox,
                     Config::GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION, m_game_layer);
  ConfigWidget::Bind(m_ui->cpuCullCheckBox, Config::GFX_CPU_CULL, m_game_layer);

#ifdef _WIN32
  ConfigWidget::Bind(m_ui->borderlessFullscreenCheckBox, Config::GFX_BORDERLESS_FULLSCREEN,
                     m_game_layer);
#else
  m_ui->borderlessFullscreenCheckBox->hide();
#endif

  ConfigWidget::Bind(m_ui->deferEfbInvalidationCheckBox, Config::GFX_HACK_EFB_DEFER_INVALIDATION,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->manualTextureSamplingCheckBox, Config::GFX_HACK_FAST_TEXTURE_SAMPLING,
                     m_game_layer, true);
}

void AdvancedWidget::ConnectWidgets()
{
  ConfigWidget::ConnectCheckStateChanged(m_ui->loadCustomTexturesCheckBox, this, [this] {
    m_ui->prefetchCustomTexturesCheckBox->setEnabled(
        ConfigWidget::EffectiveChecked(m_ui->loadCustomTexturesCheckBox));
  });
  ConfigWidget::ConnectCheckStateChanged(m_ui->dumpTexturesCheckBox, this, [this] {
    const bool checked = ConfigWidget::EffectiveChecked(m_ui->dumpTexturesCheckBox);
    m_ui->dumpMipTexturesCheckBox->setEnabled(checked);
    m_ui->dumpBaseTexturesCheckBox->setEnabled(checked);
  });
  ConfigWidget::ConnectCheckStateChanged(m_ui->enableGraphicsModsCheckBox, this, [this] {
    emit Settings::Instance().EnableGfxModsChanged(
        ConfigWidget::EffectiveChecked(m_ui->enableGraphicsModsCheckBox));
  });
  ConfigWidget::ConnectCheckStateChanged(m_ui->customCropCheckBox, this, [this] {
    m_ui->customCropGroup->setDisabled(!ConfigWidget::EffectiveChecked(m_ui->customCropCheckBox));
  });
#if defined(HAVE_FFMPEG)
  ConfigWidget::ConnectCheckStateChanged(m_ui->losslessCodecCheckBox, this, [this] {
    m_ui->bitrateSpinBox->setEnabled(!ConfigWidget::EffectiveChecked(m_ui->losslessCodecCheckBox));
  });
#endif
}

void AdvancedWidget::OnBackendChanged()
{
  m_ui->backendMultithreadingCheckBox->setEnabled(g_backend_info.bSupportsMultithreading);
  m_ui->preferVsExpansionCheckBox->setEnabled(g_backend_info.bSupportsGeometryShaders &&
                                              g_backend_info.bSupportsVSLinePointExpand);
  AddDescriptions();
}

void AdvancedWidget::OnEmulationStateChanged(bool running)
{
  m_ui->progressiveScanCheckBox->setEnabled(!running);
}

void AdvancedWidget::AddDescriptions()
{
  static const char TR_WIREFRAME_DESCRIPTION[] =
      QT_TR_NOOP("Renders the scene as a wireframe.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_TEXTURE_FORMAT_DESCRIPTION[] =
      QT_TR_NOOP("Modifies textures to show the format they're encoded in.<br><br>May require "
                 "an emulation reset to apply.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_VALIDATION_LAYER_DESCRIPTION[] =
      QT_TR_NOOP("Enables validation of API calls made by the video backend, which may assist in "
                 "debugging graphical issues. On the Vulkan and D3D backends, this also enables "
                 "debug symbols for the compiled shaders.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_LOG_RENDERTIME_DESCRIPTION[] = QT_TR_NOOP(
      "Logs the render time of every frame to User/Logs/render_time.txt.<br><br>Use this "
      "feature to measure Dolphin's performance.<br><br><dolphin_emphasis>If "
      "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_TEXTURE_DESCRIPTION[] =
      QT_TR_NOOP("Dumps decoded game textures based on the other flags to "
                 "User/Dump/Textures/&lt;game_id&gt;/.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_MIP_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Whether to dump mipmapped game textures to "
      "User/Dump/Textures/&lt;game_id&gt;/.  This includes arbitrary mipmapped textures if "
      "'Arbitrary Mipmap Detection' is enabled in Enhancements.<br><br>"
      "<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_DUMP_BASE_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Whether to dump base game textures to "
      "User/Dump/Textures/&lt;game_id&gt;/.  This includes arbitrary base textures if 'Arbitrary "
      "Mipmap Detection' is enabled in Enhancements.<br><br><dolphin_emphasis>If unsure, leave "
      "this checked.</dolphin_emphasis>");
  static const char TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION[] =
      QT_TR_NOOP("Loads custom textures from User/Load/Textures/&lt;game_id&gt;/ and "
                 "User/Load/DynamicInputTextures/&lt;game_id&gt;/.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Caches custom textures to system RAM on startup.<br><br>This can require exponentially "
      "more RAM but fixes possible stuttering.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_DUMP_EFB_DESCRIPTION[] =
      QT_TR_NOOP("Dumps the contents of EFB copies to User/Dump/Textures/.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DUMP_XFB_DESCRIPTION[] =
      QT_TR_NOOP("Dumps the contents of XFB copies to User/Dump/Textures/.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DISABLE_VRAM_COPIES_DESCRIPTION[] =
      QT_TR_NOOP("Disables the VRAM copy of the EFB, forcing a round-trip to RAM. Inhibits all "
                 "upscaling.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_LOAD_GRAPHICS_MODS_DESCRIPTION[] =
      QT_TR_NOOP("Loads graphics mods from User/Load/GraphicsMods/.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_FRAME_DUMPS_RESOLUTION_TYPE_DESCRIPTION[] = QT_TR_NOOP(
      "Selects how frame dumps (videos) and screenshots are going to be captured.<br>If the game "
      "or window resolution change during a recording, multiple video files might be created.<br>"
      "Note that color correction and cropping are always ignored by the captures."
      "<br><br><b>Window Resolution</b>: Uses the output window resolution (without black bars)."
      "<br>This is a simple dumping option that will capture the image more or less as you see it."
      "<br><b>Aspect Ratio Corrected Internal Resolution</b>: "
      "Uses the Internal Resolution (XFB size), and corrects it by the target aspect ratio.<br>"
      "This option will consistently dump at the specified Internal Resolution "
      "regardless of how the image is displayed during recording."
      "<br><b>Raw Internal Resolution</b>: Uses the Internal Resolution (XFB size) "
      "without correcting it with the target aspect ratio.<br>"
      "This will provide a clean dump without any aspect ratio correction so users have as raw as "
      "possible input for external editing software.<br><br><dolphin_emphasis>If unsure, leave "
      "this at \"Aspect Ratio Corrected Internal Resolution\".</dolphin_emphasis>");
#if defined(HAVE_FFMPEG)
  static const char TR_USE_LOSSLESS_DESCRIPTION[] =
      QT_TR_NOOP("Encodes frame dumps using the Ut Video codec. If this option is unchecked, a "
                 "lossy Xvid codec will be used.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
#endif
  static const char TR_PNG_COMPRESSION_LEVEL_DESCRIPTION[] =
      QT_TR_NOOP("Specifies the zlib compression level to use when saving PNG images (both for "
                 "screenshots and framedumping).<br><br>"
                 "Since PNG uses lossless compression, this does not affect the image quality; "
                 "instead, it is a trade-off between file size and compression time.<br><br>"
                 "A value of 0 uses no compression at all.  A value of 1 uses very little "
                 "compression, while the maximum value of 9 applies a lot of compression.  "
                 "However, for PNG files, levels between 3 and 6 are generally about as good as "
                 "level 9 but finish in significantly less time.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this at 6.</dolphin_emphasis>");
  static const char TR_PROGRESSIVE_SCAN_DESCRIPTION[] = QT_TR_NOOP(
      "Enables progressive scan if supported by the emulated software. Most games don't have "
      "any issue with this.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_BACKEND_MULTITHREADING_DESCRIPTION[] =
      QT_TR_NOOP("Enables multithreaded command submission in backends where supported. Enabling "
                 "this option may result in a performance improvement on systems with more than "
                 "two CPU cores. Currently, this is limited to the Vulkan backend.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_PREFER_VS_FOR_POINT_LINE_EXPANSION_DESCRIPTION[] =
      QT_TR_NOOP("On backends that support both using the geometry shader and the vertex shader "
                 "for expanding points and lines, selects the vertex shader for the job.  May "
                 "affect performance."
                 "<br><br>%1");
  static const char TR_CPU_CULL_DESCRIPTION[] =
      QT_TR_NOOP("Cull vertices on the CPU to reduce the number of draw calls required.  "
                 "May affect performance and draw statistics.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION[] = QT_TR_NOOP(
      "Defers invalidation of the EFB access cache until a GPU synchronization command "
      "is executed. If disabled, the cache will be invalidated with every draw call. "
      "<br><br>May improve performance in some games which rely on CPU EFB Access at the cost "
      "of stability.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_MANUAL_TEXTURE_SAMPLING_DESCRIPTION[] = QT_TR_NOOP(
      "Use a manual implementation of texture sampling instead of the graphics backend's built-in "
      "functionality.<br><br>"
      "This setting can fix graphical issues in some games on certain GPUs, most commonly vertical "
      "lines on FMVs. In addition to this, enabling Manual Texture Sampling will allow for correct "
      "emulation of texture wrapping special cases (at 1x IR or when scaled EFB is disabled, and "
      "with custom textures disabled) and better emulates Level of Detail calculation.<br><br>"
      "This comes at the cost of potentially worse performance, especially at higher internal "
      "resolutions.<br><br>If this setting is enabled, the Texture Filtering setting will be "
      "disabled."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

#ifdef _WIN32
  static const char TR_BORDERLESS_FULLSCREEN_DESCRIPTION[] = QT_TR_NOOP(
      "Implements fullscreen mode with a borderless window spanning the whole screen instead of "
      "using exclusive mode. Allows for faster transitions between fullscreen and windowed mode, "
      "but slightly increases input latency, makes movement less smooth and slightly decreases "
      "performance.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
#endif

  // Crop.
  static const char TR_CROP_TO_ASPECT_RATIO_DESCRIPTION[] = QT_TR_NOOP(
      "Crops the picture from its native aspect ratio (which rarely exactly matches 4:3 or 16:9, "
      "because it often includes overscan) to the specific user target aspect ratio (e.g. 4:3 or "
      "16:9)."
      "<br><br>This option was previously known as Graphics/Advanced/Misc/Crop."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_CROP_CUSTOM_DESCRIPTION[] = QT_TR_NOOP(
      "Enables options to crop the picture by discrete native pixels (independent of the internal "
      "resolution setting) in order to attain a specific "
      "user target aspect ratio. Useful to resolve letterboxing issues."
      " <br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_CROP_CUSTOM_LEFT[] = QT_TR_NOOP(
      "Crops the picture left side by discrete native pixels (independent of the internal "
      "resolution setting) in order to attain a specific user target aspect ratio. "
      " <br><br><dolphin_emphasis>If unsure, leave this value to 0.</dolphin_emphasis>");
  static const char TR_CROP_CUSTOM_TOP[] = QT_TR_NOOP(
      "Crops the picture top side by discrete native pixels (independent of the internal "
      "resolution setting) in order to attain a specific user target aspect ratio. "
      " <br><br><dolphin_emphasis>If unsure, leave this value to 0.</dolphin_emphasis>");
  static const char TR_CROP_CUSTOM_RIGHT[] = QT_TR_NOOP(
      "Crops the picture right side by discrete native pixels (independent of the internal "
      "resolution setting) in order to attain a specific user target aspect ratio. "
      " <br><br><dolphin_emphasis>If unsure, leave this value to 0.</dolphin_emphasis>");
  static const char TR_CROP_CUSTOM_BOTTOM[] = QT_TR_NOOP(
      "Crops the picture bottom side by discrete native pixels (independent of the internal "
      "resolution setting) in order to attain a specific user target aspect ratio. "
      " <br><br><dolphin_emphasis>If unsure, leave this value to 0.</dolphin_emphasis>");

  static const char IF_UNSURE_UNCHECKED[] =
      QT_TR_NOOP("<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->wireframeCheckBox, {}, tr(TR_WIREFRAME_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->textureFormatOverlayCheckBox, {},
                               tr(TR_TEXTURE_FORMAT_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->apiValidationCheckBox, {},
                               tr(TR_VALIDATION_LAYER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->logRenderTimeCheckBox, {}, tr(TR_LOG_RENDERTIME_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->dumpTexturesCheckBox, {}, tr(TR_DUMP_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->dumpMipTexturesCheckBox, {},
                               tr(TR_DUMP_MIP_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->dumpBaseTexturesCheckBox, {},
                               tr(TR_DUMP_BASE_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->loadCustomTexturesCheckBox, {},
                               tr(TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->prefetchCustomTexturesCheckBox, {},
                               tr(TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->dumpEfbTargetCheckBox, {}, tr(TR_DUMP_EFB_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->dumpXfbTargetCheckBox, {}, tr(TR_DUMP_XFB_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->disableVramCopiesCheckBox, {},
                               tr(TR_DISABLE_VRAM_COPIES_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->enableGraphicsModsCheckBox, {},
                               tr(TR_LOAD_GRAPHICS_MODS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->resolutionTypeComboBox, {},
                               tr(TR_FRAME_DUMPS_RESOLUTION_TYPE_DESCRIPTION));
#ifdef HAVE_FFMPEG
  ConfigWidget::SetDescription(m_ui->losslessCodecCheckBox, {}, tr(TR_USE_LOSSLESS_DESCRIPTION));
#endif
  ConfigWidget::SetDescription(m_ui->pngCompressionSpinBox, tr("PNG Compression Level"),
                               tr(TR_PNG_COMPRESSION_LEVEL_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->progressiveScanCheckBox, {},
                               tr(TR_PROGRESSIVE_SCAN_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->backendMultithreadingCheckBox, {},
                               tr(TR_BACKEND_MULTITHREADING_DESCRIPTION));
  QString vsexpand_extra;
  if (!g_backend_info.bSupportsGeometryShaders)
    vsexpand_extra = tr("Forced on because %1 doesn't support geometry shaders.")
                         .arg(tr(g_backend_info.DisplayName.c_str()));
  else if (!g_backend_info.bSupportsVSLinePointExpand)
    vsexpand_extra = tr("Forced off because %1 doesn't support VS expansion.")
                         .arg(tr(g_backend_info.DisplayName.c_str()));
  else
    vsexpand_extra = tr(IF_UNSURE_UNCHECKED);
  ConfigWidget::SetDescription(
      m_ui->preferVsExpansionCheckBox, {},
      tr(TR_PREFER_VS_FOR_POINT_LINE_EXPANSION_DESCRIPTION).arg(vsexpand_extra));
  ConfigWidget::SetDescription(m_ui->cpuCullCheckBox, {}, tr(TR_CPU_CULL_DESCRIPTION));
#ifdef _WIN32
  ConfigWidget::SetDescription(m_ui->borderlessFullscreenCheckBox, {},
                               tr(TR_BORDERLESS_FULLSCREEN_DESCRIPTION));
#endif
  ConfigWidget::SetDescription(m_ui->cropToAspectRatioCheckBox, {},
                               tr(TR_CROP_TO_ASPECT_RATIO_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->customCropCheckBox, {}, tr(TR_CROP_CUSTOM_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->cropLeftSpinBox, {}, tr(TR_CROP_CUSTOM_LEFT));
  ConfigWidget::SetDescription(m_ui->cropTopSpinBox, {}, tr(TR_CROP_CUSTOM_TOP));
  ConfigWidget::SetDescription(m_ui->cropRightSpinBox, {}, tr(TR_CROP_CUSTOM_RIGHT));
  ConfigWidget::SetDescription(m_ui->cropBottomSpinBox, {}, tr(TR_CROP_CUSTOM_BOTTOM));
  ConfigWidget::SetDescription(m_ui->deferEfbInvalidationCheckBox, {},
                               tr(TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->manualTextureSamplingCheckBox, {},
                               tr(TR_MANUAL_TEXTURE_SAMPLING_DESCRIPTION));
}
