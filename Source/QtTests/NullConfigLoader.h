// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Layer.h"

class NullLoader final : public Config::ConfigLayerLoader
{
public:
  NullLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer*) override {}
  void Save(Config::Layer*) override {}
};
