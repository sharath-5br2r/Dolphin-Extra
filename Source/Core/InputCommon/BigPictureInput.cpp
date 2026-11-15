// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/BigPictureInput.h"

#include <algorithm>
#include <utility>

#include "Common/BigPicture/PresentationState.h"
#include "Common/Common.h"
#include "Common/ScopeGuard.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/InputConfig.h"

namespace BigPictureInput
{
namespace
{
InputConfig s_config("BigPicture", _trans("Big Picture"), "BigPicture", "BigPicture");
std::array<BigPicture::NavigationFilter, CONTROLLER_COUNT> s_filters;
std::array<unsigned, BigPicture::NAV_BUTTON_COUNT> s_held{};
int s_active = 0;
constexpr std::array<const char*, BigPicture::NAV_BUTTON_COUNT> LABELS = {
    _trans("Open Menu"), _trans("Up"),          _trans("Down"), _trans("Left"),
    _trans("Right"),     _trans("Confirm"),     _trans("Back"), _trans("Previous Page"),
    _trans("Next Page"), _trans("Game Options")};
}  // namespace

MenuController::MenuController(int index) : m_index(index)
{
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Navigation")));
  for (const auto* label : LABELS)
    m_buttons->AddInput(ControllerEmu::Translatability::Translate, label);
  groups.emplace_back(m_options = new ControllerEmu::ControlGroup(_trans("Menu Preferences")));
  m_options->AddSetting(&m_swap, {_trans("Swap Confirm and Back")}, false);
  m_options->AddSetting(&m_scale, {_trans("Interface Scale"), _trans("%")}, 100.0, 75.0, 150.0);
  m_options->AddSetting(&m_safe_area, {_trans("Safe Area"), _trans("%")}, 5.0, 0.0, 15.0);
}

std::string MenuController::GetName() const
{
  return "BigPicture" + std::to_string(m_index + 1);
}

InputConfig* MenuController::GetConfig() const
{
  return &s_config;
}

void MenuController::LoadDefaults(const ControllerInterface& controller_interface)
{
  const auto selected_device = GetDefaultDevice();
  EmulatedController::LoadDefaults(controller_interface);
  if (!selected_device.ToString().empty())
    SetDefaultDevice(selected_device);
  // Keyboard entry is supplied by the Open Pause Menu hotkey (Escape by default).
  // Selecting an XInput device also supplies controller navigation defaults.
  if (GetDefaultDevice().source == "XInput")
  {
    constexpr std::array<const char*, BigPicture::NAV_BUTTON_COUNT> bindings = {
        "@(Back+Start)",       "`Pad N` | `Left Y+`", "`Pad S` | `Left Y-`", "`Pad W` | `Left X-`",
        "`Pad E` | `Left X+`", "`Button A`",          "`Button B`",          "`Shoulder L`",
        "`Shoulder R`",        "`Button X`"};
    for (std::size_t i = 0; i < bindings.size(); ++i)
      m_buttons->SetControlExpression(static_cast<int>(i), bindings[i]);
  }
}

ControllerEmu::ControlGroup* MenuController::GetButtons() const
{
  return m_buttons;
}
ControllerEmu::ControlGroup* MenuController::GetOptionsGroup() const
{
  return m_options;
}

BigPicture::NavigationOptions MenuController::GetOptions() const
{
  return BigPicture::SanitizeNavigationOptions(
      {m_swap.GetValue(), static_cast<float>(m_scale.GetValue() / 100.0),
       static_cast<float>(m_safe_area.GetValue() / 100.0)});
}

void MenuController::SetOptions(BigPicture::NavigationOptions options)
{
  options = BigPicture::SanitizeNavigationOptions(options);
  m_swap.SetValue(options.swap_confirm_back);
  m_scale.SetValue(options.scale * 100.0);
  m_safe_area.SetValue(options.safe_area * 100.0);
}

InputConfig* GetConfig()
{
  return &s_config;
}

MenuInput MenuController::ReadInput(bool enabled, bool modal)
{
  if (!enabled || !modal)
  {
    for (const auto& setting : m_options->numeric_settings)
      setting->GetInputReference().ClearHotkeySuppressions();
  }
  MenuInput input;
  for (std::size_t i = 0; i < input.values.size(); ++i)
  {
    const auto& reference = m_buttons->controls[i]->control_ref;
    if (!enabled || (i != 0 && !modal))
    {
      // Chord expressions own global suppressions. Release them when we stop polling
      // the binding, without advancing stateful expressions while navigation is inactive.
      reference->ClearHotkeySuppressions();
    }
    if (i != 0 && !modal)
      continue;
    if (reference->BoundCount() > 0)
    {
      input.connected = true;
      input.has_navigation |= i != 0;
      if (enabled)
        input.values[i] = reference->State();
    }
  }
  return input;
}
MenuController* GetController(int index)
{
  return static_cast<MenuController*>(s_config.GetController(index));
}
void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (int i = 0; i < CONTROLLER_COUNT; ++i)
      s_config.CreateController<MenuController>(i);
  }
  s_filters = {};
  s_held = {};
  s_active = 0;
  s_config.RegisterHotplugCallback();
  LoadConfig();
}
void Shutdown()
{
  s_config.UnregisterHotplugCallback();
  s_config.ClearControllers();
  BigPicture::GetPresentationState().SetNavigationStatus({});
}
void LoadConfig()
{
  s_config.LoadConfig();
  BigPicture::GetPresentationState().ResetInput();
}
void SetOptions(BigPicture::NavigationOptions options)
{
  {
    const auto lock = MenuController::GetStateLock();
    GetController(0)->SetOptions(options);
  }
  BigPicture::GetPresentationState().ResetInput();
  s_config.SaveConfig();
}

bool Poll(bool enabled, bool modal)
{
  const auto lock = MenuController::GetStateLock();
  if (s_config.ControllersNeedToBeCreated())
    return false;
  const auto previous_channel = ControllerInterface::GetCurrentInputChannel();
  const bool previous_gate = ControlReference::GetInputGate();
  ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::BigPicture);
  ControlReference::SetInputGate(enabled);
  Common::ScopeGuard restore([&] {
    ControllerInterface::SetCurrentInputChannel(previous_channel);
    ControlReference::SetInputGate(previous_gate);
  });

  auto& presentation = BigPicture::GetPresentationState();
  const auto snapshot = presentation.GetSnapshot();
  const auto generation = snapshot ? snapshot->generation : 0;
  bool menu = false;
  bool has_navigation = false;
  const auto old_held = s_held;
  for (int port = 0; port < CONTROLLER_COUNT; ++port)
  {
    auto* controller = GetController(port);
    const auto input = controller->ReadInput(enabled, modal);
    has_navigation |= input.has_navigation;
    for (const auto& change : s_filters[port].Update(input.values, input.connected, enabled,
                                                     presentation.GetInputEpoch()))
    {
      const auto button = static_cast<std::size_t>(change.button);
      if (change.down)
      {
        s_active = port;
        s_held[button] |= 1u << port;
      }
      else
      {
        s_held[button] &= ~(1u << port);
      }
    }
  }
  for (std::size_t i = 0; i < s_held.size(); ++i)
  {
    const bool down = s_held[i] != 0;
    if ((old_held[i] != 0) == down)
      continue;
    if (i == static_cast<std::size_t>(BigPicture::NavButton::Menu))
      menu |= down;
    else
      presentation.Enqueue({BigPicture::InputEvent::Type::Navigation, generation,
                            static_cast<std::uint32_t>(i), down});
  }

  if (!modal && !menu)
    return false;
  BigPicture::NavigationStatus status;
  status.gamepad = has_navigation;
  status.options = GetController(0)->GetOptions();
  auto* active = GetController(s_active);
  status.device = active->IsDefaultDeviceConnected() ? active->GetDefaultDevice().name : "";
  status.device_source = active->GetDefaultDevice().source;
  for (std::size_t i = 0; i < status.bindings.size(); ++i)
  {
    const auto& reference = active->GetButtons()->controls[i]->control_ref;
    status.bindings[i] = reference->GetExpression();
    // Qualified single-button bindings carry their own device identity. The renderer
    // resolves that qualifier per hint; it must not erase the default for other hints.
  }
  presentation.SetNavigationStatus(std::move(status));
  return menu;
}
}  // namespace BigPictureInput
