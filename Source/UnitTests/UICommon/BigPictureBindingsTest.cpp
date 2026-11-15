// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "Common/BigPicture/PresentationState.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/ScopeGuard.h"
#include "Core/ConfigManager.h"
#include "Core/HotkeyManager.h"
#include "InputCommon/BigPictureInput.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "UICommon/UICommon.h"

namespace
{
class ChordInput final : public ciface::Core::Device::Input
{
public:
  explicit ChordInput(std::string name) : name(std::move(name)) {}
  std::string GetName() const override { return name; }
  ControlState GetState() const override { return value; }
  std::string name;
  ControlState value = 0;
};

class RelativeAxis final : public ciface::Core::Device::RelativeInput
{
public:
  explicit RelativeAxis(const ciface::RelativeInputState<ControlState>& motion) : motion(motion) {}
  std::string GetName() const override { return "RelativeMouse X+"; }
  ControlState GetState() const override { return motion.GetValue(); }
  const ciface::RelativeInputState<ControlState>& motion;
};

class ChordDevice final : public ciface::Core::Device
{
public:
  ChordDevice()
  {
    SetId(0);
    AddInput(shift = new ChordInput("Shift"));
    AddInput(key = new ChordInput("W"));
    AddInput(new RelativeAxis(motion));
  }
  std::string GetName() const override { return "Review input"; }
  std::string GetSource() const override { return "Test"; }
  ChordInput* shift;
  ChordInput* key;
  ciface::RelativeInputState<ControlState> motion;
};

class BindingInterface final : public ControllerInterface
{
public:
  void AddTestDevice(std::shared_ptr<ciface::Core::Device> device)
  {
    m_devices.push_back(std::move(device));
  }
};

class BigPictureBindingLifecycleTest : public testing::Test
{
protected:
  void SetUp() override
  {
    BigPicture::GetPresentationState().Publish(nullptr);
    ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
    ControlReference::SetInputGate(true);
    devices.AddTestDevice(device);
    ciface::Core::DeviceQualifier qualifier;
    qualifier.FromDevice(device.get());
    controller.SetDefaultDevice(qualifier);
    controller.GetButtons()->SetControlExpression(5, "@(Shift+W)");
    controller.UpdateReferences(devices);
    plain.SetExpression("W");
    controller.UpdateSingleControlReference(devices, &plain);
  }
  void TearDown() override
  {
    BigPicture::GetPresentationState().Publish(nullptr);
    ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::Host);
    ControlReference::SetInputGate(true);
  }
  std::unique_lock<std::recursive_mutex> lock = BigPictureInput::MenuController::GetStateLock();
  BindingInterface devices;
  std::shared_ptr<ChordDevice> device = std::make_shared<ChordDevice>();
  BigPictureInput::MenuController controller{0};
  InputReference plain;
};

TEST_F(BigPictureBindingLifecycleTest, ClosingWhileChordIsHeldDoesNotLeaveGameplaySuppressed)
{
  device->shift->value = device->key->value = 1;
  EXPECT_EQ(controller.ReadInput(true, true).values[5], 1);
  EXPECT_EQ(plain.State(0), 0);
  controller.ReadInput(true, false);  // Stop polling navigation while the chord is still held.
  device->shift->value = device->key->value = 0;
  controller.ReadInput(true, false);
  device->key->value = 1;
  EXPECT_EQ(plain.State(0), 1);
}

TEST_F(BigPictureBindingLifecycleTest, CleanupTraversesExpressionsWithoutEvaluatingSideEffects)
{
  controller.GetButtons()->SetControlExpression(
      5, "$counter = $counter + 1, if(1, @(Shift+W) | 0, 0)");
  controller.UpdateReferences(devices);
  device->shift->value = device->key->value = 1;
  EXPECT_EQ(controller.ReadInput(true, true).values[5], 1);
  EXPECT_EQ(plain.State(0), 0);
  const auto counter = controller.GetExpressionVariables().at("counter");
  EXPECT_EQ(*counter, 1);
  controller.ReadInput(true, false);
  EXPECT_EQ(*counter, 1);
  EXPECT_EQ(plain.State(0), 1);
}

TEST_F(BigPictureBindingLifecycleTest, CleanupPreservesSuppressionsOwnedByOtherBindings)
{
  InputReference other;
  other.SetExpression("@(Shift+W)");
  controller.UpdateSingleControlReference(devices, &other);
  device->shift->value = device->key->value = 1;
  EXPECT_EQ(other.State(0), 1);
  EXPECT_EQ(controller.ReadInput(true, true).values[5], 1);
  controller.ReadInput(true, false);
  EXPECT_EQ(plain.State(0), 0);
  other.ClearHotkeySuppressions();
  EXPECT_EQ(plain.State(0), 1);
}

TEST_F(BigPictureBindingLifecycleTest, DisabledPollingReleasesMenuAndNavigationChordOwnership)
{
  controller.GetButtons()->SetControlExpression(0, "@(Shift+W)");
  controller.UpdateReferences(devices);
  device->shift->value = device->key->value = 1;
  EXPECT_EQ(controller.ReadInput(true, true).values[0], 1);
  EXPECT_EQ(plain.State(0), 0);
  const auto disabled = controller.ReadInput(false, true);
  EXPECT_EQ(disabled.values[0], 0);
  EXPECT_EQ(disabled.values[5], 0);
  EXPECT_EQ(plain.State(0), 1);
}

TEST_F(BigPictureBindingLifecycleTest, SharedRelativeSampleReachesTheActualMenuReference)
{
  controller.GetButtons()->SetControlExpression(5, "`RelativeMouse X+`");
  controller.UpdateReferences(devices);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::FreeLook);
  device->motion.Update(ciface::InputChannel::BigPicture);
  device->motion.Move(0.8);
  device->motion.Update(ciface::InputChannel::BigPicture);
  EXPECT_EQ(device->motion.GetValue(), 0.8);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  EXPECT_EQ(controller.ReadInput(true, true).values[5], 0.8);
}

TEST_F(BigPictureBindingLifecycleTest, InactivePreferencesReleaseTheirChordSuppressions)
{
  controller.GetOptionsGroup()->numeric_settings[0]->GetInputReference().SetExpression(
      "@(Shift+W)");
  controller.UpdateReferences(devices);
  device->shift->value = device->key->value = 1;
  EXPECT_TRUE(controller.GetOptions().swap_confirm_back);
  EXPECT_EQ(plain.State(0), 0);
  controller.ReadInput(false, true);
  ControlReference::SetInputGate(false);
  EXPECT_TRUE(controller.GetOptions().swap_confirm_back);  // Preserve the cached preference.
  ControlReference::SetInputGate(true);
  EXPECT_EQ(plain.State(0), 1);
}
}  // namespace

TEST(BigPictureBindingsTest, BindingsAndPreferencesRoundTripWithoutGameControllerConfig)
{
  BigPictureInput::MenuController original(0);
  original.SetDefaultDevice("SDL/0/Test pad");
  original.GetButtons()->SetControlExpression(0, "@(Back+Start)");
  original.GetButtons()->SetControlExpression(1, "`Pad N` | `Left Y+`");
  original.SetOptions({true, 1.25f, 0.1f});
  Common::IniFile ini;
  auto* section = ini.GetOrCreateSection(original.GetName());
  original.SaveConfig(section);
  BigPictureInput::MenuController restored(0);
  restored.LoadConfig(section);
  EXPECT_EQ(restored.GetDefaultDevice().ToString(), "SDL/0/Test pad");
  EXPECT_EQ(restored.GetButtons()->controls[0]->control_ref->GetExpression(), "@(Back+Start)");
  EXPECT_EQ(restored.GetButtons()->controls[1]->control_ref->GetExpression(),
            "`Pad N` | `Left Y+`");
  EXPECT_EQ(restored.GetOptions(), (BigPicture::NavigationOptions{true, 1.25f, 0.1f}));
  EXPECT_EQ(restored.GetButtons()->controls.size(), BigPicture::NAV_BUTTON_COUNT);
}

TEST(BigPictureBindingsTest, BindingSetsUseIndependentIniSections)
{
  Common::IniFile ini;
  for (int port = 0; port < BigPictureInput::CONTROLLER_COUNT; ++port)
  {
    BigPictureInput::MenuController controller(port);
    controller.GetButtons()->SetControlExpression(0, "`Button " + std::to_string(port) + "`");
    controller.SaveConfig(ini.GetOrCreateSection(controller.GetName()));
  }
  for (int port = 0; port < BigPictureInput::CONTROLLER_COUNT; ++port)
  {
    BigPictureInput::MenuController controller(port);
    controller.LoadConfig(ini.GetOrCreateSection(controller.GetName()));
    EXPECT_EQ(controller.GetButtons()->controls[0]->control_ref->GetExpression(),
              "`Button " + std::to_string(port) + "`");
  }
}

TEST(BigPictureBindingsTest, EscapeDefaultsToPauseMenuAndStopRemainsUnbound)
{
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  BindingInterface devices;
  HotkeyManager hotkeys;
  hotkeys.LoadDefaults(devices);
  Common::IniFile ini;
  auto* section = ini.GetOrCreateSection("Hotkeys");
  hotkeys.SaveConfig(section);
  std::string expression;
  ASSERT_TRUE(section->Get("General/Open Pause Menu", &expression));
#ifdef _WIN32
  EXPECT_EQ(expression, "ESCAPE");
#else
  EXPECT_EQ(expression, "Escape");
#endif
  EXPECT_FALSE(section->Exists("General/Stop"));
}

TEST(BigPictureBindingsTest, ExistingHotkeyProfilesGainEscapeOnceAndPreserveSubsequentEdits)
{
  const auto original_user = File::GetUserPath(D_USER_IDX);
  const auto temporary_user = File::CreateTempDir();
  ASSERT_FALSE(temporary_user.empty());
  UICommon::SetUserDirectory(temporary_user);
  File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
  const auto path = File::GetUserPath(D_CONFIG_IDX) + "Hotkeys.ini";
  Common::IniFile ini;
  ini.GetOrCreateSection("Hotkeys")->Set("General/Stop", std::string("ESCAPE"));
  ini.Save(path);
  Config::Init();
  SConfig::Init();
  HotkeyManagerEmu::Initialize();
  Common::ScopeGuard cleanup([&] {
    HotkeyManagerEmu::Shutdown();
    SConfig::Shutdown();
    Config::Shutdown();
    UICommon::SetUserDirectory(original_user);
    File::DeleteDirRecursively(temporary_user);
  });
  ASSERT_TRUE(ini.Load(path));
  auto* section = ini.GetOrCreateSection("Hotkeys");
  EXPECT_TRUE(section->Exists("PauseMenuBindingInitialized"));
  EXPECT_TRUE(section->Exists("General/Open Pause Menu"));
  std::string expression;
  section->Get("General/Stop", &expression);
  EXPECT_EQ(expression, "ESCAPE");
  auto* controller = static_cast<HotkeyManager*>(HotkeyManagerEmu::GetConfig()->GetController(0));
  auto* group = controller->GetHotkeyGroup(HKGP_GENERAL);
  const auto index = controller->GetIndexForGroup(HKGP_GENERAL, HK_OPEN_PAUSE_MENU);
  group->SetControlExpression(index, "");
  HotkeyManagerEmu::GetConfig()->SaveConfig();
  HotkeyManagerEmu::LoadConfig();
  EXPECT_TRUE(group->controls[index]->control_ref->GetExpression().empty());
  group->SetControlExpression(index, "F12");
  HotkeyManagerEmu::GetConfig()->SaveConfig();
  HotkeyManagerEmu::LoadConfig();
  EXPECT_EQ(group->controls[index]->control_ref->GetExpression(), "F12");
}
