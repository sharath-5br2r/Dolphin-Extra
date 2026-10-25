// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "Core/HW/WiimoteEmu/ExtensionPort.h"

namespace Ui
{
class WiimoteEmuExtension;
}

class WiimoteEmuExtension final : public MappingWidget
{
  Q_OBJECT
public:
  explicit WiimoteEmuExtension(MappingWindow* window);
  ~WiimoteEmuExtension() override;

  InputConfig* GetConfig() override;

  void ChangeExtensionType(u32 type);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateClassicLayout();
  void CreateDrumsLayout();
  void CreateGuitarLayout();
  void CreateNunchukLayout();
  void CreateTurntableLayout();
  void CreateUDrawTabletLayout();
  void CreateDrawsomeTabletLayout();
  void CreateTaTaConLayout();
  void CreateShinkansenLayout();

  std::unique_ptr<Ui::WiimoteEmuExtension> m_ui;
};
