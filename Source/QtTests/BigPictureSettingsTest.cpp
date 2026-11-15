// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <thread>

#include <QApplication>
#include <QDir>
#include <QSlider>
#include <QTemporaryDir>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
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
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/InputConfig.h"
#include "NullConfigLoader.h"
#include "UICommon/UICommon.h"

namespace
{
using namespace Common::Settings;
using O = BigPicture::SettingsOperation;
class TestAttachment final : public ControllerEmu::AttachedController
{
public:
  explicit TestAttachment(std::string name) : m_name(std::move(name))
  {
    if (m_name == "None")
      return;
    auto* buttons = new ControllerEmu::Buttons("Buttons");
    groups.emplace_back(buttons);
    buttons->AddInput(ControllerEmu::Translatability::DoNotTranslate, "C");
  }
  std::string GetName() const override { return m_name; }

private:
  std::string m_name;
};
class TestController final : public ControllerEmu::EmulatedController
{
public:
  explicit TestController(InputConfig* config) : m_config(config)
  {
    auto* buttons = new ControllerEmu::Buttons("Buttons");
    groups.emplace_back(buttons);
    buttons->AddInput(ControllerEmu::Translatability::DoNotTranslate, "A");
    buttons->AddInput(ControllerEmu::Translatability::DoNotTranslate, "B");
    buttons->AddSetting(&dead_zone, {"Dead Zone", "%"}, 10.0, 0.0, 50.0);
    buttons->AddSetting(&sideways, {"Sideways"}, false);
    auto* extension = new ControllerEmu::Attachments("Extension");
    groups.emplace_back(extension);
    extension->AddSetting(&attach_motion, {"Attach MotionPlus"}, true);
    extension->AddAttachment(std::make_unique<TestAttachment>("None"));
    extension->AddAttachment(std::make_unique<TestAttachment>("Nunchuk"));
  }
  std::string GetName() const override { return "GCPad1"; }
  InputConfig* GetConfig() const override { return m_config; }
  ControllerEmu::SettingValue<double> dead_zone;
  ControllerEmu::SettingValue<bool> sideways;
  ControllerEmu::SettingValue<bool> attach_motion;

private:
  InputConfig* m_config;
};
// The "Test/0/Original" device, with one button whose state the test controls.
class FakeDevice final : public ciface::Core::Device
{
public:
  class Button final : public Input
  {
  public:
    std::string GetName() const override { return "Fire"; }
    ControlState GetState() const override { return state; }
    ControlState state = 0;
  };
  FakeDevice()
  {
    SetId(0);
    AddInput(button = new Button);
  }
  std::string GetName() const override { return "Original"; }
  std::string GetSource() const override { return "Test"; }
  Button* button;
};
class FakeDevices final : public ciface::Core::DeviceContainer
{
public:
  FakeDevices() { m_devices.push_back(device); }
  std::shared_ptr<FakeDevice> device = std::make_shared<FakeDevice>();
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
  bool HasRow(const std::string& id)
  {
    const auto view = menu->ReadMenu();
    return std::ranges::any_of(view->rows, [&](const auto& row) { return row.id == id; }) ||
           std::ranges::any_of(view->toolbar, [&](const auto& row) { return row.id == id; });
  }
  Status Activate(const std::string& id)
  {
    if (id == "game" || id == "global")
      return menu->Open(id == "game") ? Status::Success : Status::Unavailable;
    const auto row = Row(id);
    if (row.navigation)
      return menu->Execute(*row.navigation);
    if (row.edit)
      return menu->Apply(*row.edit).status;
    ADD_FAILURE() << "Row has no request: " << id;
    return Status::InvalidValue;
  }
  // The value row's slider step ("decrease", "increase", "decrease-large", "increase-large").
  std::optional<BigPicture::SettingsAction> Step(const std::string& step)
  {
    const auto row = Row("value");
    if (!row.slider)
    {
      ADD_FAILURE() << "The value row has no slider";
      return std::nullopt;
    }
    const auto& slider = *row.slider;
    return step == "decrease"       ? slider.decrease :
           step == "increase"       ? slider.increase :
           step == "decrease-large" ? slider.decrease_large :
                                      slider.increase_large;
  }
  Status Run(const BigPicture::SettingsAction& action)
  {
    if (action.navigation)
      return menu->Execute(*action.navigation);
    if (action.edit)
      return menu->Apply(*action.edit).status;
    ADD_FAILURE() << "Slider action has no request";
    return Status::InvalidValue;
  }
  Status Slide(const std::string& step)
  {
    const auto action = Step(step);
    if (!action)
    {
      ADD_FAILURE() << "Slider step unavailable: " << step;
      return Status::InvalidValue;
    }
    return Run(*action);
  }
  // Sets the slider to an arbitrary value the way a pointer drag does.
  Status SlideTo(double value)
  {
    auto action = Row("value").slider->set;
    if (action.navigation)
      action.navigation->value = fmt::format("{}", value);
    if (action.edit)
      action.edit->display_value = value;
    return Run(action);
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
  EXPECT_FALSE(HasRow("game"));
  EXPECT_FALSE(HasRow("global"));
  ASSERT_EQ(Activate("Audio"), Status::Success);
  ASSERT_EQ(Activate("audio.volume"), Status::Success);
  // Volume uses the same slider as other ranged numbers, not a row per value.
  const auto slider = Row("value").slider;
  ASSERT_TRUE(slider);
  EXPECT_EQ(slider->minimum, 0);
  EXPECT_EQ(slider->maximum, 100);
  EXPECT_EQ(slider->value, 100);
  EXPECT_FALSE(slider->increase);
  EXPECT_EQ(menu->ReadMenu()->default_focus, "value");
  EXPECT_FALSE(HasRow("volume-37"));
  ASSERT_EQ(Slide("decrease-large"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_AUDIO_VOLUME), 90);
  EXPECT_NE(retained->id, menu->ReadMenu()->id);
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

TEST_F(BigPictureSettingsTest, NativeNumberEditorUsesDisplayUnitsAndValidatesTypedInput)
{
  ASSERT_EQ(Activate("General"), Status::Success);
  ASSERT_EQ(Activate("general.speed"), Status::Success);
  ASSERT_EQ(Slide("increase"), Status::Success);
  EXPECT_NEAR(Config::GetBase(Config::MAIN_EMULATION_SPEED), 1.1f, 0.00001f);
  ASSERT_EQ(Activate("enter-value"), Status::Success);
  EXPECT_TRUE(menu->ReadMenu()->text_input);
  ASSERT_EQ(Activate("text-clear"), Status::Success);
  const auto revision = menu->ReadMenu()->revision;
  ASSERT_EQ(menu->Execute({revision, O::TextKey, "125"}), Status::Success);
  ASSERT_EQ(Activate("text-done"), Status::Success);
  // The step is 10%, so a typed 125% must leave the editor open without changing storage.
  EXPECT_TRUE(menu->ReadMenu()->text_input);
  EXPECT_NEAR(Config::GetBase(Config::MAIN_EMULATION_SPEED), 1.1f, 0.00001f);
  ASSERT_EQ(Activate("text-clear"), Status::Success);
  ASSERT_EQ(menu->Execute({menu->ReadMenu()->revision, O::TextKey, "150"}), Status::Success);
  ASSERT_EQ(Activate("text-done"), Status::Success);
  EXPECT_FALSE(menu->ReadMenu()->text_input);
  EXPECT_FLOAT_EQ(Config::GetBase(Config::MAIN_EMULATION_SPEED), 1.5f);
  EXPECT_EQ(Step("increase-large")->edit->display_value, 200);
  // A pointer sets any value in display units.
  ASSERT_EQ(SlideTo(70), Status::Success);
  EXPECT_FLOAT_EQ(Config::GetBase(Config::MAIN_EMULATION_SPEED), 0.7f);
  EXPECT_EQ(Row("value").slider->value, 70);
}

TEST_F(BigPictureSettingsTest, ConvergenceEditorAcceptsFloatStepsAndTypedValuesAtTheUpperBound)
{
  environment.stereo = true;
  menu->SetEnvironment(environment);
  Config::SetBase(Config::GFX_STEREO_CONVERGENCE, 100.0f);
  ASSERT_EQ(Activate("Enhancements"), Status::Success);
  ASSERT_EQ(Activate("native.gfx_stereo_convergence"), Status::Success);
  ASSERT_EQ(Slide("increase"), Status::Success);
  EXPECT_FLOAT_EQ(Config::GetBase(Config::GFX_STEREO_CONVERGENCE), 100.01f);
  ASSERT_EQ(Activate("enter-value"), Status::Success);
  ASSERT_EQ(Activate("text-clear"), Status::Success);
  ASSERT_EQ(menu->Execute({menu->ReadMenu()->revision, O::TextKey, "199.99"}), Status::Success);
  ASSERT_EQ(Activate("text-done"), Status::Success);
  EXPECT_FALSE(menu->ReadMenu()->text_input);
  EXPECT_FLOAT_EQ(Config::GetBase(Config::GFX_STEREO_CONVERGENCE), 199.99f);
  ASSERT_EQ(Slide("increase"), Status::Success);
  EXPECT_FLOAT_EQ(Config::GetBase(Config::GFX_STEREO_CONVERGENCE), 200.0f);
  EXPECT_FALSE(Step("increase"));
  EXPECT_FALSE(Step("increase-large"));
}

TEST_F(BigPictureSettingsTest, TextEditorKeepsDraftsUntilDoneAndHandlesUnicodeAndQueuedCharacters)
{
  ASSERT_EQ(Activate("Paths"), Status::Success);
  ASSERT_EQ(Activate("native.main_default_iso"), Status::Success);
  ASSERT_EQ(Activate("enter-value"), Status::Success);
  ASSERT_EQ(Activate("text-clear"), Status::Success);
  const auto revision = menu->ReadMenu()->revision;
  ASSERT_EQ(menu->Execute({revision, O::TextKey, "C:/日本語/"}), Status::Success);
  ASSERT_EQ(menu->Execute({revision, O::TextKey, "🐬"}), Status::Success);
  ASSERT_EQ(Activate("text-erase"), Status::Success);
  EXPECT_EQ(menu->ReadMenu()->draft, "C:/日本語/");
  EXPECT_TRUE(Config::GetBase(Config::MAIN_DEFAULT_ISO).empty());
  ASSERT_EQ(Activate("text-key-0"), Status::Success);
  EXPECT_EQ(menu->ReadMenu()->draft, "C:/日本語/a");
  ASSERT_EQ(menu->Execute({revision, O::TextKey, std::string(4096, 'x')}), Status::InvalidValue);
  ASSERT_EQ(menu->Execute({revision, O::TextKey, std::string(1, '\xff')}), Status::Unavailable);
  ASSERT_EQ(Activate("text-done"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_DEFAULT_ISO), "C:/日本語/a");
  ASSERT_EQ(Activate("enter-value"), Status::Success);
  ASSERT_EQ(Activate("text-clear"), Status::Success);
  Back();
  EXPECT_EQ(Config::GetBase(Config::MAIN_DEFAULT_ISO), "C:/日本語/a");
}

TEST_F(BigPictureSettingsTest, PathSettingsBrowseForFoldersAndFilesWithoutTyping)
{
  const auto listed = [&](const std::string& id) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!HasRow(id) && std::chrono::steady_clock::now() < until)
    {
      QCoreApplication::processEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return HasRow(id);
  };
  QTemporaryDir root;
  ASSERT_TRUE(root.isValid());
  const auto base = root.path().toStdString();
  ASSERT_TRUE(QDir(root.path()).mkpath(QStringLiteral("dumps/inner")));
  {
    File::IOFile game(base + "/dumps/game.iso", "wb");
    ASSERT_TRUE(game.WriteString("disc"));
  }

  // A folder setting starts where its value lives and can choose the open folder.
  Config::SetBase(Config::MAIN_DUMP_PATH, base + "/dumps");
  ASSERT_EQ(Activate("Paths"), Status::Success);
  ASSERT_EQ(Activate("native.main_dump_path"), Status::Success);
  EXPECT_EQ(menu->ReadMenu()->default_focus, "browse");
  EXPECT_TRUE(HasRow("enter-value"));
  ASSERT_EQ(Activate("browse"), Status::Success);
  ASSERT_TRUE(listed("browse:" + base + "/dumps/inner"));
  // Folder pickers list folders only.
  EXPECT_FALSE(HasRow("browse:" + base + "/dumps/game.iso"));
  EXPECT_EQ(menu->ReadMenu()->default_focus, "browse-use");
  ASSERT_EQ(Activate("browse:" + base + "/dumps/inner"), Status::Success);
  ASSERT_TRUE(listed("browse-up"));
  EXPECT_EQ(Row("browse-up").navigation->value, base + "/dumps");
  ASSERT_EQ(Activate("browse-use"), Status::Success);
  EXPECT_EQ(Config::GetBase(Config::MAIN_DUMP_PATH),
            QDir::toNativeSeparators(QString::fromStdString(base + "/dumps/inner")).toStdString());
  EXPECT_TRUE(HasRow("browse"));

  // Only published entries can be chosen, and a file setting needs an existing file.
  Back();
  ASSERT_EQ(Activate("native.main_default_iso"), Status::Success);
  Config::SetBase(Config::MAIN_DEFAULT_ISO, base + "/dumps/missing.iso");
  ASSERT_EQ(Activate("browse"), Status::Success);
  const auto game = "browse:" + base + "/dumps/game.iso";
  ASSERT_TRUE(listed(game));
  EXPECT_FALSE(HasRow("browse-use"));
  EXPECT_EQ(menu->Execute({menu->ReadMenu()->revision, O::BrowseChoose, base + "/elsewhere.iso"}),
            Status::Unavailable);
  ASSERT_EQ(Activate(game), Status::Success);
  EXPECT_EQ(
      Config::GetBase(Config::MAIN_DEFAULT_ISO),
      QDir::toNativeSeparators(QString::fromStdString(base + "/dumps/game.iso")).toStdString());
}

TEST_F(BigPictureSettingsTest, MappingBindsByPressingAnInputAndClearsOrRestoresDefaults)
{
  FakeDevices detection;
  menu = std::make_unique<BigPictureSettings>(
      nullptr, std::function<void()>{},
      std::vector<BigPictureSettings::ControllerTarget>{
          {"pad0", "Test controller", input.get(), 0}},
      [this] { return devices; }, &detection);
  menu->SetEnvironment(environment);
  menu->SetGame({"RTEST1", 0, "Test game", false});
  menu->Open();
  const auto wait = [&](auto predicate) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate() && std::chrono::steady_clock::now() < until)
    {
      QCoreApplication::processEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
  };
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  ASSERT_EQ(Activate("mapping"), Status::Success);
  EXPECT_EQ(menu->ReadMenu()->default_focus, "map:Buttons/A");
  EXPECT_EQ(Row("map:Buttons/A").value, "Unbound");
  EXPECT_EQ(Row("map:Buttons/A").section, "Buttons");
  ASSERT_TRUE(Row("map:Buttons/A").options);

  // A press that is held when capture starts is ignored; a fresh press binds.
  const auto tick = [] {
    for (int i = 0; i < 4; ++i)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      QCoreApplication::processEvents();
    }
  };
  detection.device->button->state = 1;
  ASSERT_EQ(Activate("map:Buttons/A"), Status::Success);
  EXPECT_TRUE(menu->ReadMenu()->capturing);
  EXPECT_TRUE(menu->ReadMenu()->rows.empty());
  tick();
  detection.device->button->state = 0;
  tick();
  EXPECT_TRUE(menu->ReadMenu()->capturing);
  detection.device->button->state = 1;
  tick();
  detection.device->button->state = 0;
  ASSERT_TRUE(wait([&] { return !menu->ReadMenu()->capturing; }));
  EXPECT_EQ(Row("map:Buttons/A").value, "Fire");
  EXPECT_EQ(input->GetController(0)->groups[0]->controls[0]->control_ref->GetExpression(), "Fire");

  // Leaving the settings screen ends a capture without binding.
  ASSERT_EQ(Activate("map:Buttons/B"), Status::Success);
  EXPECT_TRUE(menu->ReadMenu()->capturing);
  menu->CancelCapture();
  EXPECT_FALSE(menu->ReadMenu()->capturing);
  EXPECT_EQ(Row("map:Buttons/B").value, "Unbound");

  ASSERT_EQ(menu->Execute(*Row("map:Buttons/A").options), Status::Success);
  EXPECT_EQ(menu->ReadMenu()->default_focus, "map-bind");
  ASSERT_EQ(Activate("map-clear"), Status::Success);
  EXPECT_EQ(Row("map:Buttons/A").value, "Unbound");

  ASSERT_EQ(Activate("map-defaults"), Status::Success);
  EXPECT_EQ(menu->ReadMenu()->default_focus, "map-defaults-cancel");
  ASSERT_EQ(Activate("map-defaults-cancel"), Status::Success);
  EXPECT_TRUE(HasRow("map:Buttons/A"));
  ASSERT_EQ(Activate("map-defaults"), Status::Success);
  ASSERT_EQ(Activate("map-defaults-confirm"), Status::Success);
  EXPECT_TRUE(HasRow("map:Buttons/A"));

  // Mapping is global; game settings select profiles instead.
  ASSERT_TRUE(menu->Open(true));
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  EXPECT_FALSE(HasRow("mapping"));
}

TEST_F(BigPictureSettingsTest, MappingTunesSettingsSwitchesExtensionsAndSavesProfiles)
{
  auto* controller = static_cast<TestController*>(input->GetController(0));
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  ASSERT_EQ(Activate("mapping"), Status::Success);
  EXPECT_EQ(Row("map-extension").value, "None");
  EXPECT_EQ(Row("set:Buttons/Dead Zone").value, "10 %");
  ASSERT_TRUE(Row("set:Buttons/Sideways").toggle);
  EXPECT_FALSE(*Row("set:Buttons/Sideways").toggle);

  ASSERT_EQ(Activate("set:Buttons/Sideways"), Status::Success);
  EXPECT_TRUE(controller->sideways.GetValue());
  EXPECT_TRUE(*Row("set:Buttons/Sideways").toggle);

  // Numbers use a slider, clamped to the setting's range.
  ASSERT_EQ(Activate("set:Buttons/Dead Zone"), Status::Success);
  ASSERT_TRUE(Row("value").slider);
  EXPECT_EQ(Row("value").slider->maximum_label, "50 %");
  ASSERT_EQ(Slide("increase"), Status::Success);
  EXPECT_DOUBLE_EQ(controller->dead_zone.GetValue(), 11);
  ASSERT_EQ(Slide("decrease-large"), Status::Success);
  EXPECT_DOUBLE_EQ(controller->dead_zone.GetValue(), 1);
  ASSERT_EQ(Slide("decrease"), Status::Success);
  EXPECT_DOUBLE_EQ(controller->dead_zone.GetValue(), 0);
  EXPECT_FALSE(Step("decrease"));
  ASSERT_EQ(SlideTo(42), Status::Success);
  EXPECT_DOUBLE_EQ(controller->dead_zone.GetValue(), 42);
  EXPECT_EQ(Row("value").slider->value, 42);
  // Only the published set operation runs, and only with a number.
  EXPECT_EQ(menu->Execute({menu->ReadMenu()->revision, O::MapSettingSet, "inf"}),
            Status::InvalidValue);
  EXPECT_EQ(menu->Execute({menu->ReadMenu()->revision, O::MapSettingSet, "x"}),
            Status::InvalidValue);
  EXPECT_DOUBLE_EQ(controller->dead_zone.GetValue(), 42);
  ASSERT_EQ(Activate("reset"), Status::Success);
  EXPECT_DOUBLE_EQ(controller->dead_zone.GetValue(), 10);
  Back();

  // The extension group's own settings are tunable alongside the extension choice.
  ASSERT_TRUE(Row("set:Extension/Attach MotionPlus").toggle);
  ASSERT_EQ(Activate("set:Extension/Attach MotionPlus"), Status::Success);
  EXPECT_FALSE(controller->attach_motion.GetValue());

  // Choosing an extension shows its inputs in their own section.
  EXPECT_FALSE(HasRow("map:ext:Nunchuk/Buttons/C"));
  ASSERT_EQ(Activate("map-extension"), Status::Success);
  EXPECT_TRUE(Row("ext-0").active);
  ASSERT_EQ(Activate("ext-1"), Status::Success);
  EXPECT_EQ(Row("map-extension").value, "Nunchuk");
  ASSERT_TRUE(HasRow("map:ext:Nunchuk/Buttons/C"));
  EXPECT_EQ(Row("map:ext:Nunchuk/Buttons/C").section, "Nunchuk: Buttons");

  // Profiles are saved by name from the on-screen keyboard; bad names keep the editor open.
  Back();
  ASSERT_EQ(Activate("save-profile"), Status::Success);
  EXPECT_TRUE(menu->ReadMenu()->text_input);
  ASSERT_EQ(menu->Execute({menu->ReadMenu()->revision, O::TextKey, "bad/name"}), Status::Success);
  ASSERT_EQ(Activate("text-done"), Status::Success);
  EXPECT_TRUE(menu->ReadMenu()->text_input);
  ASSERT_EQ(Activate("text-clear"), Status::Success);
  ASSERT_EQ(menu->Execute({menu->ReadMenu()->revision, O::TextKey, "My Pad"}), Status::Success);
  ASSERT_EQ(Activate("text-done"), Status::Success);
  EXPECT_FALSE(menu->ReadMenu()->text_input);
  const auto path = input->GetUserProfileDirectoryPath() + "My Pad.ini";
  Common::IniFile profile;
  ASSERT_TRUE(profile.Load(path));
  EXPECT_TRUE(profile.Exists("Profile"));
  EXPECT_NE(menu->ReadMenu()->description.find("Saved profile My Pad."), std::string::npos);
}

TEST_F(BigPictureSettingsTest, GlobalMenuRejectsForgedScopeChangesAndGameMenuOmitsGlobalOptions)
{
  EXPECT_EQ(menu->Execute({menu->ReadMenu()->revision, O::Game}), Status::Unavailable);
  ASSERT_TRUE(menu->Open(true));
  for (const auto& row : menu->ReadMenu()->rows)
  {
    if (row.edit)
      EXPECT_TRUE(UICommon::Settings::GetCatalog().Find(row.edit->id)->descriptor.per_game);
  }
  EXPECT_FALSE(HasRow("Interface"));
  EXPECT_FALSE(HasRow("Paths"));
  EXPECT_FALSE(HasRow("Audio"));
  EXPECT_EQ(menu->Execute({menu->ReadMenu()->revision, O::Global}), Status::Unavailable);
  ASSERT_EQ(Activate("controllers"), Status::Success);
  ASSERT_EQ(Activate("pad0"), Status::Success);
  EXPECT_FALSE(HasRow("devices"));
}

TEST_F(BigPictureSettingsTest, SettingsOpenOnInterfaceAndPublishIndependentValuesAndHelp)
{
  const auto view = menu->ReadMenu();
  EXPECT_EQ(view->title, "Interface Settings");
  EXPECT_TRUE(view->category_page);
  EXPECT_FALSE(view->back);
  EXPECT_TRUE(Row("Interface").active);
  ASSERT_FALSE(view->rows.empty());
  EXPECT_EQ(view->rows.front().id, "interface.big_picture_theme");
  EXPECT_EQ(Row("interface.big_picture_theme").section, "Appearance");
  EXPECT_EQ(Row("interface.big_picture_theme").value, "Dark");
  EXPECT_FALSE(Row("interface.big_picture_theme").help.empty());
  auto startup = Row("interface.start_big_picture");
  ASSERT_EQ(startup.toggle, false);
  ASSERT_TRUE(startup.edit);
  EXPECT_EQ(startup.edit->operation, Operation::SetDisplay);
  EXPECT_EQ(menu->Apply(*startup.edit).status, Status::Success);
  EXPECT_TRUE(Config::GetBase(Config::MAIN_START_BIG_PICTURE));
  EXPECT_EQ(Row("interface.start_big_picture").toggle, true);
  EXPECT_EQ(menu->Apply(*startup.edit).status, Status::Stale);
  // The row also retains a route to its defaults/inheritance editor.
  ASSERT_TRUE(startup.navigation);
  EXPECT_EQ(startup.navigation->operation, O::Edit);
}

TEST_F(BigPictureSettingsTest, DirectPerGameTogglesPreserveDefaultsAndCanStillClearOverrides)
{
  Config::SetBase(Config::GFX_VSYNC, true);
  ASSERT_EQ(Activate("game"), Status::Success);
  ASSERT_EQ(Activate("Display"), Status::Success);
  auto vsync = Row("display.vsync");
  EXPECT_EQ(vsync.toggle, true);
  EXPECT_NE(vsync.help.find("Inherited"), std::string::npos);
  ASSERT_TRUE(vsync.edit);
  EXPECT_EQ(menu->Apply(*vsync.edit).status, Status::Success);
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
  EXPECT_EQ(Row("display.vsync").toggle, false);
  EXPECT_NE(Row("display.vsync").help.find("Override"), std::string::npos);
  ASSERT_EQ(Activate("display.vsync"), Status::Success);
  ASSERT_EQ(Activate("inherit"), Status::Success);
  Back();
  EXPECT_EQ(Row("display.vsync").toggle, true);
  EXPECT_FALSE(HasRow("Interface"));
  EXPECT_FALSE(HasRow("interface.start_big_picture"));
}

TEST_F(BigPictureSettingsTest, CategoryNavigationRemainsAvailableAndHighlightsCurrentContext)
{
  ASSERT_EQ(Activate("Audio"), Status::Success);
  EXPECT_FALSE(HasRow("global"));
  EXPECT_FALSE(HasRow("game"));
  EXPECT_TRUE(Row("Audio").active);
  ASSERT_EQ(Activate("Display"), Status::Success);
  EXPECT_TRUE(Row("Display").active);
  EXPECT_FALSE(Row("Audio").active);
  ASSERT_EQ(Activate("game"), Status::Success);
  ASSERT_EQ(Activate("PostProcessing"), Status::Success);
  EXPECT_FALSE(HasRow("game"));
  EXPECT_FALSE(HasRow("global"));
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
  ASSERT_EQ(Slide("decrease-large"), Status::Success);
  QCoreApplication::processEvents();
  EXPECT_EQ(desktop.value(), 90);
  const auto old = *Step("increase")->edit;
  desktop.setValue(48);
  QCoreApplication::processEvents();
  EXPECT_EQ(Step("increase")->edit->display_value, 49);
  EXPECT_EQ(Row("value").slider->value, 48);
  EXPECT_EQ(menu->Apply(old).status, Status::Stale);
  EXPECT_EQ(Config::GetBase(Config::MAIN_AUDIO_VOLUME), 48);
}

TEST_F(BigPictureSettingsTest, UnsupportedGameEditsAndStaleNavigationAreRejected)
{
  const auto old = *Row("Audio").navigation;
  ASSERT_EQ(Activate("game"), Status::Success);
  EXPECT_EQ(menu->Execute(old), Status::Stale);
  EXPECT_FALSE(HasRow("Audio"));
  Request audio{menu->ReadSnapshot()->revision, "audio.volume", Operation::Set, 45};
  EXPECT_EQ(menu->Apply(audio).status, Status::UnsupportedContext);
  const auto game_request = *Row("Display").navigation;
  menu->SetGame({});
  EXPECT_EQ(menu->Execute(game_request), Status::Stale);
  EXPECT_FALSE(HasRow("game"));
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
  EXPECT_FALSE(HasRow("devices"));
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

TEST_F(BigPictureSettingsTest, OpeningFromPauseSelectsGameContextAndReusesTheGlobalCatalog)
{
  Config::AddLayer(std::make_unique<GameLoader>(Config::LayerType::GlobalGame));
  Config::AddLayer(std::make_unique<GameLoader>(Config::LayerType::LocalGame));
  Config::SetBase(Config::GFX_VSYNC, true);
  menu->SetGame({"RTEST1", 0, "Running game", true});
  environment.emulating = true;
  menu->SetEnvironment(environment);
  ASSERT_TRUE(menu->Open(true));
  EXPECT_EQ(menu->ReadSnapshot()->target, EditTarget::Game);
  EXPECT_TRUE(Row("Display").active);
  EXPECT_FALSE(menu->ReadMenu()->back);
  EXPECT_NE(menu->ReadMenu()->description.find("Running game"), std::string::npos);
  ASSERT_EQ(Activate("Display"), Status::Success);
  const auto game_display = menu->ReadMenu();
  ASSERT_EQ(Activate("display.vsync"), Status::Success);
  ASSERT_EQ(Activate("off"), Status::Success);
  EXPECT_FALSE(Config::Get(Config::GFX_VSYNC));
  EXPECT_TRUE(Config::GetBase(Config::GFX_VSYNC));
  ASSERT_EQ(Activate("inherit"), Status::Success);
  EXPECT_TRUE(Config::Get(Config::GFX_VSYNC));
  ASSERT_TRUE(menu->Open());
  EXPECT_EQ(menu->ReadSnapshot()->target, EditTarget::Base);
  ASSERT_EQ(Activate("Display"), Status::Success);
  const auto global_display = menu->ReadMenu();
  for (const auto& row : game_display->rows)
    EXPECT_TRUE(std::ranges::any_of(global_display->rows,
                                    [&](const auto& global) { return global.id == row.id; }));
}

TEST_F(BigPictureSettingsTest, OpeningGameSettingsWithoutAGameCannotFallBackToGlobalEdits)
{
  menu->SetGame({});
  EXPECT_FALSE(menu->Open(true));
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
  EXPECT_EQ(menu->ReadMenu()->title, "Interface Settings");
  QCoreApplication::processEvents();
  EXPECT_EQ(menu->ReadMenu()->title, "Audio Settings");
  host.RequestSettingsMenu(0, current);
  QCoreApplication::processEvents();
  EXPECT_EQ(host.GetSnapshot()->result->status, BigPicture::ResultStatus::Stale);
}
}  // namespace
