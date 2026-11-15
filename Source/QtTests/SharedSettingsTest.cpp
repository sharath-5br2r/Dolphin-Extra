// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <thread>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <gtest/gtest.h>

#include "Common/ScopeGuard.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "DolphinQt/BigPictureHost.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"
#include "DolphinQt/Config/Binder/SharedSettings.h"
#include "NullConfigLoader.h"
#include "UICommon/Settings/Service.h"
#include "VideoCommon/VideoConfig.h"

#include "ui_AdvancedPane.h"
#include "ui_GeneralWidget.h"

namespace
{
using namespace UICommon::Settings;

TEST(SharedSettingsDeathTest, UnsupportedCatalogBindingsFailFast)
{
  const Config::Location location{Config::System::Main, "SharedSettingsDeathTest", "Missing"};
  EXPECT_DEATH_IF_SUPPORTED(ConfigWidget::CatalogDescriptor(location), "");
}

class SharedSettingsTest : public testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
    // Settings normally connects this callback to the broadcaster. This test binary
    // links the binder independently of the application's Settings singleton.
    callback = Config::AddConfigChangedCallback(
        [] { ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast(); });
    ConfigWidget::ConfigSettingRegistry::Instance().ClearForTesting();
    service = std::make_unique<Service>();
  }
  void TearDown() override
  {
    QCoreApplication::processEvents();
    service.reset();
    Config::RemoveConfigChangedCallback(callback);
    Config::Shutdown();
  }
  Result Set(std::string id, Value value, Operation operation = Operation::Set)
  {
    auto result =
        service->Apply({service->ReadSnapshot()->revision, std::move(id), operation, value});
    QCoreApplication::processEvents();
    return result;
  }
  State Read(std::string_view id)
  {
    const auto snapshot = service->ReadSnapshot();
    return *std::ranges::find(snapshot->settings, id,
                              [](const State& s) { return s.descriptor.id; });
  }
  std::unique_ptr<Service> service;
  Config::ConfigChangedCallbackID callback;
};

TEST_F(SharedSettingsTest, ColdSnapshotDoesNotPopulateOrDependOnWidgetRegistry)
{
  const auto translated = ConfigWidget::TranslateSettingsSnapshot(*service->ReadSnapshot());
  EXPECT_EQ(translated->settings.size(), service->ReadSnapshot()->settings.size());
  EXPECT_GE(translated->settings.size(), 180u);
  EXPECT_TRUE(ConfigWidget::ConfigSettingRegistry::Instance().Entries().empty());
  EXPECT_FALSE(translated->settings.front().descriptor.label.empty());
}

TEST_F(SharedSettingsTest, DesignerAspectChoicesMatchCanonicalTypedValues)
{
  QWidget parent;
  Ui::GeneralWidget ui;
  ui.setupUi(&parent);
  const auto descriptor = ConfigWidget::CatalogDescriptor(Config::GFX_ASPECT_RATIO.GetLocation());
  ASSERT_EQ(ui.aspectRatioComboBox->count(), descriptor.choices.size());
  for (int i = 0; i < ui.aspectRatioComboBox->count(); ++i)
    EXPECT_EQ(ui.aspectRatioComboBox->itemText(i),
              QString::fromStdString(descriptor.choices[i].label));
  ConfigWidget::BindCatalog(ui.aspectRatioComboBox, Config::GFX_ASPECT_RATIO);
  ui.aspectRatioComboBox->setCurrentIndex(2);
  EXPECT_EQ(Read("display.aspect_ratio").value, Value(int(AspectMode::ForceStandard)));
  EXPECT_EQ(Set("display.aspect_ratio", int(AspectMode::ForceWide)).status, Status::Success);
  EXPECT_EQ(ui.aspectRatioComboBox->currentIndex(), 1);
  const auto* recorded = ConfigWidget::ConfigSettingRegistry::Instance().Find(descriptor.location);
  ASSERT_NE(recorded, nullptr);
  ASSERT_TRUE(recorded->shared_descriptor);
  EXPECT_TRUE(recorded->shared_descriptor->per_game);
  EXPECT_FALSE(recorded->per_game);  // A global widget binding does not define context support.
  EXPECT_EQ(recorded->choice_values[2], Value(int(AspectMode::ForceStandard)));
  Config::SetBase(Config::GFX_ASPECT_RATIO, AspectMode::Raw);
  QCoreApplication::processEvents();
  EXPECT_EQ(ui.aspectRatioComboBox->currentIndex(), -1);
  EXPECT_EQ(Config::Get(Config::GFX_ASPECT_RATIO), AspectMode::Raw);
}

TEST_F(SharedSettingsTest, TickAndReverseControlsRoundTripThroughTheService)
{
  QSlider ticks;
  ticks.setRange(0, 2);
  ConfigWidget::BindCatalog(&ticks, Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
  ticks.setValue(1);
  EXPECT_EQ(Read("graphics.texture_cache").value, Value(512));
  EXPECT_EQ(Set("graphics.texture_cache", 128).status, Status::Success);
  EXPECT_EQ(ticks.value(), 2);
  QCheckBox skip;
  ConfigWidget::BindCatalog(&skip, Config::GFX_HACK_EFB_ACCESS_ENABLE);
  skip.setChecked(true);
  EXPECT_FALSE(Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE));
  EXPECT_EQ(Read("graphics.skip_efb_access").display_value, Value(true));
  EXPECT_EQ(Set("graphics.skip_efb_access", false, Operation::SetDisplay).status, Status::Success);
  EXPECT_FALSE(skip.isChecked());
}

TEST_F(SharedSettingsTest, DesignerFloatAndScaledRangesMatchCanonicalStorage)
{
  QWidget parent;
  Ui::AdvancedPane ui;
  ui.setupUi(&parent);
  const auto clock = ConfigWidget::BindCatalogFloat(ui.cpuClockSlider, Config::MAIN_OVERCLOCK);
  EXPECT_EQ(ui.cpuClockSlider->minimum(), 0);
  EXPECT_EQ(ui.cpuClockSlider->maximum(), clock.range.MaximumPosition());
  ConfigWidget::BindCatalogScaled(ui.mem1Slider, Config::MAIN_MEM1_SIZE);
  const auto memory = ConfigWidget::CatalogDescriptor(Config::MAIN_MEM1_SIZE.GetLocation());
  EXPECT_DOUBLE_EQ(ui.mem1Slider->minimum(), memory.range->minimum * memory.range->scale);
  EXPECT_DOUBLE_EQ(ui.mem1Slider->maximum(), memory.range->maximum * memory.range->scale);
  ui.cpuClockSlider->setValue(124);
  EXPECT_FLOAT_EQ(std::get<float>(Read("advanced.cpu_clock").value), 1.25f);
  ui.mem1Slider->setValue(48);
  EXPECT_EQ(Read("advanced.mem1_size").value, Value(u32(48 * 1048576)));
  Set("advanced.overclock_enabled", true);
  Request request{service->ReadSnapshot()->revision, "advanced.cpu_clock", Operation::SetDisplay};
  request.display_value = 150;
  EXPECT_EQ(service->Apply(request).status, Status::Success);
  QCoreApplication::processEvents();
  EXPECT_EQ(ui.cpuClockSlider->value(), 149);
  Config::SetBase(Config::MAIN_RAM_OVERRIDE_ENABLE, true);
  EXPECT_EQ(Set("advanced.mem1_size", u32(32 * 1048576)).status, Status::Success);
  EXPECT_EQ(ui.mem1Slider->value(), 32);
}

TEST_F(SharedSettingsTest, ShippedFallbackAndOverrideClearingStayInSyncWithQt)
{
  Config::SetBase(Config::GFX_VSYNC, true);
  auto local = std::make_shared<Config::Layer>(Config::LayerType::LocalGame);
  auto shipped = std::make_shared<Config::Layer>(Config::LayerType::GlobalGame);
  shipped->Set(Config::GFX_VSYNC, false);
  ConfigWidget::Logic::SetFallbackLayer(local.get(), shipped.get());
  Common::ScopeGuard clear([&] { ConfigWidget::Logic::SetFallbackLayer(local.get(), nullptr); });
  QCheckBox box;
  ConfigWidget::BindCatalog(&box, Config::GFX_VSYNC, local.get());
  ASSERT_TRUE(service->SetContext({EditTarget::Game, local, shipped}));
  EXPECT_EQ(box.checkState(), Qt::PartiallyChecked);
  EXPECT_FALSE(ConfigWidget::EffectiveChecked(&box));
  EXPECT_EQ(Read("display.vsync").value, Value(false));
  EXPECT_EQ(Set("display.vsync", true).status, Status::Success);
  EXPECT_EQ(box.checkState(), Qt::Checked);
  EXPECT_EQ(Set("display.vsync", false, Operation::ClearOverride).status, Status::Success);
  EXPECT_EQ(box.checkState(), Qt::PartiallyChecked);
  EXPECT_FALSE(ConfigWidget::EffectiveChecked(&box));
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
}

TEST_F(SharedSettingsTest, LegacyMappedRegistryKeepsValuesAndExactUnorderedTicks)
{
  const Config::Info<int> choice{{Config::System::Main, "SharedSettingsTest", "Choice"}, 20};
  QComboBox box;
  box.addItems({QStringLiteral("Ten"), QStringLiteral("Twenty"), QStringLiteral("Forty")});
  const int values[] = {10, 20, 40};
  ConfigWidget::BindMapped(&box, choice, std::span<const int>{values});
  const auto* entry = ConfigWidget::ConfigSettingRegistry::Instance().Find(choice.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->choice_values, (std::vector<Value>{10, 20, 40}));
  QSlider slider;
  slider.setRange(0, 2);
  const int ticks[] = {0, 512, 128};
  ConfigWidget::BindMapped(&slider, Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, ticks);
  entry = ConfigWidget::ConfigSettingRegistry::Instance().Find(
      Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES.GetLocation());
  EXPECT_EQ(entry->ticks, (std::vector<Value>{0, 512, 128}));
  EXPECT_EQ(entry->maximum, 512);
}

TEST_F(SharedSettingsTest, LargeMappedValuesNeverPassThroughFloatOrNarrowIntegerStorage)
{
  constexpr u64 large = u64{1} << 54;
  const Config::Info<u64> setting{{Config::System::Main, "SharedSettingsTest", "Large"}, large};
  QComboBox box;
  box.addItems({QStringLiteral("Large"), QStringLiteral("Large plus one")});
  const u64 values[] = {large, large + 1};
  ConfigWidget::BindMapped(&box, setting, std::span<const u64>{values});
  box.setCurrentIndex(1);
  EXPECT_EQ(Config::Get(setting), large + 1);
  const auto* entry = ConfigWidget::ConfigSettingRegistry::Instance().Find(setting.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(std::get<u64>(entry->choice_values[1]), large + 1);
}

TEST_F(SharedSettingsTest, HostQueuesTypedRequestsAndRejectsStaleOrWrongScreenEdits)
{
  BigPicture::HostState state;
  state.lifecycle = BigPicture::Lifecycle::Idle;
  BigPictureHost host(
      nullptr,
      [&] {
        state.settings = service->ReadSnapshot();
        return state;
      },
      [&](const BigPicture::Command& command) -> BigPicture::Result {
        if (command.action != BigPicture::Action::EditSetting || !command.setting)
          return {command};
        const auto result = service->Apply(*command.setting);
        return {command, result.status == Status::Success ? BigPicture::ResultStatus::Succeeded :
                                                            BigPicture::ResultStatus::Stale};
      });
  host.RequestAction(host.GetSnapshot()->generation, BigPicture::Action::OpenMenu);
  QCoreApplication::processEvents();
  Request request{host.GetSnapshot()->host.settings->revision, "audio.volume", Operation::Set, 31};
  host.RequestSetting(host.GetSnapshot()->generation, request);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->result->status, BigPicture::ResultStatus::Unavailable);
  host.RequestAction(host.GetSnapshot()->generation, BigPicture::Action::Settings);
  QCoreApplication::processEvents();
  request.revision = host.GetSnapshot()->host.settings->revision;
  const auto generation = host.GetSnapshot()->generation;
  std::thread worker([&] { host.RequestSetting(generation, request); });
  worker.join();
  EXPECT_NE(Config::Get(Config::MAIN_AUDIO_VOLUME), 31);
  QCoreApplication::processEvents();
  EXPECT_EQ(Config::Get(Config::MAIN_AUDIO_VOLUME), 31);
  EXPECT_EQ(host.GetSnapshot()->result->status, BigPicture::ResultStatus::Succeeded);
  host.RequestSetting(generation, request);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->result->status, BigPicture::ResultStatus::Stale);
  EXPECT_TRUE(ConfigWidget::ConfigSettingRegistry::Instance().Entries().empty());
}
}  // namespace
