// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/Settings/Catalog.h"

#include <algorithm>
#include <type_traits>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Enums.h"
#include "Common/Common.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/MemoryCardSyncSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/Config/UISettings.h"
#include "VideoCommon/VideoConfig.h"

namespace UICommon::Settings
{
namespace
{
template <typename Info>
using SettingType = std::remove_cvref_t<decltype(std::declval<Info>().GetDefaultValue())>;
}

void AppendApplicationSettings(std::vector<Entry>& entries)
{
  const auto add = [&]<typename T>(const Config::Info<T>& setting, const char* id,
                                   const char* category, const char* label, Control control,
                                   bool per_game = false,
                                   ApplyPolicy policy = ApplyPolicy::Live) -> Entry& {
    const auto existing = std::ranges::find(entries, setting.GetLocation(), [](const auto& entry) {
      return entry.descriptor.location;
    });
    if (existing != entries.end())
      return *existing;
    Descriptor descriptor;
    descriptor.id = id;
    descriptor.category = category;
    descriptor.order = static_cast<int>(entries.size());
    descriptor.label = label;
    descriptor.help = label;
    descriptor.control = control;
    descriptor.per_game = per_game;
    descriptor.policy = policy;
    entries.push_back(MakeEntry(setting, std::move(descriptor)));
    return entries.back();
  };

#include "UICommon/Settings/ApplicationSettings.inc"

  auto& language = add(Config::MAIN_INTERFACE_LANGUAGE, "interface.language", "Interface",
                       _trans("Language"), Control::Choice, false, ApplyPolicy::NextBoot);
  language.options = [](const Environment& env) { return env.languages; };
  language.descriptor.help =
      _trans("Select the interface language. Restart Dolphin to apply the change.");
  auto& aspect_height = add(Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT, "display.custom_aspect_height",
                            "Display", _trans("Custom Aspect Ratio Height"), Control::Number, true);
  aspect_height.descriptor.range = Range{1, 10000, 1, 1, 0, ""};
  aspect_height.descriptor.help = _trans("Set the height component of a custom aspect ratio.");
  auto& speed = add(Config::MAIN_EMULATION_SPEED, "general.speed", "General", _trans("Speed Limit"),
                    Control::Number);
  speed.descriptor.range = Range{0, 2, 0.1, 100, 0, "%"};
  speed.descriptor.help = _trans("Limit emulation speed. Zero selects unlimited speed.");
  auto& cpu = add(Config::MAIN_CPU_CORE, "advanced.cpu_engine", "Advanced",
                  _trans("CPU Emulation Engine"), Control::Choice, false, ApplyPolicy::IdleOnly);
  cpu.options = [](const Environment& env) { return env.cpu_engines; };
  cpu.descriptor.hardcore_safe = false;
  auto& adapter = add(Config::GFX_ADAPTER, "display.adapter", "Display", _trans("Graphics Adapter"),
                      Control::Choice, true, ApplyPolicy::IdleOnly);
  adapter.options = [](const Environment& env) { return env.graphics_adapters; };
  adapter.unavailable = [](const Environment& env, const Context&) {
    return env.graphics_adapters.empty() ?
               std::string(_trans("Adapter selection is unavailable with this graphics backend.")) :
               std::string{};
  };
  auto& vbi = add(Config::MAIN_VI_OVERCLOCK, "advanced.vbi_rate", "Advanced",
                  _trans("VBI Rate Override"), Control::Number, false, ApplyPolicy::IdleOnly);
  vbi.descriptor.range = Range{0.01, 5, 0.01, 100, 0, "%"};
  auto& mem2 = add(Config::MAIN_MEM2_SIZE, "advanced.mem2_size", "Advanced", _trans("MEM2 Size"),
                   Control::Number, false, ApplyPolicy::IdleOnly);
  mem2.descriptor.range = Range{64 * 1048576.0, 256 * 1048576.0, 1048576, 1.0 / 1048576, 0, "MiB"};

  auto& aa = add(Config::GFX_MSAA, "graphics.anti_aliasing", "Enhancements",
                 _trans("Anti-Aliasing Samples"), Control::Choice, true);
  aa.options = [](const Environment& env) { return env.anti_aliasing; };
  auto& ssaa = add(Config::GFX_SSAA, "graphics.ssaa", "Enhancements",
                   _trans("Supersampling Anti-Aliasing"), Control::Toggle, true);
  ssaa.unavailable = [](const Environment& env, const Context&) {
    return env.ssaa ?
               std::string{} :
               std::string(_trans("Supersampling is unavailable with this graphics backend."));
  };
  auto& aniso = add(Config::GFX_ENHANCE_MAX_ANISOTROPY, "graphics.anisotropy", "Enhancements",
                    _trans("Anisotropic Filtering"), Control::Choice, true);
  aniso.descriptor.choices = {{ToValue(AnisotropicFilteringMode::Default), _trans("Default")},
                              {ToValue(AnisotropicFilteringMode::Force1x), _trans("1x")},
                              {ToValue(AnisotropicFilteringMode::Force2x), _trans("2x")},
                              {ToValue(AnisotropicFilteringMode::Force4x), _trans("4x")},
                              {ToValue(AnisotropicFilteringMode::Force8x), _trans("8x")},
                              {ToValue(AnisotropicFilteringMode::Force16x), _trans("16x")}};
  auto& filtering = add(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING, "graphics.texture_filtering",
                        "Enhancements", _trans("Texture Filtering"), Control::Choice, true);
  filtering.descriptor.choices = {{ToValue(TextureFilteringMode::Default), _trans("Default")},
                                  {ToValue(TextureFilteringMode::Nearest), _trans("Nearest")},
                                  {ToValue(TextureFilteringMode::Linear), _trans("Linear")}};
  auto& stereo = add(Config::GFX_STEREO_MODE, "graphics.stereo_mode", "Enhancements",
                     _trans("Stereoscopic 3D Mode"), Control::Choice, true);
  stereo.descriptor.choices = {{ToValue(StereoMode::Off), _trans("Off")},
                               {ToValue(StereoMode::SideBySide), _trans("Side-by-Side")},
                               {ToValue(StereoMode::TopAndBottom), _trans("Top-and-Bottom")},
                               {ToValue(StereoMode::QuadBuffer), _trans("Quad Buffer")}};
  auto& latency = add(Config::MAIN_AUDIO_LATENCY, "audio.latency", "Audio", _trans("Audio Latency"),
                      Control::Number, false, ApplyPolicy::IdleOnly);
  latency.descriptor.range = Range{0, 200, 1, 1, 0, "ms"};
  auto& hle = add(Config::MAIN_DSP_HLE, "audio.dsp_hle", "Audio", _trans("DSP HLE Emulation"),
                  Control::Toggle, false, ApplyPolicy::IdleOnly);
  hle.descriptor.help = _trans("Use the HLE DSP engine. Turn off to use LLE.");
  add(Config::MAIN_DSP_JIT, "audio.dsp_jit", "Audio", _trans("DSP LLE Recompiler"), Control::Toggle,
      false, ApplyPolicy::IdleOnly);
  auto& quality = add(Config::MAIN_DPL2_QUALITY, "audio.dolby_quality", "Audio",
                      _trans("Dolby Pro Logic II Decoder Quality"), Control::Choice, false,
                      ApplyPolicy::IdleOnly);
  quality.descriptor.choices = {{ToValue(AudioCommon::DPL2Quality::Lowest), _trans("Lowest")},
                                {ToValue(AudioCommon::DPL2Quality::Low), _trans("Low")},
                                {ToValue(AudioCommon::DPL2Quality::High), _trans("High")},
                                {ToValue(AudioCommon::DPL2Quality::Highest), _trans("Highest")}};
#ifdef _WIN32
  auto& wasapi = add(Config::MAIN_WASAPI_DEVICE, "audio.wasapi_device", "Audio",
                     _trans("WASAPI Device"), Control::Choice, false, ApplyPolicy::IdleOnly);
  wasapi.options = [](const Environment& env) { return env.wasapi_devices; };
#endif
  auto& wide = add(Config::SYSCONF_WIDESCREEN, "wii.aspect", "Wii", _trans("Aspect Ratio"),
                   Control::Toggle, false, ApplyPolicy::IdleOnly);
  wide.descriptor.help = _trans("Enable the Wii widescreen setting.");
  auto& sensor = add(Config::SYSCONF_SENSOR_BAR_POSITION, "wii.sensor_bar_position", "Wii",
                     _trans("Sensor Bar Position"), Control::Choice, false, ApplyPolicy::IdleOnly);
  sensor.descriptor.choices = {{u32{1}, _trans("Top")}, {u32{0}, _trans("Bottom")}};
  auto& sd = add(Config::MAIN_WII_SD_CARD_FILESIZE, "wii.sd_card_size", "Wii",
                 _trans("SD Card Size"), Control::Choice, false, ApplyPolicy::IdleOnly);
  sd.descriptor.choices = {{u64{0}, _trans("Auto")}};
  for (const u64 mib : {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768})
    sd.descriptor.choices.push_back({mib * 1048576, std::to_string(mib) + " MiB"});

#ifdef USE_RETRO_ACHIEVEMENTS
  add(Config::RA_ENABLED, "achievements.enabled", "Achievements",
      _trans("Enable RetroAchievements"), Control::Toggle, false, ApplyPolicy::NextBoot);
  add(Config::RA_HARDCORE_ENABLED, "achievements.hardcore", "Achievements", _trans("Hardcore Mode"),
      Control::Toggle, false, ApplyPolicy::IdleOnly);
  add(Config::RA_UNOFFICIAL_ENABLED, "achievements.unofficial", "Achievements",
      _trans("Unofficial Achievements"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  add(Config::RA_ENCORE_ENABLED, "achievements.encore", "Achievements", _trans("Encore Mode"),
      Control::Toggle, false, ApplyPolicy::IdleOnly);
  add(Config::RA_SPECTATOR_ENABLED, "achievements.spectator", "Achievements",
      _trans("Spectator Mode"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  add(Config::RA_LEADERBOARD_TRACKER_ENABLED, "achievements.leaderboards", "Achievements",
      _trans("Leaderboard Tracker"), Control::Toggle);
  add(Config::RA_CHALLENGE_INDICATORS_ENABLED, "achievements.challenges", "Achievements",
      _trans("Challenge Indicators"), Control::Toggle);
  add(Config::RA_PROGRESS_ENABLED, "achievements.progress", "Achievements",
      _trans("Achievement Progress"), Control::Toggle);
#endif

  const auto find = [&](const auto& setting) -> Entry& {
    return *std::ranges::find(entries, setting.GetLocation(),
                              [](const auto& entry) { return entry.descriptor.location; });
  };
  const auto require = [&](const auto& setting, const Config::Info<bool>& parent) {
    find(setting).unavailable = [parent](const Environment&, const Context& context) {
      const bool enabled = context.target == EditTarget::Base ?
                               Config::GetBase(parent) :
                               Logic::Read(parent, context.layer.get(), context.fallback.get());
      return enabled ? std::string{} : std::string(_trans("Enable the parent setting first."));
    };
  };
  require(Config::GFX_CACHE_HIRES_TEXTURES, Config::GFX_HIRES_TEXTURES);
  require(Config::GFX_DUMP_BASE_TEXTURES, Config::GFX_DUMP_TEXTURES);
  require(Config::GFX_DUMP_MIP_TEXTURES, Config::GFX_DUMP_TEXTURES);
  require(Config::MAIN_VI_OVERCLOCK, Config::MAIN_VI_OVERCLOCK_ENABLE);
  require(Config::MAIN_MEM2_SIZE, Config::MAIN_RAM_OVERRIDE_ENABLE);
  require(Config::GFX_CROP_CUSTOM_LEFT, Config::GFX_CROP_CUSTOM);
  require(Config::GFX_CROP_CUSTOM_TOP, Config::GFX_CROP_CUSTOM);
  require(Config::GFX_CROP_CUSTOM_RIGHT, Config::GFX_CROP_CUSTOM);
  require(Config::GFX_CROP_CUSTOM_BOTTOM, Config::GFX_CROP_CUSTOM);
  const auto read = [](const auto& setting, const Context& context) {
    return context.target == EditTarget::Base ?
               Config::GetBase(setting) :
               Logic::Read(setting, context.layer.get(), context.fallback.get());
  };
  for (const auto& location : {Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH.GetLocation(),
                               Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT.GetLocation()})
  {
    auto& entry =
        *std::ranges::find(entries, location, [](const auto& e) { return e.descriptor.location; });
    entry.unavailable = [read](const Environment&, const Context& context) {
      const auto aspect = read(Config::GFX_ASPECT_RATIO, context);
      return aspect == AspectMode::Custom || aspect == AspectMode::CustomStretch ?
                 std::string{} :
                 std::string(_trans("Select a custom aspect ratio first."));
    };
  }
  find(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH).descriptor.label =
      _trans("Custom Aspect Ratio Width");
  find(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH).descriptor.help =
      _trans("Set the width component of a custom aspect ratio.");
  find(Config::MAIN_DPL2_DECODER).unavailable = [read](const Environment&, const Context& context) {
    return !read(Config::MAIN_DSP_HLE, context) &&
                   AudioCommon::SupportsDPL2Decoder(read(Config::MAIN_AUDIO_BACKEND, context)) ?
               std::string{} :
               std::string(_trans(
                   "Requires DSP LLE and an audio backend that supports surround decoding."));
  };
  find(Config::MAIN_DPL2_QUALITY).unavailable = [read](const Environment&, const Context& context) {
    return !read(Config::MAIN_DSP_HLE, context) && read(Config::MAIN_DPL2_DECODER, context) &&
                   AudioCommon::SupportsDPL2Decoder(read(Config::MAIN_AUDIO_BACKEND, context)) ?
               std::string{} :
               std::string(_trans("Enable supported Dolby Pro Logic II decoding first."));
  };
  find(Config::MAIN_DSP_JIT).unavailable = [read](const Environment&, const Context& context) {
    return read(Config::MAIN_DSP_HLE, context) ?
               std::string(_trans("Select DSP LLE to use this setting.")) :
               std::string{};
  };
  find(Config::MAIN_AUDIO_LATENCY).unavailable = [read](const Environment&,
                                                        const Context& context) {
    return AudioCommon::SupportsLatencyControl(read(Config::MAIN_AUDIO_BACKEND, context)) ?
               std::string{} :
               std::string(_trans("This audio backend does not support latency control."));
  };
#ifdef _WIN32
  find(Config::MAIN_WASAPI_DEVICE).unavailable = [read](const Environment&,
                                                        const Context& context) {
    return read(Config::MAIN_AUDIO_BACKEND, context) == BACKEND_WASAPI ?
               std::string{} :
               std::string(_trans("Select the WASAPI audio backend first."));
  };
#endif
  for (const auto& location : {Config::MAIN_WIIMOTE_AUDIO_ROUTING_ENABLED.GetLocation(),
                               Config::MAIN_GBA_AUDIO_ROUTING_ENABLED.GetLocation()})
  {
    auto& entry =
        *std::ranges::find(entries, location, [](const auto& e) { return e.descriptor.location; });
    entry.descriptor.policy = ApplyPolicy::IdleOnly;
    const bool wiimote = location == Config::MAIN_WIIMOTE_AUDIO_ROUTING_ENABLED.GetLocation();
    entry.unavailable = [read, wiimote](const Environment&, const Context& context) {
      return read(Config::MAIN_AUDIO_BACKEND, context) == "Cubeb" &&
                     (!wiimote || (read(Config::MAIN_WIIMOTE_ENABLE_SPEAKER, context) &&
                                   !read(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED, context))) ?
                 std::string{} :
                 std::string(_trans("Requires Cubeb and a supported emulated device."));
    };
  }
  for (auto& entry : entries)
  {
    if (entry.descriptor.category == "MemoryCards")
    {
      entry.descriptor.policy = ApplyPolicy::NextBoot;
      if (entry.descriptor.control == Control::Path)
        entry.descriptor.control = Control::Text;
      entry.descriptor.help +=
          "\n" + std::string(_trans("Restart Dolphin to apply memory card sync configuration "
                                    "changes. Credentials are managed in desktop settings."));
    }
    else if (entry.descriptor.category == "Paths" && entry.descriptor.control == Control::Path)
      entry.descriptor.help +=
          "\n" + std::string(_trans(
                     "Restart Dolphin to apply changes to resource and storage directories."));
  }
  // Pickers browse for these; the remaining paths name files.
  for (const auto* folder :
       {&Config::MAIN_GBA_SAVES_PATH, &Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH,
        &Config::MAIN_FS_PATH, &Config::MAIN_DUMP_PATH, &Config::MAIN_LOAD_PATH,
        &Config::MAIN_RESOURCEPACK_PATH, &Config::MAIN_WFS_PATH})
    find(*folder).descriptor.directory = true;
  find(Config::GFX_ENABLE_GPU_TEXTURE_DECODING).unavailable = [](const Environment& env,
                                                                 const Context& context) {
    const bool mipmaps = context.target == EditTarget::Base ?
                             Config::GetBase(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION) :
                             Logic::Read(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
                                         context.layer.get(), context.fallback.get());
    return env.gpu_texture_decoding && !mipmaps ?
               std::string{} :
               std::string(_trans("Requires GPU texture decoding support and Arbitrary Mipmap "
                                  "Detection to be off."));
  };
  find(Config::GFX_HACK_BBOX_ENABLE).unavailable = [](const Environment& env, const Context&) {
    return env.bounding_box ?
               std::string{} :
               std::string(_trans("Bounding boxes are unavailable with this graphics backend."));
  };
  find(Config::MAIN_TIME_TRACKING).descriptor.policy = ApplyPolicy::IdleOnly;
  find(Config::MAIN_SHOW_CURSOR).descriptor.label = _trans("Mouse Cursor Visibility");
  find(Config::MAIN_SHOW_CURSOR).descriptor.help = _trans("Choose when to show the mouse cursor.");
  find(Config::GFX_ENHANCE_HDR_OUTPUT).unavailable = [](const Environment& env, const Context&) {
    return env.hdr ? std::string{} :
                     std::string(_trans("HDR output is unavailable with this graphics backend."));
  };
  for (const auto& location :
       {Config::GFX_STEREO_MODE.GetLocation(), Config::GFX_STEREO_DEPTH.GetLocation(),
        Config::GFX_STEREO_CONVERGENCE.GetLocation(), Config::GFX_STEREO_SWAP_EYES.GetLocation(),
        Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL.GetLocation()})
    std::ranges::find(entries, location, [](const auto& e) {
      return e.descriptor.location;
    })->unavailable = [](const Environment& env, const Context&) {
      return env.stereo ?
                 std::string{} :
                 std::string(_trans("Stereoscopy is unavailable with this graphics backend."));
    };
#ifndef HAVE_CUBEB
  for (const auto& location : {Config::MAIN_WIIMOTE_AUDIO_ROUTING_ENABLED.GetLocation(),
                               Config::MAIN_GBA_AUDIO_ROUTING_ENABLED.GetLocation()})
    std::ranges::find(entries, location, [](const auto& e) {
      return e.descriptor.location;
    })->unavailable = [](const Environment&, const Context&) {
      return std::string(_trans("Requires the Cubeb audio backend."));
    };
#endif
#ifndef _WIN32
  find(Config::GFX_BORDERLESS_FULLSCREEN).unavailable = [](const Environment&, const Context&) {
    return std::string(_trans("Available on Windows only."));
  };
#endif
}
}  // namespace UICommon::Settings
