// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "UICommon/Settings/Catalog.h"

namespace ConfigWidget
{
Common::Settings::Descriptor CatalogDescriptor(const Config::Location& location);
void RecordCatalogBinding(const Common::Settings::Descriptor& descriptor);
std::shared_ptr<const Common::Settings::Snapshot>
TranslateSettingsSnapshot(const Common::Settings::Snapshot& source);

void BindCatalog(QCheckBox* widget, const Config::Info<bool>& setting,
                 Config::Layer* layer = nullptr);
void BindCatalog(QSlider* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
void BindCatalog(QLineEdit* widget, const Config::Info<std::string>& setting,
                 Config::Layer* layer = nullptr);
FloatSliderHandle BindCatalogFloat(QSlider* widget, const Config::Info<float>& setting,
                                   Config::Layer* layer = nullptr);
void BindCatalogScaled(QSlider* widget, const Config::Info<u32>& setting,
                       Config::Layer* layer = nullptr);

template <typename T>
void BindCatalog(QComboBox* widget, const Config::Info<T>& setting, Config::Layer* layer = nullptr)
{
  const auto descriptor = CatalogDescriptor(setting.GetLocation());
  DEBUG_ASSERT(descriptor.default_value == UICommon::Settings::ToValue(setting.GetDefaultValue()));
  DEBUG_ASSERT(!layer || descriptor.per_game);
  std::vector<T> values;
  for (const auto& choice : descriptor.choices)
    values.push_back(UICommon::Settings::FromValue<T>(choice.value));
  BindMapped(widget, setting, std::span<const T>{values}, layer);
  RecordCatalogBinding(descriptor);
}
}  // namespace ConfigWidget
