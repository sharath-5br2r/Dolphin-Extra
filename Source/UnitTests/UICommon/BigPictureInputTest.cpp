// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "Common/BigPicture/PresentationState.h"
#include "Core/Core.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapterInputGate.h"

namespace
{
class PhysicalInput final : public ciface::Core::Device::Input
{
public:
  explicit PhysicalInput(std::string name) : m_name(std::move(name)) {}
  std::string GetName() const override { return m_name; }
  ControlState GetState() const override { return value; }
  ControlState value = 0;

private:
  std::string m_name;
};

class PhysicalDevice final : public ciface::Core::Device
{
public:
  PhysicalDevice()
  {
    AddFullAnalogSurfaceInputs(new PhysicalInput("Axis X-"), new PhysicalInput("Axis X+"));
    AddInput(new PhysicalInput("Left Ctrl"));
    AddInput(new PhysicalInput("Right Ctrl"));
    AddCombinedInput("Ctrl", {"Left Ctrl", "Right Ctrl"});
  }
  std::string GetName() const override { return "Capture test"; }
  std::string GetSource() const override { return "Test"; }
};

class TestExpression final : public ciface::ExpressionParser::Expression
{
public:
  ControlState value = 1;
  ControlState GetValue() override { return value; }
  void SetValue(ControlState state) override { value = state; }
  int CountNumControls() const override { return 1; }
  bool IsAnyInputActive() const override { return value > 0.1; }
  void UpdateReferences(ciface::ExpressionParser::ControlEnvironment&) override {}
};

class TestInput final : public InputReference
{
public:
  TestInput()
  {
    auto expression = std::make_unique<TestExpression>();
    test = expression.get();
    m_parsed_expression = std::move(expression);
  }
  TestExpression* test;
};

class BigPictureInputTest : public testing::Test
{
protected:
  void SetUp() override
  {
    BigPicture::GetPresentationState().Publish(nullptr);
    ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::SerialInterface);
    ControlReference::SetInputGate(true);
  }
  void TearDown() override
  {
    BigPicture::GetPresentationState().Publish(nullptr);
    ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::Host);
    ControlReference::SetInputGate(true);
  }
  void Open()
  {
    auto snapshot = std::make_shared<BigPicture::Snapshot>();
    snapshot->mode = BigPicture::Mode::InGame;
    snapshot->host.lifecycle = BigPicture::Lifecycle::Paused;
    BigPicture::GetPresentationState().Publish(snapshot);
  }
};

TEST_F(BigPictureInputTest, ModalCaptureBlocksGuestEvenWhenItsInputGateAllowsBackgroundInput)
{
  TestInput input;
  EXPECT_EQ(input.State(0), 1);
  Open();
  Core::UpdateInputGate(false, false);
  EXPECT_FALSE(ControlReference::GetInputGate());
  EXPECT_EQ(input.State(0), 0);
}

TEST_F(BigPictureInputTest, MenuChannelRemainsAvailableWhileGuestIsCaptured)
{
  TestInput input;
  Open();
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  EXPECT_EQ(input.State(0), 0);  // The button used to open the menu must first be released.
  input.test->value = 0;
  EXPECT_EQ(input.State(0), 0);
  input.test->value = 1;
  EXPECT_EQ(input.State(0), 1);
}

TEST_F(BigPictureInputTest, ControlsNotPolledWhilePausedStillRequireReleaseAfterClose)
{
  TestInput input;
  Open();
  BigPicture::GetPresentationState().Publish(nullptr);
  EXPECT_EQ(input.State(0), 0);
  input.test->value = 0;
  EXPECT_EQ(input.State(0), 0);
  input.test->value = 1;
  EXPECT_EQ(input.State(0), 1);
}

TEST_F(BigPictureInputTest, ConstantsRemainUsableAndFocusLossRequiresNeutralInput)
{
  TestInput input;
  InputReference constant;
  constant.SetExpression("1.0");
  EXPECT_EQ(constant.State(0), 1);
  Open();
  BigPicture::GetPresentationState().SetFocused(false);
  BigPicture::GetPresentationState().Publish(nullptr);
  EXPECT_EQ(input.State(0), 0);
  EXPECT_EQ(constant.State(0), 1);
  BigPicture::GetPresentationState().SetFocused(true);
}

TEST_F(BigPictureInputTest, RelativeMotionAccumulatedDuringMenuDoesNotReachGameplay)
{
  ciface::RelativeInputState<int> motion{};
  motion.Update();  // Establish the current session's epoch.
  motion.Move(10);
  Open();
  motion.Update();
  EXPECT_EQ(motion.GetValue(), 0);
  motion.Move(20);
  BigPicture::GetPresentationState().Publish(nullptr);
  motion.Update();
  EXPECT_EQ(motion.GetValue(), 0);
  motion.Move(3);
  motion.Update();
  EXPECT_EQ(motion.GetValue(), 3);
}

TEST_F(BigPictureInputTest,
       CompanionRelativeSamplingPreservesOtherReadersAndAvoidsDoubleConsumption)
{
  ciface::RelativeInputState<int> motion;
  for (auto channel : {ciface::InputChannel::FreeLook, ciface::InputChannel::BigPicture,
                       ciface::InputChannel::Host, ciface::InputChannel::SerialInterface})
  {
    ControllerInterface::SetCurrentInputChannel(channel);
    motion.Update();
  }
  motion.Move(10);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::FreeLook);
  motion.Update(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 10);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 10);
  motion.Move(3);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::Host);
  motion.Update();
  EXPECT_EQ(motion.GetValue(), 13);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 10);  // A host update must not overwrite the menu's sample.
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::FreeLook);
  motion.Update(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 3);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::SerialInterface);
  motion.Update(ciface::InputChannel::SerialInterface);
  EXPECT_EQ(motion.GetValue(), 13);  // The duplicate channel must not consume its delta twice.
}

TEST_F(BigPictureInputTest, CompanionRelativeSamplingDropsMotionAcrossCaptureEpochs)
{
  ciface::RelativeInputState<int> motion;
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::FreeLook);
  motion.Update(ciface::InputChannel::BigPicture);
  motion.Move(30);
  Open();
  motion.Update(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 0);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 0);
  motion.Move(4);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::FreeLook);
  motion.Update(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 4);
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  EXPECT_EQ(motion.GetValue(), 4);
}

TEST_F(BigPictureInputTest, NativeAdapterPreservesOriginAndBlocksHeldButtonsUntilRelease)
{
  GCAdapterInputGate gate;
  GCPadStatus pressed;
  pressed.button = static_cast<u16>(PAD_BUTTON_A) | PAD_GET_ORIGIN;
  EXPECT_EQ(gate.Filter(pressed, true, 1, false).button, PAD_GET_ORIGIN);
  EXPECT_EQ(gate.Filter(pressed, false, 2, false).button, PAD_GET_ORIGIN);
  EXPECT_EQ(gate.Filter(pressed, true, 2, true).button, pressed.button);
  gate.Filter({}, false, 2, false);
  EXPECT_EQ(gate.Filter(pressed, false, 2, false).button, pressed.button);
}

TEST_F(BigPictureInputTest, AbsoluteSurfacesAndCombinedModifiersHaveDistinctReleaseSemantics)
{
  PhysicalDevice device;
  const auto full = device.FindInput("Full Axis X+");
  ASSERT_NE(full, nullptr);
  EXPECT_EQ(full->GetState(), 0.5);
  EXPECT_FALSE(full->IsPressedForCapture());
  auto* high = static_cast<PhysicalInput*>(device.FindInput("Axis X+"));
  high->value = 0.8;
  EXPECT_TRUE(high->IsPressedForCapture());
  EXPECT_FALSE(full->IsPressedForCapture());
  auto* left = static_cast<PhysicalInput*>(device.FindInput("Left Ctrl"));
  left->value = 1;
  EXPECT_TRUE(device.FindInput("Ctrl")->IsPressedForCapture());
  left->value = 0;
  EXPECT_FALSE(device.FindInput("Ctrl")->IsPressedForCapture());
}
}  // namespace
