// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <thread>

#include <QApplication>
#include <QDir>
#include <QSlider>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/ScopeGuard.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt/BigPictureHost.h"
#include "DolphinQt/BigPictureSettings.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/SharedSettings.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/InputConfig.h"
#include "NullConfigLoader.h"
#include "UICommon/UICommon.h"

namespace
{
using namespace Common::Settings;
using O = BigPicture::SettingsOperation;
class TestController final : public ControllerEmu::EmulatedController
{
public:
  explicit TestController(InputConfig* config) : m_config(config) {}
  std::string GetName() const override { return "GCPad1"; }
  InputConfig* GetConfig() const override { return m_config; }

private:
  InputConfig* m_config;
};
class GameLoader final : public Config::ConfigLayerLoader
{
public:
  explicit GameLoader(Config::LayerType type) : ConfigLayerLoader(type) {}
  void Load(Config::Layer*) override {}
  void Save(Config::Layer*) override {}
};
class BigPictureSettingsTest : public testing::Test
{
protected:
  void SetUp() override
  {
    original_user = File::GetUserPath(D_USER_IDX);
    UICommon::SetUserDirectory(temp.path().toStdString());
    QDir().mkpath(QString::fromStdString(File::GetUserPath(D_CONFIG_IDX)));
    QDir().mkpath(QString::fromStdString(File::GetUserPath(D_GAMESETTINGS_IDX)));
    Config::Init();
    Config::AddLayer(std::make_unique<NullLoader>());
    SConfig::Init();
    input = std::make_unique<InputConfig>("SettingsTest", "Test", "SettingsTest", "Pad");
    input->CreateController<TestController>(input.get());
    input->GetController(0)->SetDefaultDevice("Test/0/Original");
    input->SaveConfig();
    menu = std::make_unique<BigPictureSettings>(nullptr, std::function<void()>{},
                                                std::vector<BigPictureSettings::ControllerTarget>{
                                                    {"pad0", "Test controller", input.get(), 0}},
                                                [this] { return devices; });
    environment.graphics_backends = {{std::string("D3D"), "Direct3D 11"},
                                     {std::string("Vulkan"), "Vulkan"}};
    environment.audio_backends = {{std::string("Null"), "No audio"}};
    environment.post_processing = true;
    menu->SetEnvironment(environment);
    menu->SetGame({"RTEST1", 0, "Test game", false});
    menu->Open();
  }
  void TearDown() override
  {
    menu.reset();
    input.reset();
    SConfig::Shutdown();
    Config::Shutdown();
    UICommon::SetUserDirectory(original_user);
    QCoreApplication::processEvents();
  }
  BigPicture::SettingsRow Row(const std::string& id)
  {
    const auto view = menu->ReadMenu();
    for (const auto* rows : {&view->rows, &view->toolbar})
      for (const auto& row : *rows)
        if (row.id == id)
          return row;
    ADD_FAILURE() << "Missing settings row " << id;
    return {};
  }
  Status Activate(const std::string& id)
  {
    const auto row = Row(id);
    if (row.navigation)
      return menu->Execute(*row.navigation);
    if (row.edit)
      return menu->Apply(*row.edit).status;
    ADD_FAILURE() << "Row has no request: " << id;
    return Status::InvalidValue;
  }
  void Back()
  {
    const auto request = menu->ReadMenu()->back;
    ASSERT_TRUE(request);
    ASSERT_EQ(menu->Execute(*request), Status::Success);
  }
  std::string MakeProfile(const std::string& name, const std::string& device)
  {
    const auto path = input->GetUserProfileDirectoryPath() + name + ".ini";
    File::CreateFullPath(path);
    Common::IniFile file;
    file.GetOrCreateSection("Profile")->Set("Device", device);
    EXPECT_TRUE(file.Save(path));
    return path;
  }
  QTemporaryDir temp;
  std::string original_user;
  UICommon::Settings::Environment environment;
  std::unique_ptr<InputConfig> input;
  std::unique_ptr<BigPictureSettings> menu;
  std::vector<std::string> devices;
};

TEST_F(BigPictureSettingsTest, ColdPagesAndQueuedValueEditsDoNotConstructDesktopDialogs)
{
  const auto widgets = QApplication::allWidgets().size();
  const auto retained = menu->ReadMenu();
  EXPECT_TRUE(Row("game").enabled);
  ASSERT_EQ(Activate("Audio"), Status::Success);
  ASSERT_EQ(Activate("audio.volume"), Status::Success);
  ASSERT_EQ(Activate("volume-37"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_AUDIO_VOLUME), 37);
  EXPECT_NE(retained->id, menu->ReadMenu()->id);
  Back();
  Back();
  ASSERT_EQ(Activate("Interface"), Status::Success);
  EXPECT_TRUE(menu->ReadMenu()->preferences);
  EXPECT_TRUE(Row("interface.start_big_picture").enabled);
  EXPECT_EQ(QApplication::allWidgets().size(), widgets);
}

TEST_F(BigPictureSettingsTest, ThemeMenuAppliesTypedChoicesWithoutLeavingThePage)
{
  ASSERT_EQ(Activate("Interface"), Status::Success);
  ASSERT_EQ(Activate("interface.big_picture_theme"), Status::Success);
  ASSERT_EQ(Activate("choice-1"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_BIG_PICTURE_THEME), "Light");
  EXPECT_EQ(menu->ReadMenu()->default_focus, "choice-1");
  EXPECT_TRUE(Row("choice-1").active);
  EXPECT_FALSE(Row("choice-0").active);
  ASSERT_EQ(Activate("choice-2"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_BIG_PICTURE_THEME), "AMOLED");
  EXPECT_EQ(menu->ReadMenu()->default_focus, "choice-2");
  EXPECT_TRUE(Row("choice-2").active);
  EXPECT_FALSE(Row("choice-1").active);
  ASSERT_EQ(Activate("reset"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_BIG_PICTURE_THEME), "Dark");
}

TEST_F(BigPictureSettingsTest, CategoryNavigationRemainsAvailableAndHighlightsCurrentContext)
{
  ASSERT_EQ(Activate("Audio"), Status::Success);
  EXPECT_TRUE(Row("global").active);
  EXPECT_FALSE(Row("game").active);
  EXPECT_TRUE(Row("Audio").active);
  ASSERT_EQ(Activate("Display"), Status::Success);
  EXPECT_TRUE(Row("Display").active);
  EXPECT_FALSE(Row("Audio").active);
  ASSERT_EQ(Activate("game"), Status::Success);
  ASSERT_EQ(Activate("PostProcessing"), Status::Success);
  EXPECT_TRUE(Row("game").active);
  EXPECT_FALSE(Row("global").active);
  EXPECT_TRUE(Row("PostProcessing").active);
  ASSERT_EQ(Activate("controllers"), Status::Success);
  EXPECT_TRUE(Row("controllers").active);
  EXPECT_FALSE(Row("PostProcessing").active);
}

TEST_F(BigPictureSettingsTest, PerGameEditsPersistAndClearingRevealsGlobalWithoutChangingIt)
{
  Config::SetBase(Config::GFX_VSYNC, true);
  ASSERT_EQ(Activate("game"), Status::Success);
  ASSERT_EQ(Activate("Display"), Status::Success);
  EXPECT_NE(Row("display.vsync").detail.find("Inherited"), std::string::npos);
  ASSERT_EQ(Activate("display.vsync"), Status::Success);
  ASSERT_EQ(Activate("off"), Status::Success);
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
  Common::IniFile ini;
  const auto path = File::GetUserPath(D_GAMESETTINGS_IDX) + "RTEST1.ini";
  ASSERT_TRUE(ini.Load(path));
  bool value = true;
  ASSERT_TRUE(ini.GetOrCreateSection("Video_Hardware")->Get("VSync", &value));
  EXPECT_FALSE(value);
  EXPECT_TRUE(Row("inherit").enabled);
  ASSERT_EQ(Activate("inherit"), Status::Success);
  ASSERT_TRUE(ini.Load(path));
  EXPECT_FALSE(ini.Exists("Video_Hardware", "VSync"));
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
  EXPECT_FALSE(Row("inherit").enabled);
  menu->Open();
  ASSERT_EQ(Activate("game"), Status::Success);
  const auto snapshot = menu->ReadSnapshot();
  const auto state = std::ranges::find(snapshot->settings, std::string("display.vsync"),
                                       [](const State& s) { return s.descriptor.id; });
  ASSERT_NE(state, snapshot->settings.end());
  EXPECT_EQ(state->value, Value(true));
}

TEST_F(BigPictureSettingsTest, MenuEditsAndDesktopBindingsRoundTripInBothDirections)
{
  const auto callback = Config::AddConfigChangedCallback(
      [] { ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast(); });
  Common::ScopeGuard cleanup([&] { Config::RemoveConfigChangedCallback(callback); });
  QSlider desktop;
  desktop.setRange(0, 100);
  ConfigWidget::BindCatalog(&desktop, Config::MAIN_AUDIO_VOLUME);
  ASSERT_EQ(Activate("Audio"), Status::Success);
  ASSERT_EQ(Activate("audio.volume"), Status::Success);
  ASSERT_EQ(Activate("volume-37"), Status::Success);
  QCoreApplication::processEvents();
  EXPECT_EQ(desktop.value(), 37);
  const auto old = *Row("volume-45").edit;
  desktop.setValue(48);
  QCoreApplication::processEvents();
  EXPECT_EQ(menu->ReadMenu()->default_focus, "volume-48");
  EXPECT_EQ(menu->Apply(old).status, Status::Stale);
  EXPECT_EQ(Config::GetBase(Config::MAIN_AUDIO_VOLUME), 48);
}

TEST_F(BigPictureSettingsTest, UnsupportedGameEditsAndStaleNavigationAreRejected)
{
  const auto old = *Row("Audio").navigation;
  ASSERT_EQ(Activate("game"), Status::Success);
  EXPECT_EQ(menu->Execute(old), Status::Stale);
  ASSERT_EQ(Activate("Audio"), Status::Success);
  ASSERT_EQ(Activate("audio.volume"), Status::Success);
  EXPECT_FALSE(Row("volume-45").enabled);
  EXPECT_EQ(Activate("volume-45"), Status::UnsupportedContext);
  Back();
  Back();
  const auto game_request = *Row("game").navigation;
  menu->SetGame({});
  EXPECT_EQ(menu->Execute(game_request), Status::Stale);
  EXPECT_FALSE(Row("game").enabled);
}

TEST_F(BigPictureSettingsTest, UnknownDynamicBackendRemainsStoredAndRuntimePoliciesBlockChanges)
{
  Config::SetBase(Config::MAIN_GFX_BACKEND, std::string("Missing"));
  ASSERT_EQ(Activate("Display"), Status::Success);
  EXPECT_NE(Row("display.backend").detail.find("Missing"), std::string::npos);
  ASSERT_EQ(Activate("display.backend"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_GFX_BACKEND), "Missing");
  environment.emulating = true;
  menu->SetEnvironment(environment);
  EXPECT_FALSE(Row("choice-0").enabled);
  EXPECT_EQ(Activate("choice-0"), Status::Unavailable);
  EXPECT_EQ(Config::GetBase(Config::MAIN_GFX_BACKEND), "Missing");
}

TEST_F(BigPictureSettingsTest, GlobalProfilesUseExistingFilesWithoutRewritingThem)
{
  const auto path = MakeProfile("Nested/Test", "Test/0/Profile");
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  ASSERT_EQ(Activate("profiles"), Status::Success);
  ASSERT_EQ(Activate("profile-" + path), Status::Success);
  EXPECT_EQ(input->GetController(0)->GetDefaultDevice().ToString(), "Test/0/Profile");
  Common::IniFile ini;
  ASSERT_TRUE(ini.Load(File::GetUserPath(D_CONFIG_IDX) + "SettingsTest.ini"));
  std::string device;
  ASSERT_TRUE(ini.GetOrCreateSection("GCPad1")->Get("Device", &device));
  EXPECT_EQ(device, "Test/0/Profile");
  ASSERT_TRUE(ini.Load(path));
  EXPECT_TRUE(ini.Exists("Profile", "Device"));
}

TEST_F(BigPictureSettingsTest, GameProfilesSaveReferencesAndClearWithoutTouchingGlobalMapping)
{
  const auto path = MakeProfile("Nested/Test", "Test/0/Profile");
  const auto comma = MakeProfile("Comma,Test", "Test/0/Other");
  const auto whitespace = MakeProfile(" Space ", "Test/0/Other");
  ASSERT_EQ(Activate("game"), Status::Success);
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  EXPECT_FALSE(Row("devices").enabled);
  ASSERT_EQ(Activate("profiles"), Status::Success);
  const auto rows = menu->ReadMenu()->rows;
  EXPECT_FALSE(std::ranges::any_of(rows, [&](const auto& row) {
    return row.navigation &&
           (row.navigation->value == comma || row.navigation->value == whitespace);
  }));
  ASSERT_EQ(Activate("profile-" + path), Status::Success);
  EXPECT_EQ(input->GetController(0)->GetDefaultDevice().ToString(), "Test/0/Original");
  Common::IniFile ini;
  const auto game_ini = File::GetUserPath(D_GAMESETTINGS_IDX) + "RTEST1.ini";
  ASSERT_TRUE(ini.Load(game_ini));
  std::string profile;
  ASSERT_TRUE(ini.GetOrCreateSection("Controls")->Get("PadProfile1", &profile));
  EXPECT_EQ(profile, "Nested/Test");
  ASSERT_EQ(Activate("clear-profile"), Status::Success);
  ASSERT_TRUE(ini.Load(game_ini));
  EXPECT_FALSE(ini.Exists("Controls", "PadProfile1"));
  EXPECT_EQ(input->GetController(0)->GetDefaultDevice().ToString(), "Test/0/Original");
}

TEST_F(BigPictureSettingsTest, DeviceSelectionPersistsAndDisconnectRejectsOldRequests)
{
  devices = {"Test/0/Replacement"};
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  ASSERT_EQ(Activate("devices"), Status::Success);
  ASSERT_EQ(Activate("device-Test/0/Replacement"), Status::Success);
  EXPECT_EQ(input->GetController(0)->GetDefaultDevice().ToString(), "Test/0/Replacement");
  Common::IniFile ini;
  ASSERT_TRUE(ini.Load(File::GetUserPath(D_CONFIG_IDX) + "SettingsTest.ini"));
  std::string device;
  ASSERT_TRUE(ini.GetOrCreateSection("GCPad1")->Get("Device", &device));
  EXPECT_EQ(device, "Test/0/Replacement");
  ASSERT_EQ(Activate("devices"), Status::Success);
  const auto request = *Row("device-Test/0/Replacement").navigation;
  devices.clear();
  EXPECT_EQ(menu->Execute(request), Status::Stale);
  EXPECT_NE(menu->ReadMenu()->description.find("disconnected"), std::string::npos);
  EXPECT_EQ(input->GetController(0)->GetDefaultDevice().ToString(), "Test/0/Replacement");
}

TEST_F(BigPictureSettingsTest, ActiveGameEditsUseTheActiveLayerAndShippedFallback)
{
  Config::AddLayer(std::make_unique<GameLoader>(Config::LayerType::GlobalGame));
  Config::AddLayer(std::make_unique<GameLoader>(Config::LayerType::LocalGame));
  Config::SetBase(Config::GFX_VSYNC, false);
  Config::GetLayer(Config::LayerType::GlobalGame)->Set(Config::GFX_VSYNC, true);
  Config::OnConfigChanged();
  menu->SetGame({"RTEST1", 0, "Test game", true});
  environment.emulating = true;
  menu->SetEnvironment(environment);
  ASSERT_EQ(Activate("game"), Status::Success);
  ASSERT_EQ(Activate("Display"), Status::Success);
  ASSERT_EQ(Activate("display.vsync"), Status::Success);
  ASSERT_EQ(Activate("off"), Status::Success);
  EXPECT_FALSE(Config::Get(Config::GFX_VSYNC));
  EXPECT_FALSE(Config::GetBase(Config::GFX_VSYNC));
  ASSERT_EQ(Activate("inherit"), Status::Success);
  EXPECT_TRUE(Config::Get(Config::GFX_VSYNC));
  EXPECT_FALSE(
      Config::GetLayer(Config::LayerType::LocalGame)->Exists(Config::GFX_VSYNC.GetLocation()));
  EXPECT_TRUE(Config::GetLayer(Config::LayerType::GlobalGame)->Get(Config::GFX_VSYNC));
}

TEST_F(BigPictureSettingsTest, MissingProfilesAndUnsafeControllerChangesNeverReplaceMappings)
{
  const auto path = MakeProfile("Test", "Test/0/Profile");
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  ASSERT_EQ(Activate("profiles"), Status::Success);
  const auto request = *Row("profile-" + path).navigation;
  ASSERT_TRUE(File::Delete(path));
  EXPECT_EQ(menu->Execute(request), Status::Unavailable);
  EXPECT_EQ(input->GetController(0)->GetDefaultDevice().ToString(), "Test/0/Original");
  Back();
  environment.movie = true;
  menu->SetEnvironment(environment);
  EXPECT_FALSE(Row("profiles").enabled);
  environment.movie = false;
  environment.netplay = true;
  menu->SetEnvironment(environment);
  EXPECT_FALSE(Row("devices").enabled);
  environment.netplay = false;
  environment.emulating = true;
  menu->SetEnvironment(environment);
  EXPECT_FALSE(Row("profiles").enabled);
}

TEST_F(BigPictureSettingsTest, PresetDiscoveryIsAsyncAndDisabledCapabilityPreservesThePreset)
{
  Config::SetBase(Config::GFX_ENHANCE_POST_SHADER, std::string("missing/preset.slangp"));
  ASSERT_EQ(Activate("PostProcessing"), Status::Success);
  ASSERT_EQ(Activate("display.post_processing_preset"), Status::Success);
  environment.post_processing = false;
  menu->SetEnvironment(environment);
  EXPECT_FALSE(Row("presets").enabled);
  EXPECT_EQ(Activate("off"), Status::Unavailable);
  EXPECT_EQ(Config::GetBase(Config::GFX_ENHANCE_POST_SHADER), "missing/preset.slangp");
  environment.post_processing = true;
  menu->SetEnvironment(environment);
  ASSERT_EQ(Activate("presets"), Status::Success);
  const auto loading = menu->ReadMenu();
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (menu->ReadMenu()->revision == loading->revision &&
         std::chrono::steady_clock::now() < until)
  {
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_NE(menu->ReadMenu()->revision, loading->revision);
  Back();
  ASSERT_EQ(Activate("off"), Status::Success);
  EXPECT_TRUE(Config::GetBase(Config::GFX_ENHANCE_POST_SHADER).empty());
}

TEST_F(BigPictureSettingsTest, MenuRequestsUseTheHostQueueAndRejectWrongScreensAndStaleGenerations)
{
  BigPicture::HostState state;
  state.lifecycle = BigPicture::Lifecycle::Idle;
  state.settings_menu = menu->ReadMenu();
  BigPictureHost host(
      nullptr,
      [&] {
        state.settings_menu = menu->ReadMenu();
        return state;
      },
      [&](const BigPicture::Command& command) {
        if (command.action == BigPicture::Action::Settings)
          menu->Open();
        else if (command.settings_menu)
          EXPECT_EQ(menu->Execute(*command.settings_menu), Status::Success);
        return BigPicture::Result{command, BigPicture::ResultStatus::Succeeded};
      });
  auto snapshot = host.GetSnapshot();
  const auto request = *Row("Audio").navigation;
  host.RequestSettingsMenu(snapshot->generation, request);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->result->status, BigPicture::ResultStatus::Unavailable);
  host.RequestAction(snapshot->generation, BigPicture::Action::OpenMenu);
  QCoreApplication::processEvents();
  host.RequestAction(host.GetSnapshot()->generation, BigPicture::Action::Settings);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->screens.back(), BigPicture::Screen::Settings);
  const auto current = *Row("Audio").navigation;
  std::thread worker([&] { host.RequestSettingsMenu(host.GetSnapshot()->generation, current); });
  worker.join();
  EXPECT_EQ(menu->ReadMenu()->title, "Settings");
  QCoreApplication::processEvents();
  EXPECT_EQ(menu->ReadMenu()->title, "Audio");
  host.RequestSettingsMenu(0, current);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->result->status, BigPicture::ResultStatus::Stale);
}
}  // namespace
