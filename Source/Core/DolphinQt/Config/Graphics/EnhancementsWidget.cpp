// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"

#include <array>
#include <atomic>
#include <future>
#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/Config/Graphics/ShaderParametersDialog.h"
#include "DolphinQt/Config/Graphics/ShaderPresetPickerDialog.h"

#include "VideoCommon/PostProcessing/LibrashaderLoader.h"
#include "VideoCommon/PostProcessing/PostProcessingConfig.h"
#include "VideoCommon/PostProcessing/RetroCrisisInstall.h"
#include "VideoCommon/PostProcessing/ShaderPackDownload.h"
#include "VideoCommon/PostProcessing/ShaderPackSource.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

#include "ui_EnhancementsWidget.h"

EnhancementsWidget::EnhancementsWidget(GraphicsPane* gfx_pane)
    : m_ui{std::make_unique<Ui::EnhancementsWidget>()}, m_game_layer{gfx_pane->GetConfigLayer()}
{
  MigrateRemovedStereoModes();
  m_ui->setupUi(this);
  BindSettings();
  ShaderChanged();
  ConnectWidgets();
  AddDescriptions();

  // The preset field is filled by its own constructor, i.e. before ConnectWidgets could hear about
  // it, so the initial value needs one explicit pass to be resolved for display.
  ShowResolvedPreset();

  // BackendChanged is called by parent on window creation.
  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &EnhancementsWidget::OnBackendChanged);
  connect(gfx_pane, &GraphicsPane::UseFastTextureSamplingChanged, this, [this] {
    m_ui->textureFilteringComboBox->setEnabled(
        ConfigWidget::Logic::ReadValue(Config::GFX_HACK_FAST_TEXTURE_SAMPLING, m_game_layer));
  });
  ConfigWidget::ConnectCheckStateChanged(m_ui->arbitraryMipmapDetectionCheckBox, gfx_pane,
                                         [gfx_pane] { emit gfx_pane->UpdateGPUTextureDecoding(); });
}

EnhancementsWidget::~EnhancementsWidget() = default;

constexpr int ANISO_1x = std::to_underlying(AnisotropicFilteringMode::Force1x);
constexpr int ANISO_2X = std::to_underlying(AnisotropicFilteringMode::Force2x);
constexpr int ANISO_4X = std::to_underlying(AnisotropicFilteringMode::Force4x);
constexpr int ANISO_8X = std::to_underlying(AnisotropicFilteringMode::Force8x);
constexpr int ANISO_16X = std::to_underlying(AnisotropicFilteringMode::Force16x);
constexpr int FILTERING_DEFAULT = std::to_underlying(TextureFilteringMode::Default);
constexpr int FILTERING_NEAREST = std::to_underlying(TextureFilteringMode::Nearest);
constexpr int FILTERING_LINEAR = std::to_underlying(TextureFilteringMode::Linear);

void EnhancementsWidget::MigrateRemovedStereoModes()
{
  // Anaglyph and Passive are no longer offered (see the stereo combo below), but their enumerators
  // still parse out of an existing GFX.ini. The mapped combo has no entry for them, so it would
  // setCurrentIndex(-1) and leave the combo blank -- the user could not tell which mode they were
  // in, and VideoConfig::VerifyValidity() is meanwhile rendering them as Off. Rewrite the stored
  // value once so the control always shows a real mode.
  //
  // Normalize whichever source the combo displays. A shipped game setting is changed only in this
  // window's in-memory layer; its source INI remains untouched.
  const bool has_local_value =
      m_game_layer != nullptr && m_game_layer->Exists(Config::GFX_STEREO_MODE.GetLocation());
  Config::Layer* const shipped_game_layer = ConfigWidget::Logic::GetFallbackLayer(m_game_layer);
  const bool has_shipped_value = shipped_game_layer != nullptr &&
                                 shipped_game_layer->Exists(Config::GFX_STEREO_MODE.GetLocation());
  const StereoMode mode = ConfigWidget::Logic::ReadValue(Config::GFX_STEREO_MODE, m_game_layer);
  if (mode != StereoMode::Anaglyph && mode != StereoMode::Passive)
    return;

  if (has_local_value)
  {
    m_game_layer->Set(Config::GFX_STEREO_MODE, StereoMode::Off);
    Config::OnConfigChanged();
  }
  else if (has_shipped_value)
  {
    shipped_game_layer->Set(Config::GFX_STEREO_MODE, StereoMode::Off);
    Config::OnConfigChanged();
  }
  else
  {
    Config::SetBaseOrCurrent(Config::GFX_STEREO_MODE, StereoMode::Off);
  }
}

void EnhancementsWidget::BindSettings()
{
  QStringList resolution_options{tr("Auto (Multiple of 640x528)"), tr("Native (640x528)")};
  // From 2x up.
  // To calculate the suggested internal resolution scale for each common output resolution,
  // we find the minimum multiplier that results in an equal or greater resolution than the
  // output one, on both width and height.
  // Note that often games don't render to the full resolution, but have some black bars
  // on the edges; this is not accounted for in the calculations.
  const QStringList resolution_extra_options{
      tr("720p"),         tr("1080p"),        tr("1440p"), QStringLiteral(""),
      tr("4K"),           QStringLiteral(""), tr("5K"),    QStringLiteral(""),
      QStringLiteral(""), QStringLiteral(""), tr("8K")};
  const int visible_resolution_option_count = static_cast<int>(resolution_options.size()) +
                                              static_cast<int>(resolution_extra_options.size());

  // If the current scale is greater than the max scale in the ini, add sufficient options so that
  // when the settings are saved we don't lose the user-modified value from the ini.
  const int max_efb_scale =
      std::max(ConfigWidget::Logic::ReadValue(Config::GFX_EFB_SCALE, m_game_layer),
               ConfigWidget::Logic::ReadValue(Config::GFX_MAX_EFB_SCALE, m_game_layer));
  for (int scale = static_cast<int>(resolution_options.size()); scale <= max_efb_scale; scale++)
  {
    const QString scale_text = QString::number(scale);
    const QString width_text = QString::number(static_cast<int>(EFB_WIDTH) * scale);
    const QString height_text = QString::number(static_cast<int>(EFB_HEIGHT) * scale);
    const int extra_index = resolution_options.size() - 2;
    const QString extra_text = resolution_extra_options.size() > extra_index ?
                                   resolution_extra_options[extra_index] :
                                   QStringLiteral("");
    if (extra_text.isEmpty())
    {
      resolution_options.append(tr("%1x Native (%2x%3)").arg(scale_text, width_text, height_text));
    }
    else
    {
      resolution_options.append(
          tr("%1x Native (%2x%3) for %4").arg(scale_text, width_text, height_text, extra_text));
    }
  }

  m_ui->internalResolutionComboBox->addItems(resolution_options);
  m_ui->internalResolutionComboBox->setMaxVisibleItems(visible_resolution_option_count);
  ConfigWidget::Bind(m_ui->internalResolutionComboBox, Config::GFX_EFB_SCALE, m_game_layer);

  m_antialiasing_binding = ConfigWidget::BindComplex(m_ui->antiAliasingComboBox, Config::GFX_MSAA,
                                                     Config::GFX_SSAA, m_game_layer);
  m_antialiasing_binding->Add(tr("None"), static_cast<u32>(1), false);

  m_texture_filtering_binding =
      ConfigWidget::BindComplex(m_ui->textureFilteringComboBox, Config::GFX_ENHANCE_MAX_ANISOTROPY,
                                Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING, m_game_layer);
  m_texture_filtering_binding->Add(tr("Default"), Config::DefaultState{}, FILTERING_DEFAULT);
  m_texture_filtering_binding->Add(tr("1x Anisotropic"), ANISO_1x, FILTERING_DEFAULT);
  m_texture_filtering_binding->Add(tr("2x Anisotropic"), ANISO_2X, FILTERING_DEFAULT);
  m_texture_filtering_binding->Add(tr("4x Anisotropic"), ANISO_4X, FILTERING_DEFAULT);
  m_texture_filtering_binding->Add(tr("8x Anisotropic"), ANISO_8X, FILTERING_DEFAULT);
  m_texture_filtering_binding->Add(tr("16x Anisotropic"), ANISO_16X, FILTERING_DEFAULT);
  m_texture_filtering_binding->Add(tr("Force Nearest and 1x Anisotropic "), ANISO_1x,
                                   FILTERING_NEAREST);
  m_texture_filtering_binding->Add(tr("Force Linear and 1x Anisotropic"), ANISO_1x,
                                   FILTERING_LINEAR);
  m_texture_filtering_binding->Add(tr("Force Linear and 2x Anisotropic"), ANISO_2X,
                                   FILTERING_LINEAR);
  m_texture_filtering_binding->Add(tr("Force Linear and 4x Anisotropic"), ANISO_4X,
                                   FILTERING_LINEAR);
  m_texture_filtering_binding->Add(tr("Force Linear and 8x Anisotropic"), ANISO_8X,
                                   FILTERING_LINEAR);
  m_texture_filtering_binding->Add(tr("Force Linear and 16x Anisotropic"), ANISO_16X,
                                   FILTERING_LINEAR);
  m_ui->textureFilteringComboBox->setEnabled(
      ConfigWidget::Logic::ReadValue(Config::GFX_HACK_FAST_TEXTURE_SAMPLING, m_game_layer));

  ConfigWidget::Bind(m_ui->postProcessingPresetLineEdit, Config::GFX_ENHANCE_POST_SHADER,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->scaledEfbCopyCheckBox, Config::GFX_HACK_COPY_EFB_SCALED, m_game_layer);
  ConfigWidget::Bind(m_ui->perPixelLightingCheckBox, Config::GFX_ENABLE_PIXEL_LIGHTING,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->widescreenHackCheckBox, Config::GFX_WIDESCREEN_HACK, m_game_layer);
  ConfigWidget::Bind(m_ui->disableFogCheckBox, Config::GFX_DISABLE_FOG, m_game_layer);
  ConfigWidget::Bind(m_ui->force24BitColorCheckBox, Config::GFX_ENHANCE_FORCE_TRUE_COLOR,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->disableCopyFilterCheckBox, Config::GFX_ENHANCE_DISABLE_COPY_FILTER,
                     m_game_layer);
  ConfigWidget::Bind(m_ui->arbitraryMipmapDetectionCheckBox,
                     Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, m_game_layer);
  ConfigWidget::Bind(m_ui->hdrCheckBox, Config::GFX_ENHANCE_HDR_OUTPUT, m_game_layer);

  // Anaglyph and Passive were implemented by the old post-processing shader, which no longer
  // exists; selecting them renders a second layer for no visible effect. The mapped binding stores
  // explicit values, so the remaining entries keep their StereoMode meanings. Stored Anaglyph /
  // Passive values are rewritten to Off by MigrateRemovedStereoModes() before we get here.
  constexpr std::array stereo_modes{StereoMode::Off, StereoMode::SideBySide,
                                    StereoMode::TopAndBottom, StereoMode::QuadBuffer};
  ConfigWidget::BindMapped(m_ui->stereoModeComboBox, Config::GFX_STEREO_MODE,
                           std::span<const StereoMode>{stereo_modes}, m_game_layer);
  const ConfigWidget::FloatSliderHandle depth =
      ConfigWidget::BindFloat(m_ui->stereoDepthSlider, Config::GFX_STEREO_DEPTH, 0,
                              Config::GFX_STEREO_DEPTH_MAXIMUM, 1.0f, m_game_layer);
  const ConfigWidget::FloatSliderHandle convergence =
      ConfigWidget::BindFloat(m_ui->stereoConvergenceSlider, Config::GFX_STEREO_CONVERGENCE, 0,
                              Config::GFX_STEREO_CONVERGENCE_MAXIMUM, 0.01f, m_game_layer);
  ConfigWidget::MirrorFloatValue(m_ui->stereoDepthValueLabel, depth, QStringLiteral("%.0f"));
  ConfigWidget::MirrorFloatValue(m_ui->stereoConvergenceValueLabel, convergence,
                                 QStringLiteral("%.2f"));
  ConfigWidget::MirrorFont(m_ui->stereoDepthLabel, m_ui->stereoDepthSlider);
  ConfigWidget::MirrorFont(m_ui->stereoConvergenceLabel, m_ui->stereoConvergenceSlider);
  ConfigWidget::Bind(m_ui->swapEyesCheckBox, Config::GFX_STEREO_SWAP_EYES, m_game_layer);
  ConfigWidget::Bind(m_ui->fullResolutionPerEyeCheckBox, Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL,
                     m_game_layer);

  const auto current_stereo_mode =
      ConfigWidget::Logic::ReadValue(Config::GFX_STEREO_MODE, m_game_layer);
  if (current_stereo_mode != StereoMode::SideBySide &&
      current_stereo_mode != StereoMode::TopAndBottom)
  {
    m_ui->fullResolutionPerEyeCheckBox->hide();
  }
}

void EnhancementsWidget::ConnectWidgets()
{
  connect(m_ui->stereoModeComboBox, &QComboBox::currentIndexChanged, this, [this] {
    const auto current_stereo_mode =
        ConfigWidget::Logic::ReadValue(Config::GFX_STEREO_MODE, m_game_layer);

    if (current_stereo_mode == StereoMode::SideBySide ||
        current_stereo_mode == StereoMode::TopAndBottom)
    {
      m_ui->fullResolutionPerEyeCheckBox->show();
    }
    else
    {
      m_ui->fullResolutionPerEyeCheckBox->hide();
    }
  });

  connect(m_ui->postProcessingBrowseButton, &QPushButton::clicked, this,
          &EnhancementsWidget::BrowseForShaderPreset);
  connect(m_ui->postProcessingClearButton, &QPushButton::clicked, this,
          &EnhancementsWidget::ClearShaderPreset);
  connect(m_ui->postProcessingParametersButton, &QPushButton::clicked, this,
          &EnhancementsWidget::EditShaderParameters);

  // Parameters… needs a preset, and the field is what every path that changes one goes through:
  // the picker, Clear, and the right-click that drops a game override (the binder refresh calls
  // setText, which emits this too). Reading the field is what makes this correct rather than one
  // edit behind -- see CurrentShaderPreset().
  connect(m_ui->postProcessingPresetLineEdit, &QLineEdit::textChanged, this, [this] {
    ShowResolvedPreset();
    UpdateParametersButtonState();
  });

  // Convert download button to menu
  auto* const menu = new QMenu(this);
  for (const VideoCommon::ShaderPackSource& source : VideoCommon::GetShaderPackSources())
  {
    const std::string id = source.id;
    QAction* const action = menu->addAction(QString::fromStdString(source.display_name));
    if (id == "retrocrisis")
    {
      connect(action, &QAction::triggered, this, [this, id] {
        QStringList profiles;
        for (const std::string& profile : VideoCommon::GetRetroCrisisProfiles())
          profiles << QString::fromStdString(profile);
        bool ok = false;
        const QString profile = QInputDialog::getItem(this, tr("Choose a display profile"),
                                                      tr("Profile:"), profiles, 0, false, &ok);
        if (ok)
          DownloadShaderPack(id, profile.toStdString());
      });
    }
    else
    {
      connect(action, &QAction::triggered, this, [this, id] { DownloadShaderPack(id, ""); });
    }
  }
  m_ui->downloadShaderPackButton->setMenu(menu);
}

std::string EnhancementsWidget::CurrentShaderPreset() const
{
  // The field rather than the config, for two reasons. SetShaderPreset emits textChanged from
  // setText() and writes the config only afterwards, so anything reached from that signal would
  // read the value from before the edit. And the field is what resolves the game layer the way this
  // page displays it -- the game's value if the game overrides the preset, the global one if it
  // does not -- which Config::Get on a layer does not do: it falls back to the setting's default,
  // so in Game Properties it reports "no preset" for every game without an override.
  //
  // ResolveConfiguredPreset, not the raw text: a GFX.ini written before chains were removed can
  // hold a ';'-separated list, and both the picker and the parameters dialog have to act on the
  // preset that is actually in use rather than on a string neither can match. Only ever one preset
  // is written back.
  return VideoCommon::ResolveConfiguredPreset(
      m_ui->postProcessingPresetLineEdit->text().toStdString());
}

void EnhancementsWidget::ShowResolvedPreset()
{
  // The bound line edit displays the stored string verbatim -- and a GFX.ini written before chains
  // were removed stores a ';'-separated list. Every path that acts on the value uses only the first
  // entry (CurrentShaderPreset, the picker, the parameters dialog, the post-processor), so left
  // alone the row would name presets that nothing loads. Show the one in use instead.
  //
  // Display only, no write-back: replacing the stored value here would silently rewrite a game or
  // global INI just because someone opened the graphics page. The row now matches what runs; the
  // stored chain is rewritten only when the user picks or clears a preset. The binder treats a
  // read-only line edit as display-only, so this setText cannot become a write when the window
  // closes.
  //
  // The guard also terminates the recursion through textChanged -> here, since the resolved value
  // resolves to itself, and it means ResolveConfiguredPreset stops warning about the dropped tail
  // after the first pass instead of on every keystroke-sized signal.
  const QString resolved = QString::fromStdString(VideoCommon::ResolveConfiguredPreset(
      m_ui->postProcessingPresetLineEdit->text().toStdString()));
  if (resolved == m_ui->postProcessingPresetLineEdit->text())
    return;

  m_ui->postProcessingPresetLineEdit->setText(resolved);
}

void EnhancementsWidget::SetShaderPreset(const QString& preset)
{
  if (preset == m_ui->postProcessingPresetLineEdit->text())
    return;

  m_ui->postProcessingPresetLineEdit->setText(preset);
  const std::string value = preset.toStdString();
  if (m_game_layer != nullptr)
  {
    m_game_layer->Set(Config::GFX_ENHANCE_POST_SHADER, value);
    Config::OnConfigChanged();
  }
  else
  {
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, value);
  }
}

void EnhancementsWidget::BrowseForShaderPreset()
{
  ShaderPresetPickerDialog dialog(this, QString::fromStdString(CurrentShaderPreset()));
  if (dialog.exec() != QDialog::Accepted || dialog.SelectedPreset().isEmpty())
    return;

  SetShaderPreset(dialog.SelectedPreset());
}

void EnhancementsWidget::EditShaderParameters()
{
  // The layer is passed as a bare "are we in Game Properties" flag, not as a layer to write: the
  // dialog stores its values globally either way, and that is what the flag makes it say. Giving it
  // a per-game parameter layer would need a per-game section, a merge rule for it and a way to see
  // and clear the override, none of which exist.
  ShaderParametersDialog dialog(this, QString::fromStdString(CurrentShaderPreset()),
                                m_game_layer != nullptr);
  dialog.exec();
}

void EnhancementsWidget::UpdateParametersButtonState()
{
  // Whether the preset *has* parameters is deliberately not tested here: finding out costs parsing
  // the preset every time this row changes, and the dialog says so itself when there are none.
  //
  // Whether librashader loaded *is* tested, because that one is not a property of the preset. The
  // dialog gets its list from LibrashaderParameters::Enumerate, which is guarded on
  // GetAvailability() and returns the availability reason as its error, so without the library
  // every preset reports "Could not load preset" no matter how many parameters it declares. A
  // button that can only ever open an error is better disabled, with the reason in its tooltip
  // (see AddDescriptions) -- the built-in engine still post-processes, it just cannot be tuned
  // from here.
  m_ui->postProcessingParametersButton->setEnabled(
      g_backend_info.bSupportsPostProcessing &&
      VideoCommon::Librashader::GetAvailability().available && !CurrentShaderPreset().empty());
}

void EnhancementsWidget::ClearShaderPreset()
{
  // The picker cannot select "nothing", so this is what turns post-processing back off -- the job
  // the combo box's "(off)" entry used to do.
  SetShaderPreset(QString{});
  ShaderChanged();
}

void EnhancementsWidget::OnBackendChanged()
{
  m_ui->hdrCheckBox->setEnabled(g_backend_info.bSupportsHDROutput);

  // Stereoscopy
  const bool supports_stereoscopy = g_backend_info.bSupportsGeometryShaders;
  m_ui->stereoModeComboBox->setEnabled(supports_stereoscopy);
  m_ui->stereoConvergenceSlider->setEnabled(supports_stereoscopy);
  m_ui->stereoDepthSlider->setEnabled(supports_stereoscopy);
  m_ui->swapEyesCheckBox->setEnabled(supports_stereoscopy);

  // PostProcessing
  const bool supports_postprocessing = g_backend_info.bSupportsPostProcessing;
  const QString unsupported_tooltip =
      supports_postprocessing ?
          QString{} :
          tr("%1 doesn't support this feature.").arg(tr(g_video_backend->GetDisplayName().c_str()));
  for (QWidget* const widget : {static_cast<QWidget*>(m_ui->postProcessingPresetLineEdit),
                                static_cast<QWidget*>(m_ui->postProcessingBrowseButton),
                                static_cast<QWidget*>(m_ui->postProcessingClearButton),
                                static_cast<QWidget*>(m_ui->postProcessingParametersButton)})
  {
    widget->setEnabled(supports_postprocessing);
    widget->setToolTip(unsupported_tooltip);
  }
  // Parameters… needs a preset as well as a backend that can run one, so it narrows what the loop
  // above just set. This is also where its initial state comes from: GraphicsPane emits
  // BackendChanged once on window creation.
  UpdateParametersButtonState();

  UpdateAntialiasingOptions();
}

void EnhancementsWidget::ShaderChanged()
{
  auto shader = ConfigWidget::Logic::ReadValue(Config::GFX_ENHANCE_POST_SHADER, m_game_layer);

  if (shader == "(off)" || shader == "")
  {
    shader = "";

    // Setting a shader to null in a game ini could be confusing, as it won't be bolded. Remove it
    // instead.
    if (m_game_layer != nullptr)
      m_game_layer->DeleteKey(Config::GFX_ENHANCE_POST_SHADER.GetLocation());
    else
      Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, shader);
  }
}

void EnhancementsWidget::UpdateAntialiasingOptions()
{
  const QSignalBlocker blocker(m_ui->antiAliasingComboBox);

  m_antialiasing_binding->Reset();
  m_antialiasing_binding->Add(tr("None"), static_cast<u32>(1), false);

  const std::vector<u32>& aa_modes = g_backend_info.AAModes;
  for (const u32 aa_mode : aa_modes)
  {
    if (aa_mode > 1)
      m_antialiasing_binding->Add(tr("%1x MSAA").arg(aa_mode), aa_mode, false);
  }

  if (g_backend_info.bSupportsSSAA)
  {
    for (const u32 aa_mode : aa_modes)
    {
      if (aa_mode > 1)
        m_antialiasing_binding->Add(tr("%1x SSAA").arg(aa_mode), aa_mode, true);
    }
  }

  // Backend info can't be populated in the local game settings window. Only enable local game AA
  // edits when the backend info is correct - global and local have the same backend.
  const bool good_info = m_game_layer == nullptr ||
                         Config::Get(Config::MAIN_GFX_BACKEND) ==
                             ConfigWidget::Logic::ReadValue(Config::MAIN_GFX_BACKEND, m_game_layer);
  const int fixed_option_count = m_game_layer == nullptr ? 1 : 2;

  m_ui->antiAliasingComboBox->setEnabled(m_ui->antiAliasingComboBox->count() > fixed_option_count &&
                                         good_info);
}

void EnhancementsWidget::AddDescriptions()
{
  static const char TR_INTERNAL_RESOLUTION_DESCRIPTION[] =
      QT_TR_NOOP("Controls the rendering resolution.<br><br>A high resolution greatly improves "
                 "visual quality, but also greatly increases GPU load and can cause issues in "
                 "certain games. Generally speaking, the lower the internal resolution, the "
                 "better performance will be.<br><br><dolphin_emphasis>If unsure, "
                 "select Native.</dolphin_emphasis>");
  static const char TR_ANTIALIAS_DESCRIPTION[] = QT_TR_NOOP(
      "Reduces the amount of aliasing caused by rasterizing 3D graphics, resulting "
      "in smoother edges on objects. Increases GPU load and sometimes causes graphical "
      "issues.<br><br>SSAA is significantly more demanding than MSAA, but provides top quality "
      "geometry anti-aliasing and also applies anti-aliasing to lighting, shader "
      "effects, and textures.<br><br><dolphin_emphasis>If unsure, select "
      "None.</dolphin_emphasis>");
  static const char TR_FORCE_TEXTURE_FILTERING_DESCRIPTION[] = QT_TR_NOOP(
      "Adjust the texture filtering. Anisotropic filtering enhances the visual quality of textures "
      "that are at oblique viewing angles. Force Nearest and Force Linear override the texture "
      "scaling filter selected by the game.<br><br>Any option except 'Default' will alter the look "
      "of the game's textures and might cause issues in a small number of games.<br><br>This "
      "setting is disabled when Manual Texture Sampling is enabled.<br><br>"
      "<dolphin_emphasis>If unsure, select 'Default'.</dolphin_emphasis>");
  static const char TR_POSTPROCESSING_DESCRIPTION[] = QT_TR_NOOP(
      "Applies a post-processing effect after rendering a frame.<br><br />Opens a searchable tree "
      "of the presets found in the Shaders folders. Use Clear to apply no effect at all.<br><br "
      "/><dolphin_emphasis>If unsure, leave this empty.</dolphin_emphasis>");
  static const char TR_POSTPROCESSING_PARAMETERS_DESCRIPTION[] = QT_TR_NOOP(
      "Adjusts the parameters the selected preset declares, such as scanline strength or screen "
      "curvature.<br><br />Edits take effect immediately, including while a game is running. Only "
      "the values you change are saved, so a parameter you reset follows the preset's own default "
      "again.");
  static const char TR_SCALED_EFB_COPY_DESCRIPTION[] =
      QT_TR_NOOP("Greatly increases the quality of textures generated using render-to-texture "
                 "effects.<br><br>Slightly increases GPU load and causes relatively few graphical "
                 "issues. Raising the internal resolution will improve the effect of this setting. "
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_PER_PIXEL_LIGHTING_DESCRIPTION[] = QT_TR_NOOP(
      "Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
      "appearance of lit polygons and making individual triangles less noticeable.<br><br "
      "/>Rarely "
      "causes slowdowns or graphical issues.<br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
  static const char TR_WIDESCREEN_HACK_DESCRIPTION[] = QT_TR_NOOP(
      "Forces the game to output graphics at any aspect ratio by expanding the view frustum "
      "without stretching the image.<br>This is a hack, and its results will vary widely game "
      "to game (it often causes the UI to stretch).<br>"
      "Game-specific AR/Gecko-code aspect ratio patches are preferable over this if available."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_REMOVE_FOG_DESCRIPTION[] =
      QT_TR_NOOP("Makes distant objects more visible by removing fog, thus increasing the overall "
                 "detail.<br><br>Disabling fog will break some games which rely on proper fog "
                 "emulation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_3D_MODE_DESCRIPTION[] = QT_TR_NOOP(
      "Selects the stereoscopic 3D mode. Stereoscopy allows a better feeling "
      "of depth if the necessary hardware is present. Heavily decreases "
      "emulation speed and sometimes causes issues.<br><br>Side-by-Side and Top-and-Bottom are "
      "used by most 3D TVs.<br>HDMI 3D is "
      "used when the monitor supports 3D display resolutions.<br><br><dolphin_emphasis>If unsure, "
      "select Off.</dolphin_emphasis>");
  static const char TR_3D_DEPTH_DESCRIPTION[] = QT_TR_NOOP(
      "Controls the separation distance between the virtual cameras.<br><br>A higher "
      "value creates a stronger feeling of depth while a lower value is more comfortable.");
  static const char TR_3D_CONVERGENCE_DESCRIPTION[] = QT_TR_NOOP(
      "Controls the distance of the convergence plane. This is the distance at which "
      "virtual objects will appear to be in front of the screen.<br><br>A higher value creates "
      "stronger out-of-screen effects while a lower value is more comfortable.");
  static const char TR_3D_SWAP_EYES_DESCRIPTION[] = QT_TR_NOOP(
      "Swaps the left and right eye. Most useful in side-by-side stereoscopy "
      "mode.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_3D_PER_EYE_RESOLUTION_DESCRIPTION[] =
      QT_TR_NOOP("Whether each eye gets full or half image resolution when using side-by-side "
                 "or above-and-below 3D."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_FORCE_24BIT_DESCRIPTION[] = QT_TR_NOOP(
      "Forces the game to render the RGB color channels in 24-bit, thereby increasing "
      "quality by reducing color banding.<br><br>Has no impact on performance and causes "
      "few graphical issues.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_DISABLE_COPY_FILTER_DESCRIPTION[] = QT_TR_NOOP(
      "Disables the blending of adjacent rows when copying the EFB. This is known in "
      "some games as \"deflickering\" or \"smoothing\".<br><br>Disabling the filter has no "
      "effect on performance, but may result in a sharper image. Causes few "
      "graphical issues.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION[] = QT_TR_NOOP(
      "Enables detection of arbitrary mipmaps, which some games use for special distance-based "
      "effects.<br><br>May have false positives that result in blurry textures at increased "
      "internal "
      "resolution, such as in games that use very low resolution mipmaps. Disabling this can also "
      "reduce stutter in games that frequently load new textures.<br><br>If this setting is "
      "enabled, GPU Texture Decoding will be disabled.<br><br><dolphin_emphasis>If "
      "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_HDR_DESCRIPTION[] = QT_TR_NOOP(
      "Enables scRGB HDR output (if supported by your graphics backend and monitor)."
      " Fullscreen might be required."
      "<br><br>This gives post process shaders more room for accuracy, allows \"AutoHDR\" "
      "post-process shaders to work, and allows to fully display the PAL and NTSC-J color spaces."
      "<br><br>Note that games still render in SDR internally."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->internalResolutionComboBox, tr("Internal Resolution"),
                               tr(TR_INTERNAL_RESOLUTION_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->antiAliasingComboBox, tr("Anti-Aliasing"),
                               tr(TR_ANTIALIAS_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->textureFilteringComboBox, tr("Texture Filtering"),
                               tr(TR_FORCE_TEXTURE_FILTERING_DESCRIPTION));

  // The description hangs off Browse… rather than the field: the field is read-only, so the button
  // is what a user hovers to find out what the row does.
  ConfigWidget::SetDescription(m_ui->postProcessingBrowseButton, tr("Post-Processing Effect"),
                               tr(TR_POSTPROCESSING_DESCRIPTION));

  // Its own description rather than none: the row's, on Browse… above, is about choosing a preset
  // and says nothing about editing one. The same unbound-button help pattern is used for the
  // same reason -- the field it belongs to is read-only, so the buttons carry the row's help.
  // When librashader did not load, the button is disabled (UpdateParametersButtonState) and this is
  // where the user finds out why: Qt still delivers hover events to a disabled widget, so the
  // balloon still opens. Composed once rather than per state change because availability is decided
  // by the first GetAvailability() call and cached for the process.
  const VideoCommon::Librashader::Availability& librashader =
      VideoCommon::Librashader::GetAvailability();
  if (librashader.available)
  {
    ConfigWidget::SetDescription(m_ui->postProcessingParametersButton, tr("Shader Parameters"),
                                 tr(TR_POSTPROCESSING_PARAMETERS_DESCRIPTION));
  }
  else
  {
    ConfigWidget::SetDescription(
        m_ui->postProcessingParametersButton, tr("Shader Parameters"),
        tr("Unavailable: editing shader parameters needs the librashader library, which could "
           "not be loaded (%1). Post-processing itself still works.<br><br>%2")
            .arg(QString::fromStdString(librashader.reason),
                 tr(TR_POSTPROCESSING_PARAMETERS_DESCRIPTION)));
  }

  ConfigWidget::SetDescription(m_ui->scaledEfbCopyCheckBox, {}, tr(TR_SCALED_EFB_COPY_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->perPixelLightingCheckBox, {},
                               tr(TR_PER_PIXEL_LIGHTING_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->widescreenHackCheckBox, {},
                               tr(TR_WIDESCREEN_HACK_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->disableFogCheckBox, {}, tr(TR_REMOVE_FOG_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->force24BitColorCheckBox, {}, tr(TR_FORCE_24BIT_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->disableCopyFilterCheckBox, {},
                               tr(TR_DISABLE_COPY_FILTER_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->arbitraryMipmapDetectionCheckBox, {},
                               tr(TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->hdrCheckBox, {}, tr(TR_HDR_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->stereoModeComboBox, tr("Stereoscopic 3D Mode"),
                               tr(TR_3D_MODE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->stereoDepthSlider, tr("Depth"), tr(TR_3D_DEPTH_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->stereoConvergenceSlider, tr("Convergence"),
                               tr(TR_3D_CONVERGENCE_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->fullResolutionPerEyeCheckBox, {},
                               tr(TR_3D_PER_EYE_RESOLUTION_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->swapEyesCheckBox, {}, tr(TR_3D_SWAP_EYES_DESCRIPTION));
}

void EnhancementsWidget::DownloadShaderPack(const std::string& pack_id, const std::string& profile)
{
  // Parented to `this` for window modality, and heap-allocated because of that parenting: ~QObject
  // deletes its children, so a stack-allocated child would be `delete`d at a stack address if this
  // widget were destroyed while the nested event loop below is spinning.
  auto* const progress =
      new QProgressDialog(tr("Downloading slang shader pack…"), tr("Cancel"), 0, 100, this);
  progress->setWindowModality(Qt::WindowModal);
  progress->setMinimumDuration(0);
  progress->setValue(0);

  // Both directions of worker traffic go through this shared, refcounted state -- percent out,
  // cancellation in -- so the worker holds no pointer into this stack frame and none to a QObject.
  // QWidget state may only be touched on the GUI thread anyway, and the dialog is a child of
  // `this`, which can be destroyed while the worker is still running.
  struct DownloadState
  {
    std::atomic<int> percent{0};
    std::atomic<bool> canceled{false};
  };
  const auto state = std::make_shared<DownloadState>();

  connect(progress, &QProgressDialog::canceled, progress, [state] { state->canceled = true; });

  // Progress is polled on the GUI thread rather than pushed from the worker, because pushing
  // needs a pointer to the dialog on the worker side. The re-entrancy guard is the QTBUG-10561
  // one: a modal QProgressDialog::setValue() spins the event loop, which can dispatch the next
  // timeout inside it.
  //
  // QtUtils/ParallelProgressDialog::SetValueSlot has that same guard under that same bug number and
  // seven other DolphinQt sites use it, so this near-duplicate is a decision, not an oversight:
  // that class holds its QProgressDialog *by value* while handing the caller's widget to it as a
  // parent, so ~QWidget would `delete` a member address. Heap-allocating the dialog is precisely
  // what makes this function survive `this` being destroyed mid-download, and adopting the wrapper
  // would trade that away to save the five lines below.
  //
  // `progress` is captured raw here while the post-loop code goes through a QPointer, deliberately:
  // the connection's context object is `progress` itself, so this lambda cannot be invoked after
  // the dialog is gone, and the one hazard a null test could not answer anyway -- `this` being
  // destroyed inside setValue()'s nested dispatch, which deletes `progress` as one of its children
  // -- is not visible to a check made before the call. The post-loop code needs the QPointer
  // because nothing scopes it to the dialog's lifetime.
  auto* const poll = new QTimer(progress);
  connect(poll, &QTimer::timeout, progress, [progress, state, setting = false]() mutable {
    if (setting)
      return;
    setting = true;
    progress->setValue(state->percent);
    setting = false;
  });
  poll->start(100);

  const auto on_progress = [state](s64 downloaded, s64 total) -> bool {
    state->percent = total > 0 ? static_cast<int>((downloaded * 100) / total) : 0;
    return !state->canceled;
  };

  const std::string dest_root = File::GetUserPath(D_SHADERS_IDX);
  QEventLoop loop;
  // A nested event loop can outlive the object that started it. Stop dispatching events through a
  // destroyed widget, and re-check below before touching `this` or its children again.
  const QPointer<EnhancementsWidget> self(this);
  connect(this, &QObject::destroyed, &loop, &QEventLoop::quit);
  // std::async rather than QtConcurrent::run: the Qt subset vendored for the Windows build
  // (Externals/Qt) ships Core, Gui, Svg, SvgWidgets and Widgets only, so QtConcurrent is not
  // available there -- and DolphinQt never linked Qt6::Concurrent in the first place, so the macOS
  // build was relying on a header that happened to be in the same prefix. ConvertDialog.cpp already
  // drives its background conversions this way.
  //
  // The worker wakes the loop by posting to it rather than by emitting a signal, which is what
  // QFutureWatcher::finished did for us before. Posting to a stack-local QEventLoop is safe for the
  // same reason that was: the post always happens before DownloadShaderPackById returns, so `loop`
  // is still alive when it is made, and ~QObject purges any event still undelivered by the time the
  // loop goes out of scope below.
  std::future<VideoCommon::ShaderPackDownloadResult> future =
      std::async(std::launch::async, [pack_id, dest_root, on_progress, profile, &loop] {
        VideoCommon::ShaderPackDownloadResult result =
            VideoCommon::DownloadShaderPackById(pack_id, dest_root, on_progress, profile);
        QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        return result;
      });
  loop.exec();

  // `future` and `loop` are locals, so the worker has to be finished before this returns, and
  // future.get() is the only thing that guarantees it. loop.exec() returning does not:
  // QCoreApplication::exit() sets quitNow, which unwinds every loop in the thread's stack of them,
  // nested ones included, and the destroyed() connection above exits this one deliberately.
  // get() blocks until the worker completes on all three paths -- so if nothing is left to show
  // the answer to, ask the worker to stop rather than making teardown wait out a whole download.
  if (self)
  {
    // stop() before close(): closing a dialog does not stop a timer that happens to be its child,
    // and the deleteLater() below is posted at *this* loop level, so it is not dispatched inside
    // the nested loop of the QMessageBox that follows. Without this the poll would keep firing
    // setValue() on the hidden dialog for as long as that message box is up.
    poll->stop();
    progress->close();
  }
  else
  {
    state->canceled = true;
  }

  const VideoCommon::ShaderPackDownloadResult result = future.get();
  if (!self)
    return;

  progress->deleteLater();

  if (result.ok)
  {
    // Nothing to refresh here any more: the picker enumerates the Shaders folders each time it is
    // opened, so a freshly installed pack shows up on the next Browse….
    QMessageBox::information(this, tr("Shader Pack Installed"),
                             tr("Installed %1 shader presets.").arg(result.preset_count));
  }
  else
  {
    QMessageBox::warning(this, tr("Download Failed"), QString::fromStdString(result.error));
  }
}
