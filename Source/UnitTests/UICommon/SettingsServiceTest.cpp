// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>
#include <set>
#include <thread>

#include <gtest/gtest.h>

#include "Common/BigPicture/Theme.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "UICommon/Settings/Service.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
using namespace UICommon::Settings;

TEST(SettingsCatalogDeathTest, MissingCallbacksFailFast)
{
  EXPECT_DEATH_IF_SUPPORTED((Catalog({Entry{}})), "");
}

TEST(SettingsCatalogDeathTest, DuplicateEntriesFailFast)
{
  const auto entry = *GetCatalog().Find("audio.volume");
  EXPECT_DEATH_IF_SUPPORTED((Catalog({entry, entry})), "");
}

class MemoryLoader final : public Config::ConfigLayerLoader
{
public:
  MemoryLoader(Config::LayerType type, std::shared_ptr<Config::LayerMap> data)
      : ConfigLayerLoader(type), data(std::move(data))
  {
  }
  void Load(Config::Layer* layer) override
  {
    for (const auto& [location, value] : *data)
      if (value)
        layer->Set(location, *value);
  }
  void Save(Config::Layer* layer) override { *data = layer->GetLayerMap(); }
  std::shared_ptr<Config::LayerMap> data;
};

class SettingsServiceTest : public testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<MemoryLoader>(Config::LayerType::Base, saved));
    service = std::make_unique<Service>();
    environment.graphics_backends = {{std::string("D3D"), "Direct3D 11"},
                                     {std::string("Vulkan"), "Vulkan"}};
    environment.audio_backends = {{std::string("Null"), "No audio"}};
    environment.post_processing = true;
    service->SetEnvironment(environment);
  }
  void TearDown() override
  {
    service.reset();
    Config::Shutdown();
  }
  State Read(std::string_view id)
  {
    const auto snapshot = service->ReadSnapshot();
    const auto it = std::ranges::find(snapshot->settings, id,
                                      [](const State& state) { return state.descriptor.id; });
    EXPECT_NE(it, snapshot->settings.end());
    return it == snapshot->settings.end() ? State{} : *it;
  }
  Result Set(std::string id, Value value, Operation operation = Operation::Set)
  {
    return service->Apply(
        {service->ReadSnapshot()->revision, std::move(id), operation, std::move(value)});
  }
  Result Display(std::string id, double value)
  {
    Request request{service->ReadSnapshot()->revision, std::move(id), Operation::SetDisplay};
    request.display_value = value;
    return service->Apply(request);
  }
  std::shared_ptr<Config::LayerMap> saved = std::make_shared<Config::LayerMap>();
  Environment environment;
  std::unique_ptr<Service> service;
};

TEST_F(SettingsServiceTest, CatalogIsCompleteBeforeAnyWidgetsOrRendererExist)
{
  const auto snapshot = service->ReadSnapshot();
  EXPECT_GE(snapshot->settings.size(), 180u);
  std::set<std::string> ids;
  for (const auto& state : snapshot->settings)
  {
    EXPECT_TRUE(ids.insert(state.descriptor.id).second);
    EXPECT_FALSE(state.descriptor.label.empty());
    EXPECT_FALSE(state.descriptor.help.empty());

    EXPECT_NE(GetCatalog().Find(state.descriptor.location), nullptr);
  }
  EXPECT_EQ(Read("audio.volume").descriptor.default_value,
            ToValue(Config::MAIN_AUDIO_VOLUME.GetDefaultValue()));
  EXPECT_FALSE(Read("interface.start_big_picture").descriptor.per_game);
  EXPECT_TRUE(Read("display.vsync").descriptor.per_game);
}

TEST_F(SettingsServiceTest, ExpandedCatalogDefaultsMatchTypedRangesAndStaticChoices)
{
  const auto snapshot = service->ReadSnapshot();
  for (const auto& state : snapshot->settings)
  {
    const auto& descriptor = state.descriptor;
    SCOPED_TRACE(descriptor.id);
    if (descriptor.control == Control::Number)
      EXPECT_TRUE(state.value_available);
    if (descriptor.control == Control::Choice && !GetCatalog().Find(descriptor.id)->options)
      EXPECT_TRUE(state.value_available);
    if (descriptor.range)
    {
      EXPECT_GT(descriptor.range->scale, 0);
      EXPECT_GT(descriptor.range->step, 0);
      EXPECT_LT(descriptor.range->minimum, descriptor.range->maximum);
    }
  }
  EXPECT_TRUE(Read("graphics.anti_aliasing").descriptor.per_game);
  EXPECT_FALSE(Read("wii.sensor_bar_position").descriptor.per_game);
  EXPECT_EQ(Read("native.main_default_iso").descriptor.policy, ApplyPolicy::NextBoot);
}

TEST_F(SettingsServiceTest, ExpandedSettingsRespectBackendCapabilitiesAndParentValues)
{
  EXPECT_FALSE(Read("graphics.ssaa").enabled);
  EXPECT_FALSE(Read("native.gfx_enable_gpu_texture_decoding").enabled);
  EXPECT_FALSE(Read("native.gfx_enhance_hdr_output").enabled);
  environment.ssaa = true;
  environment.hdr = true;
  environment.gpu_texture_decoding = true;
  ASSERT_TRUE(service->SetEnvironment(environment));
  EXPECT_TRUE(Read("graphics.ssaa").enabled);
  EXPECT_TRUE(Read("native.gfx_enhance_hdr_output").enabled);
  Config::SetBase(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, false);
  EXPECT_TRUE(Read("native.gfx_enable_gpu_texture_decoding").enabled);
  Config::SetBase(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, true);
  EXPECT_FALSE(Read("native.gfx_enable_gpu_texture_decoding").enabled);
  EXPECT_FALSE(Read("native.gfx_cache_hires_textures").enabled);
  Config::SetBase(Config::GFX_HIRES_TEXTURES, true);
  EXPECT_TRUE(Read("native.gfx_cache_hires_textures").enabled);
  Config::SetBase(Config::MAIN_RAM_OVERRIDE_ENABLE, true);
  EXPECT_EQ(Display("advanced.mem2_size", 128).status, Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_MEM2_SIZE), 128u * 1048576);
  environment.emulating = true;
  ASSERT_TRUE(service->SetEnvironment(environment));
  EXPECT_EQ(Display("advanced.mem2_size", 128).status, Status::Unavailable);
  EXPECT_FALSE(Read("native.main_gba_audio_routing_enabled").enabled);
}

TEST_F(SettingsServiceTest, ThemeChoicesPersistGloballyAndPreserveUnknownStoredValues)
{
  ASSERT_TRUE(service->SetContext({EditTarget::Base}));
  for (const auto& choice : BigPicture::THEME_CHOICES)
  {
    ASSERT_EQ(Set("interface.big_picture_theme", std::string(choice.id)).status, Status::Success);
    EXPECT_EQ(Config::GetBase(Config::MAIN_BIG_PICTURE_THEME), choice.id);
  }
  ASSERT_TRUE(service->Save());
  Config::Layer reloaded(std::make_unique<MemoryLoader>(Config::LayerType::Base, saved));
  reloaded.Load();
  EXPECT_EQ(reloaded.Get(Config::MAIN_BIG_PICTURE_THEME), "DolphinBlue");
  EXPECT_EQ(Set("interface.big_picture_theme", std::string("InvalidTheme")).status,
            Status::InvalidValue);
  Config::SetBase(Config::MAIN_BIG_PICTURE_THEME, "FutureTheme");
  EXPECT_FALSE(Read("interface.big_picture_theme").value_available);
  EXPECT_EQ(Read("interface.big_picture_theme").value, Value(std::string("FutureTheme")));
  auto local = std::make_shared<Config::Layer>(Config::LayerType::LocalGame);
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local}));
  EXPECT_EQ(Set("interface.big_picture_theme", std::string("Light")).status,
            Status::UnsupportedContext);
  EXPECT_FALSE(local->Exists(Config::MAIN_BIG_PICTURE_THEME.GetLocation()));
  EXPECT_EQ(Config::GetBase(Config::MAIN_BIG_PICTURE_THEME), "FutureTheme");
  ASSERT_TRUE(service->SetContext({EditTarget::Base}));
  EXPECT_EQ(Set("interface.big_picture_theme", false, Operation::ResetDefault).status,
            Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_BIG_PICTURE_THEME), "Dark");
}

TEST_F(SettingsServiceTest, TypedValuesAndMappedChoicesAreStoredInsteadOfIndexes)
{
  EXPECT_EQ(Set("audio.volume", 37).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::MAIN_AUDIO_VOLUME), 37);
  EXPECT_EQ(Set("audio.volume", u32(37)).status, Status::InvalidValue);
  EXPECT_EQ(Set("display.aspect_ratio", int(AspectMode::ForceStandard)).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::GFX_ASPECT_RATIO), AspectMode::ForceStandard);
  EXPECT_EQ(Set("graphics.texture_cache", 512).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES), 512);
  EXPECT_EQ(Set("graphics.texture_cache", 1).status, Status::InvalidValue);
  EXPECT_EQ(Read("graphics.texture_cache").descriptor.ticks, (std::vector<Value>{0, 512, 128}));
  EXPECT_EQ(Set("display.post_processing_preset", std::string("日本語/preset.slangp")).status,
            Status::Success);
}

TEST_F(SettingsServiceTest, DisplayTransformsReverseBoolsAndPreserveExactStorage)
{
  EXPECT_EQ(Set("graphics.skip_efb_access", true, Operation::SetDisplay).status, Status::Success);
  EXPECT_FALSE(Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE));
  EXPECT_EQ(Read("graphics.skip_efb_access").display_value, Value(true));
  EXPECT_EQ(Set("display.vsync", true, Operation::SetDisplay).status, Status::Success);
  EXPECT_TRUE(Config::Get(Config::GFX_VSYNC));
  ASSERT_EQ(Set("advanced.overclock_enabled", true).status, Status::Success);
  EXPECT_EQ(Display("advanced.cpu_clock", 125).status, Status::Success);
  EXPECT_FLOAT_EQ(Config::Get(Config::MAIN_OVERCLOCK), 1.25f);
  EXPECT_EQ(Read("advanced.cpu_clock").display_value, Value(125.0f));
  EXPECT_EQ(Display("advanced.cpu_clock", 125.5).status, Status::InvalidValue);
  Config::SetBase(Config::MAIN_RAM_OVERRIDE_ENABLE, true);
  EXPECT_EQ(Display("advanced.mem1_size", 48).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::MAIN_MEM1_SIZE), 48u * 1048576);
  EXPECT_EQ(Display("advanced.mem1_size", 48.5).status, Status::InvalidValue);
}

TEST_F(SettingsServiceTest, FloatStepsAcceptEveryConvergenceTickButRejectValuesBetweenTicks)
{
  constexpr auto id = "native.gfx_stereo_convergence";
  const auto* entry = GetCatalog().Find(id);
  ASSERT_NE(entry, nullptr);
  // Limit the exhaustive tick sweep to this descriptor, rather than rebuilding unrelated rows.
  const Catalog catalog({*entry});
  Service convergence(catalog);
  environment.stereo = true;
  ASSERT_TRUE(convergence.SetEnvironment(environment));
  ASSERT_TRUE(convergence.SetContext({EditTarget::Base}));
  const auto display = [&](double value) {
    Request request{convergence.ReadSnapshot()->revision, id, Operation::SetDisplay};
    request.display_value = value;
    return convergence.Apply(request).status;
  };
  ASSERT_EQ(display(100), Status::Success);
  for (int tick = 0; tick <= 20000; ++tick)
  {
    const double value = tick / 100.0;
    ASSERT_EQ(display(value), Status::Success) << value;
    EXPECT_FLOAT_EQ(Config::GetBase(Config::GFX_STEREO_CONVERGENCE), static_cast<float>(value));
    ASSERT_TRUE(convergence.ReadSnapshot()->settings.front().value_available) << value;
  }
  for (double value : {0.005, 10.005, 99.995, 100.005, 199.995, -0.01, 200.01})
  {
    EXPECT_EQ(display(value), Status::InvalidValue) << value;
    EXPECT_FLOAT_EQ(Config::GetBase(Config::GFX_STEREO_CONVERGENCE), 200);
  }
}

TEST_F(SettingsServiceTest, InvalidNumbersAndTypesNeverChangeConfiguration)
{
  const int original = Config::Get(Config::MAIN_AUDIO_VOLUME);
  EXPECT_EQ(Set("audio.volume", -1).status, Status::InvalidValue);
  EXPECT_EQ(Set("audio.volume", 101).status, Status::InvalidValue);
  EXPECT_EQ(Set("audio.volume", std::string("50")).status, Status::InvalidValue);
  EXPECT_EQ(Config::Get(Config::MAIN_AUDIO_VOLUME), original);
  Set("advanced.overclock_enabled", true);
  EXPECT_EQ(Set("advanced.cpu_clock", std::numeric_limits<float>::quiet_NaN()).status,
            Status::InvalidValue);
  EXPECT_EQ(Display("advanced.cpu_clock", std::numeric_limits<double>::infinity()).status,
            Status::InvalidValue);
  EXPECT_EQ(Display("advanced.cpu_clock", 1e300).status, Status::InvalidValue);
  EXPECT_EQ(Set("does.not.exist", false).status, Status::UnknownSetting);
  Config::SetBase(Config::MAIN_OVERCLOCK, 1.005f);
  EXPECT_FALSE(Read("advanced.cpu_clock").value_available);
  EXPECT_FLOAT_EQ(std::get<float>(Read("advanced.cpu_clock").value), 1.005f);
}

TEST_F(SettingsServiceTest, GameOverridesInheritShippedThenBaseAndNeverWriteGlobal)
{
  Config::SetBase(Config::GFX_VSYNC, true);
  Config::SetCurrent(Config::GFX_VSYNC, false);
  auto local = std::make_shared<Config::Layer>(Config::LayerType::LocalGame);
  auto shipped = std::make_shared<Config::Layer>(Config::LayerType::GlobalGame);
  shipped->Set(Config::GFX_VSYNC, false);
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local, shipped}));
  EXPECT_EQ(Read("display.vsync").value, Value(false));
  EXPECT_EQ(Read("display.vsync").source_layer, Config::LayerType::GlobalGame);
  EXPECT_FALSE(Read("display.vsync").overridden);
  EXPECT_EQ(Set("display.vsync", true).status, Status::Success);
  EXPECT_TRUE(local->Get(Config::GFX_VSYNC));
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
  EXPECT_FALSE(Config::Get(Config::GFX_VSYNC));
  EXPECT_EQ(Set("display.vsync", false, Operation::ClearOverride).status, Status::Success);
  EXPECT_FALSE(local->Exists(Config::GFX_VSYNC.GetLocation()));
  EXPECT_EQ(Read("display.vsync").value, Value(false));
  shipped->DeleteKey(Config::GFX_VSYNC.GetLocation());
  Config::OnConfigChanged();
  EXPECT_EQ(Read("display.vsync").value, Value(true));
  EXPECT_EQ(Read("display.vsync").source_layer, Config::LayerType::Base);
}

TEST_F(SettingsServiceTest, DefaultResetAndClearingAnOverrideHaveDistinctMeaning)
{
  Config::SetBase(Config::GFX_VSYNC, true);
  auto local = std::make_shared<Config::Layer>(Config::LayerType::LocalGame);
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local}));
  EXPECT_EQ(Set("display.vsync", false, Operation::ResetDefault).status, Status::Success);
  EXPECT_TRUE(local->Exists(Config::GFX_VSYNC.GetLocation()));
  EXPECT_EQ(Read("display.vsync").value, ToValue(Config::GFX_VSYNC.GetDefaultValue()));
  EXPECT_TRUE(Read("display.vsync").overridden);
  EXPECT_EQ(Set("display.vsync", false, Operation::ClearOverride).status, Status::Success);
  EXPECT_EQ(Read("display.vsync").value, Value(true));
  EXPECT_FALSE(Read("display.vsync").overridden);
}

TEST_F(SettingsServiceTest, BaseAndEffectiveContextsPreserveDesktopRuntimeSemantics)
{
  Config::SetBase(Config::GFX_VSYNC, false);
  Config::SetCurrent(Config::GFX_VSYNC, true);
  EXPECT_EQ(Read("display.vsync").value, Value(true));
  EXPECT_EQ(Set("display.vsync", false).status, Status::Success);
  EXPECT_FALSE(Config::Get(Config::GFX_VSYNC));
  ASSERT_TRUE(service->SetContext({EditTarget::Base}));
  EXPECT_EQ(Set("display.vsync", true).status, Status::Success);
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
  EXPECT_FALSE(Config::Get(Config::GFX_VSYNC));
  EXPECT_EQ(Set("display.vsync", false, Operation::ClearOverride).status,
            Status::UnsupportedContext);
}

TEST_F(SettingsServiceTest, ContextOwnsLayersButSnapshotsDoNot)
{
  auto local = std::make_shared<Config::Layer>(Config::LayerType::LocalGame);
  std::weak_ptr<Config::Layer> weak = local;
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local}));
  const auto old = service->ReadSnapshot();
  local.reset();
  EXPECT_FALSE(weak.expired());
  ASSERT_TRUE(service->SetContext({EditTarget::Base}));
  EXPECT_TRUE(weak.expired());
  EXPECT_FALSE(old->settings.empty());
  EXPECT_EQ(service->Apply({old->revision, "display.vsync", Operation::Set, true}).status,
            Status::Stale);
}

TEST_F(SettingsServiceTest, ExplicitContextSupportOverridesRegistryHistory)
{
  auto local = std::make_shared<Config::Layer>(Config::LayerType::LocalGame);
  EXPECT_FALSE(service->SetContext({EditTarget::Game}));
  EXPECT_FALSE(service->SetContext({EditTarget::Base, local}));
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local}));
  EXPECT_EQ(Set("interface.start_big_picture", true).status, Status::UnsupportedContext);
  EXPECT_FALSE(Read("interface.start_big_picture").enabled);
  EXPECT_TRUE(local->GetLayerMap().empty());
}

TEST_F(SettingsServiceTest, DynamicOptionsPreserveUnknownValuesAndInvalidateRequests)
{
  ASSERT_EQ(Set("display.backend", std::string("D3D")).status, Status::Success);
  const auto old = service->ReadSnapshot();
  environment.graphics_backends = {{std::string("Vulkan"), "Vulkan"}};
  service->SetEnvironment(environment);
  EXPECT_EQ(
      service->Apply({old->revision, "display.backend", Operation::Set, std::string("D3D")}).status,
      Status::Stale);
  EXPECT_EQ(Read("display.backend").value, Value(std::string("D3D")));
  EXPECT_FALSE(Read("display.backend").value_available);
  EXPECT_EQ(Set("display.backend", std::string("D3D")).status, Status::InvalidValue);
  EXPECT_EQ(Set("display.backend", std::string("Vulkan")).status, Status::Success);
  EXPECT_TRUE(Read("display.backend").value_available);
  const auto previous = std::ranges::find_if(
      old->settings, [](const State& state) { return state.descriptor.id == "display.backend"; });
  ASSERT_NE(previous, old->settings.end());
  EXPECT_EQ(previous->descriptor.choices.size(), 2u);
}

TEST_F(SettingsServiceTest, InternalResolutionUsesStoredScaleAndRetainsUnavailableValues)
{
  EXPECT_EQ(Set("display.internal_resolution", 3).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::GFX_EFB_SCALE), 3);
  Config::SetBase(Config::GFX_EFB_SCALE, 16);
  EXPECT_EQ(Read("display.internal_resolution").value, Value(16));
  EXPECT_FALSE(Read("display.internal_resolution").value_available);
  environment.max_efb_scale = 16;
  service->SetEnvironment(environment);
  EXPECT_TRUE(Read("display.internal_resolution").value_available);
  EXPECT_EQ(Set("display.internal_resolution", 0).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::GFX_EFB_SCALE), 0);
}

TEST_F(SettingsServiceTest, ConfigChangesInvalidateRequestsAndPreservePublishedSnapshots)
{
  const auto old = service->ReadSnapshot();
  const auto value = Read("audio.volume").value;
  Config::SetBase(Config::MAIN_AUDIO_VOLUME, 23);
  EXPECT_EQ(service->Apply({old->revision, "audio.volume", Operation::Set, 77}).status,
            Status::Stale);
  const auto previous = std::ranges::find_if(
      old->settings, [](const State& state) { return state.descriptor.id == "audio.volume"; });
  ASSERT_NE(previous, old->settings.end());
  EXPECT_EQ(previous->value, value);
  EXPECT_EQ(Read("audio.volume").value, Value(23));
}

TEST_F(SettingsServiceTest, PoliciesAndDependenciesAreRecheckedAtMutationTime)
{
  EXPECT_FALSE(Read("advanced.cpu_clock").enabled);
  EXPECT_EQ(Display("advanced.cpu_clock", 150).status, Status::Unavailable);
  Set("advanced.overclock_enabled", true);
  EXPECT_TRUE(Read("advanced.cpu_clock").enabled);
  environment.emulating = true;
  service->SetEnvironment(environment);
  EXPECT_EQ(Set("display.backend", std::string("Vulkan")).status, Status::Unavailable);
  EXPECT_EQ(Set("audio.volume", 55).status, Status::Success);
  environment.netplay = true;
  service->SetEnvironment(environment);
  EXPECT_EQ(Set("advanced.overclock_enabled", false).status, Status::Unavailable);
  EXPECT_EQ(Display("advanced.cpu_clock", 150).status, Status::Unavailable);
  EXPECT_EQ(Set("audio.volume", 44).status, Status::Success);
  environment.netplay = false;
  environment.movie = true;
  service->SetEnvironment(environment);
  EXPECT_EQ(Set("advanced.overclock_enabled", false).status, Status::Unavailable);
  EXPECT_EQ(Set("audio.volume", 46).status, Status::Success);
  environment.movie = false;
  environment.hardcore = true;
  environment.post_processing = false;
  service->SetEnvironment(environment);
  EXPECT_EQ(Display("advanced.cpu_clock", 150).status, Status::Unavailable);
  EXPECT_EQ(Set("display.post_processing_preset", std::string("preset.slangp")).status,
            Status::Unavailable);
  environment.transitioning = true;
  service->SetEnvironment(environment);
  EXPECT_EQ(Set("audio.volume", 33).status, Status::Unavailable);
}

TEST_F(SettingsServiceTest, MemoryEditsRequireRamOverrideInTheSelectedContext)
{
  const u32 original = Config::Get(Config::MAIN_MEM1_SIZE);
  EXPECT_FALSE(Read("advanced.mem1_size").enabled);
  EXPECT_FALSE(Read("advanced.mem1_size").disabled_reason.empty());
  EXPECT_EQ(Set("advanced.mem1_size", u32(48 * 1048576)).status, Status::Unavailable);
  EXPECT_EQ(Display("advanced.mem1_size", 48).status, Status::Unavailable);
  EXPECT_EQ(Set("advanced.mem1_size", false, Operation::ResetDefault).status, Status::Unavailable);
  EXPECT_EQ(Config::Get(Config::MAIN_MEM1_SIZE), original);

  Config::SetBase(Config::MAIN_RAM_OVERRIDE_ENABLE, true);
  EXPECT_TRUE(Read("advanced.mem1_size").enabled);
  EXPECT_EQ(Display("advanced.mem1_size", 48).status, Status::Success);
  EXPECT_EQ(Config::Get(Config::MAIN_MEM1_SIZE), 48u * 1048576);
  const auto enabled = service->ReadSnapshot();
  Config::SetCurrent(Config::MAIN_RAM_OVERRIDE_ENABLE, false);
  EXPECT_EQ(
      service->Apply({enabled->revision, "advanced.mem1_size", Operation::Set, u32(32 * 1048576)})
          .status,
      Status::Stale);
  EXPECT_FALSE(Read("advanced.mem1_size").enabled);
  EXPECT_EQ(Display("advanced.mem1_size", 32).status, Status::Unavailable);
  EXPECT_EQ(Config::Get(Config::MAIN_MEM1_SIZE), 48u * 1048576);

  ASSERT_TRUE(service->SetContext({EditTarget::Base}));
  EXPECT_TRUE(Read("advanced.mem1_size").enabled);
  EXPECT_EQ(Display("advanced.mem1_size", 32).status, Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_MEM1_SIZE), 32u * 1048576);
  Config::SetBase(Config::MAIN_RAM_OVERRIDE_ENABLE, false);
  Config::SetCurrent(Config::MAIN_RAM_OVERRIDE_ENABLE, true);
  EXPECT_FALSE(Read("advanced.mem1_size").enabled);
  EXPECT_EQ(Display("advanced.mem1_size", 48).status, Status::Unavailable);
  EXPECT_EQ(Config::GetBase(Config::MAIN_MEM1_SIZE), 32u * 1048576);
  ASSERT_TRUE(service->SetContext({EditTarget::Effective}));
  EXPECT_TRUE(Read("advanced.mem1_size").enabled);

  environment.emulating = true;
  service->SetEnvironment(environment);
  EXPECT_FALSE(Read("advanced.mem1_size").enabled);
  EXPECT_EQ(Display("advanced.mem1_size", 48).status, Status::Unavailable);
}

TEST_F(SettingsServiceTest, PersistenceWritesOnlyTheSelectedLayer)
{
  ASSERT_TRUE(service->SetContext({EditTarget::Base}));
  Set("display.vsync", true);
  ASSERT_TRUE(service->Save());
  const auto base = *saved;
  auto game_saved = std::make_shared<Config::LayerMap>();
  auto local = std::make_shared<Config::Layer>(
      std::make_unique<MemoryLoader>(Config::LayerType::LocalGame, game_saved));
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local}));
  Set("display.vsync", false);
  ASSERT_TRUE(service->Save());
  EXPECT_EQ(*saved, base);
  Config::Layer reloaded(std::make_unique<MemoryLoader>(Config::LayerType::LocalGame, game_saved));
  reloaded.Load();
  EXPECT_TRUE(reloaded.Exists(Config::GFX_VSYNC.GetLocation()));
  EXPECT_FALSE(reloaded.Get(Config::GFX_VSYNC));
}

TEST_F(SettingsServiceTest, WrongThreadCannotReadMutateOrChangeContext)
{
  const Request request{service->ReadSnapshot()->revision, "audio.volume", Operation::Set, 17};
  std::thread worker([&] {
    EXPECT_EQ(service->Apply(request).status, Status::WrongThread);
    EXPECT_EQ(service->ReadSnapshot(), nullptr);
    EXPECT_FALSE(service->SetContext({EditTarget::Base}));
    EXPECT_FALSE(service->Save());
  });
  worker.join();
  EXPECT_NE(Config::Get(Config::MAIN_AUDIO_VOLUME), 17);
}
}  // namespace
