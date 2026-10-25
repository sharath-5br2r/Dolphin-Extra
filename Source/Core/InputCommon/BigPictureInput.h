// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>

#include "Common/BigPicture/Navigation.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

class InputConfig;
namespace ControllerEmu
{
class Buttons;
}

namespace BigPictureInput
{
constexpr int CONTROLLER_COUNT = 4;

struct MenuInput
{
  BigPicture::NavValues values{};
  bool connected = false;
  bool has_navigation = false;
};

class MenuController final : public ControllerEmu::EmulatedController
{
public:
  explicit MenuController(int index);
  std::string GetName() const override;
  InputConfig* GetConfig() const override;
  void LoadDefaults(const ControllerInterface& controller_interface) override;
  ControllerEmu::ControlGroup* GetButtons() const;
  ControllerEmu::ControlGroup* GetOptionsGroup() const;
  BigPicture::NavigationOptions GetOptions() const;
  void SetOptions(BigPicture::NavigationOptions options);
  // Caller holds the controller state lock, as for all input-reference evaluation.
  MenuInput ReadInput(bool enabled, bool modal);

private:
  int m_index;
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::ControlGroup* m_options;
  ControllerEmu::SettingValue<bool> m_swap;
  ControllerEmu::SettingValue<double> m_scale;
  ControllerEmu::SettingValue<double> m_safe_area;
};

void Initialize();
void Shutdown();
void LoadConfig();
InputConfig* GetConfig();
MenuController* GetController(int index);
void SetOptions(BigPicture::NavigationOptions options);
// Called only by the existing host-input owner, including before boot and while paused.
// Returns one menu-toggle edge; never performs a frontend/emulator operation here.
bool Poll(bool enabled, bool modal);
}  // namespace BigPictureInput
