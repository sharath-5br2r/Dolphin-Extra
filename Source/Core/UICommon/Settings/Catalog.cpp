// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/Settings/Catalog.h"

#include <algorithm>
#include <cstdlib>

#include "Common/BigPicture/Theme.h"
#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "VideoCommon/VideoConfig.h"

namespace UICommon::Settings
{
Catalog::Catalog(std::vector<Entry> entries) : m_entries(std::move(entries))
{
  for (std::size_t i = 0; i < m_entries.size(); ++i)
  {
    const auto& entry = m_entries[i];
    if (entry.descriptor.id.empty() || !entry.read || !entry.inherited || !entry.write)
    {
      ERROR_LOG_FMT(COMMON, "Invalid settings catalog entry: {}", entry.descriptor.id);
      std::abort();
    }
    for (std::size_t j = 0; j < i; ++j)
      if (m_entries[j].descriptor.id == entry.descriptor.id ||
          m_entries[j].descriptor.location == entry.descriptor.location)
      {
        ERROR_LOG_FMT(COMMON, "Duplicate settings catalog entry: {}", entry.descriptor.id);
        std::abort();
      }
  }
}

const Entry* Catalog::Find(std::string_view id) const
{
  const auto it =
      std::ranges::find(m_entries, id, [](const Entry& entry) { return entry.descriptor.id; });
  return it == m_entries.end() ? nullptr : &*it;
}

const Entry* Catalog::Find(const Config::Location& location) const
{
  const auto it = std::ranges::find(m_entries, location,
                                    [](const Entry& entry) { return entry.descriptor.location; });
  return it == m_entries.end() ? nullptr : &*it;
}

const Catalog& GetCatalog()
{
  static const Catalog catalog([] {
    std::vector<Entry> entries;
    const auto add = [&]<typename T>(const Config::Info<T>& setting, const char* id,
                                     const char* category, const char* label, Control control,
                                     bool per_game = false,
                                     ApplyPolicy policy = ApplyPolicy::Live) -> Entry& {
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
    add(Config::MAIN_START_BIG_PICTURE, "interface.start_big_picture", "Interface",
        _trans("Start Dolphin in Big Picture Mode"), Control::Toggle, false, ApplyPolicy::NextBoot);
    auto& theme = add(Config::MAIN_BIG_PICTURE_THEME, "interface.big_picture_theme", "Interface",
                      _trans("Big Picture Theme"), Control::Choice);
    for (const auto& choice : BigPicture::THEME_CHOICES)
      theme.descriptor.choices.push_back({std::string(choice.id), choice.label});
    add(Config::MAIN_AUDIO_VOLUME, "audio.volume", "Audio", _trans("Volume"), Control::Number)
        .descriptor.range = Range{0, 100, 1, 1, 0, "%"};
    add(Config::MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT, "audio.mute_unlimited", "Audio",
        _trans("Mute on Unlimited Speed"), Control::Toggle);
    add(Config::MAIN_AUDIO_BACKEND, "audio.backend", "Audio", _trans("Audio Backend"),
        Control::Choice, false, ApplyPolicy::IdleOnly)
        .options = [](const Environment& environment) { return environment.audio_backends; };
    auto& backend = add(Config::MAIN_GFX_BACKEND, "display.backend", "Display",
                        _trans("Graphics Backend"), Control::Choice, true, ApplyPolicy::IdleOnly);
    backend.options = [](const Environment& environment) { return environment.graphics_backends; };
    backend.descriptor.netplay_safe = false;
    auto& aspect = add(Config::GFX_ASPECT_RATIO, "display.aspect_ratio", "Display",
                       _trans("Aspect Ratio"), Control::Choice, true);
    aspect.descriptor.choices = {{ToValue(AspectMode::Auto), _trans("Auto")},
                                 {ToValue(AspectMode::ForceWide), _trans("Force 16:9")},
                                 {ToValue(AspectMode::ForceStandard), _trans("Force 4:3")},
                                 {ToValue(AspectMode::Stretch), _trans("Stretch to Window")},
                                 {ToValue(AspectMode::Custom), _trans("Custom")},
                                 {ToValue(AspectMode::CustomStretch), _trans("Custom (Stretch)")}};
    add(Config::GFX_VSYNC, "display.vsync", "Display", _trans("V-Sync"), Control::Toggle, true);
    add(Config::GFX_INTEGER_SCALING, "display.integer_scaling", "Display",
        _trans("Integer Scaling"), Control::Toggle, true);
    auto& preset = add(Config::GFX_ENHANCE_POST_SHADER, "display.post_processing_preset", "Display",
                       _trans("Post-Processing Preset"), Control::Path, true);
    preset.unavailable = [](const Environment& environment, const Context&) {
      return environment.post_processing ?
                 std::string{} :
                 std::string{_trans("Post-processing is unavailable with this graphics backend.")};
    };
    auto& skip = add(Config::GFX_HACK_EFB_ACCESS_ENABLE, "graphics.skip_efb_access", "Graphics",
                     _trans("Skip EFB Access from CPU"), Control::Toggle, true);
    skip.descriptor.reverse = true;
    skip.descriptor.netplay_safe = false;
    auto& accuracy = add(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, "graphics.texture_cache",
                         "Graphics", _trans("Texture Cache Accuracy"), Control::Number, true);
    accuracy.descriptor.ticks = {0, 512, 128};
    accuracy.descriptor.netplay_safe = false;
    auto& clock_enabled =
        add(Config::MAIN_OVERCLOCK_ENABLE, "advanced.overclock_enabled", "Advanced",
            _trans("Enable Emulated CPU Clock Override"), Control::Toggle);
    clock_enabled.descriptor.hardcore_safe = false;
    clock_enabled.descriptor.netplay_safe = false;
    auto& clock = add(Config::MAIN_OVERCLOCK, "advanced.cpu_clock", "Advanced",
                      _trans("Emulated CPU Clock"), Control::Number);
    clock.descriptor.range = Range{0.01, 5, 0.01, 100, 0, "%"};
    clock.descriptor.netplay_safe = false;
    clock.descriptor.hardcore_safe = false;
    clock.unavailable = [](const Environment&, const Context& context) {
      const bool enabled = context.target == EditTarget::Base ?
                               Config::GetBase(Config::MAIN_OVERCLOCK_ENABLE) :
                               Logic::Read(Config::MAIN_OVERCLOCK_ENABLE, context.layer.get(),
                                           context.fallback.get());
      return enabled ? std::string{} : std::string{_trans("Enable CPU clock override first.")};
    };
    auto& memory = add(Config::MAIN_MEM1_SIZE, "advanced.mem1_size", "Advanced",
                       _trans("MEM1 Size"), Control::Number, false, ApplyPolicy::IdleOnly);
    memory.descriptor.range =
        Range{24 * 1048576.0, 64 * 1048576.0, 1048576, 1.0 / 1048576, 0, "MiB"};
    memory.descriptor.netplay_safe = false;
    memory.descriptor.hardcore_safe = false;
    memory.unavailable = [](const Environment&, const Context& context) {
      const bool enabled = context.target == EditTarget::Base ?
                               Config::GetBase(Config::MAIN_RAM_OVERRIDE_ENABLE) :
                               Logic::Read(Config::MAIN_RAM_OVERRIDE_ENABLE, context.layer.get(),
                                           context.fallback.get());
      return enabled ? std::string{} : std::string{_trans("Enable RAM override first.")};
    };
    auto& resolution = add(Config::GFX_EFB_SCALE, "display.internal_resolution", "Display",
                           _trans("Internal Resolution"), Control::Choice, true);
    resolution.options = [](const Environment& environment) {
      std::vector<Choice> choices{{0, _trans("Auto (Multiple of 640x528)")},
                                  {1, _trans("Native (640x528)")}};
      for (int scale = 2; scale <= std::clamp(environment.max_efb_scale, 2, 64); ++scale)
        choices.push_back({scale, std::to_string(scale) + "x (" + std::to_string(640 * scale) +
                                      "x" + std::to_string(528 * scale) + ")"});
      return choices;
    };
    const std::pair<const char*, const char*> help[] = {
        {"interface.start_big_picture",
         _trans("Open the controller-operated library when Dolphin starts.")},
        {"interface.big_picture_theme",
         _trans("Choose the appearance of Big Picture menus. Changes apply immediately.")},
        {"audio.volume", _trans("Adjust the volume of emulated audio.")},
        {"audio.mute_unlimited", _trans("Mute audio while the emulation speed limit is disabled.")},
        {"audio.backend",
         _trans("Select an available audio output backend. Stop emulation before changing it.")},
        {"display.backend",
         _trans("Select an available graphics backend for Big Picture and emulation.")},
        {"display.aspect_ratio", _trans("Choose how the game image fits the display.")},
        {"display.vsync", _trans("Synchronize presentation with the display to reduce tearing.")},
        {"display.integer_scaling", _trans("Scale the game image using whole-number factors.")},
        {"display.post_processing_preset",
         _trans("Choose a post-processing preset using the existing preset path.")},
        {"graphics.skip_efb_access",
         _trans("Skip CPU access to the embedded framebuffer. Some games require this access.")},
        {"graphics.texture_cache", _trans("Choose an exact texture-cache accuracy level.")},
        {"advanced.overclock_enabled", _trans("Enable a custom emulated CPU clock rate.")},
        {"advanced.cpu_clock",
         _trans("Adjust the emulated CPU clock as a percentage of its normal rate.")},
        {"advanced.mem1_size",
         _trans("Set the MEM1 memory size in MiB. Stop emulation before changing it.")},
        {"display.internal_resolution", _trans("Choose the internal rendering resolution scale.")}};
    for (const auto& [id, text] : help)
      std::ranges::find(entries, std::string_view(id), [](const Entry& e) {
        return e.descriptor.id;
      })->descriptor.help = text;
    AppendApplicationSettings(entries);
    return entries;
  }());
  return catalog;
}
}  // namespace UICommon::Settings
