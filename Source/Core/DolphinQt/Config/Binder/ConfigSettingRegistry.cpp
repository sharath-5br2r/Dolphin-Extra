// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"

namespace ConfigWidget
{
ConfigSettingRegistry& ConfigSettingRegistry::Instance()
{
  static ConfigSettingRegistry instance;
  return instance;
}

void ConfigSettingRegistry::Record(SettingEntry entry)
{
  const auto [it, inserted] = m_index.try_emplace(entry.location, m_entries.size());
  if (inserted)
  {
    m_entries.push_back(std::move(entry));
    return;
  }

  SettingEntry& existing = m_entries[it->second];
  // A setting is per-game if *any* binding for it was layered.
  existing.per_game = existing.per_game || entry.per_game;
  if (existing.choices.empty())
    existing.choices = std::move(entry.choices);
  if (!existing.minimum.has_value())
    existing.minimum = entry.minimum;
  if (!existing.maximum.has_value())
    existing.maximum = entry.maximum;
  if (!existing.step.has_value())
    existing.step = entry.step;
  if (existing.title.isEmpty())
    existing.title = std::move(entry.title);
  if (existing.description.isEmpty())
    existing.description = std::move(entry.description);
}

void ConfigSettingRegistry::SetText(const Config::Location& location, QString title,
                                    QString description)
{
  const auto it = m_index.find(location);
  if (it == m_index.end())
    return;

  // SetDescription should win over whatever a previous bind inferred, so overwrite
  // unconditionally rather than filling empty fields only.
  SettingEntry& entry = m_entries[it->second];
  entry.title = std::move(title);
  entry.description = std::move(description);
}

const SettingEntry* ConfigSettingRegistry::Find(const Config::Location& location) const
{
  const auto it = m_index.find(location);
  return it == m_index.end() ? nullptr : &m_entries[it->second];
}

void ConfigSettingRegistry::ClearForTesting()
{
  m_entries.clear();
  m_index.clear();
}
}  // namespace ConfigWidget
