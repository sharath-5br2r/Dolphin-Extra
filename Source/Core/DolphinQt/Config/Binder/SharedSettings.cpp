// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/SharedSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <QCoreApplication>
#include <QSlider>

#include "Common/Logging/Log.h"
#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"

namespace ConfigWidget
{
Common::Settings::Descriptor CatalogDescriptor(const Config::Location& location)
{
  const auto* entry = UICommon::Settings::GetCatalog().Find(location);
  if (!entry)
  {
    ERROR_LOG_FMT(COMMON, "Setting is not in the supported shared catalog");
    std::abort();
  }
  return entry->descriptor;
}

void RecordCatalogBinding(const Common::Settings::Descriptor& descriptor)
{
  ConfigSettingRegistry::Instance().SetDescriptor(descriptor);
}

std::shared_ptr<const Common::Settings::Snapshot>
TranslateSettingsSnapshot(const Common::Settings::Snapshot& source)
{
  auto snapshot = std::make_shared<Common::Settings::Snapshot>(source);
  const auto translate = [](std::string& text) {
    if (!text.empty())
      text = QCoreApplication::translate("SharedSettings", text.c_str()).toStdString();
  };
  for (auto& state : snapshot->settings)
  {
    translate(state.descriptor.label);
    translate(state.descriptor.help);
    translate(state.descriptor.category);
    translate(state.descriptor.section);
    translate(state.disabled_reason);
    for (auto& choice : state.descriptor.choices)
      translate(choice.label);
  }
  return snapshot;
}

void BindCatalog(QCheckBox* widget, const Config::Info<bool>& setting, Config::Layer* layer)
{
  const auto descriptor = CatalogDescriptor(setting.GetLocation());
  DEBUG_ASSERT(!layer || descriptor.per_game);
  Bind(widget, setting, layer, descriptor.reverse);
  RecordCatalogBinding(descriptor);
}

void BindCatalog(QSlider* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  const auto descriptor = CatalogDescriptor(setting.GetLocation());
  DEBUG_ASSERT(!layer || descriptor.per_game);
  if (!descriptor.ticks.empty())
  {
    std::vector<int> ticks;
    for (const auto& value : descriptor.ticks)
      ticks.push_back(std::get<int>(value));
    BindMapped(widget, setting, ticks, layer);
  }
  else
  {
    DEBUG_ASSERT(descriptor.range && widget->minimum() == descriptor.range->minimum &&
                 widget->maximum() == descriptor.range->maximum);
    Bind(widget, setting, layer);
  }
  RecordCatalogBinding(descriptor);
}

void BindCatalog(QLineEdit* widget, const Config::Info<std::string>& setting, Config::Layer* layer)
{
  const auto descriptor = CatalogDescriptor(setting.GetLocation());
  DEBUG_ASSERT(!layer || descriptor.per_game);
  Bind(widget, setting, layer);
  RecordCatalogBinding(descriptor);
}

FloatSliderHandle BindCatalogFloat(QSlider* widget, const Config::Info<float>& setting,
                                   Config::Layer* layer)
{
  const auto descriptor = CatalogDescriptor(setting.GetLocation());
  DEBUG_ASSERT(descriptor.range && (!layer || descriptor.per_game));
  const auto& range = *descriptor.range;
  const auto result =
      BindFloat(widget, setting, static_cast<float>(range.minimum),
                static_cast<float>(range.maximum), static_cast<float>(range.step), layer);
  RecordCatalogBinding(descriptor);
  return result;
}

void BindCatalogScaled(QSlider* widget, const Config::Info<u32>& setting, Config::Layer* layer)
{
  const auto descriptor = CatalogDescriptor(setting.GetLocation());
  DEBUG_ASSERT(descriptor.range && (!layer || descriptor.per_game));
  const auto& range = *descriptor.range;
  const auto scale = static_cast<u32>(std::round(1 / range.scale));
  DEBUG_ASSERT(scale > 0 && widget->minimum() * double(scale) == range.minimum &&
               widget->maximum() * double(scale) == range.maximum);
  BindScaled(widget, setting, scale, layer);
  RecordCatalogBinding(descriptor);
}
}  // namespace ConfigWidget
