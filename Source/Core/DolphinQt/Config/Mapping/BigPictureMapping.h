// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class BigPictureMapping final : public MappingWidget
{
  Q_OBJECT
public:
  explicit BigPictureMapping(MappingWindow* window);
  InputConfig* GetConfig() override;
  void LoadSettings() override;
  void SaveSettings() override;
};
