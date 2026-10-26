// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <thread>

#include "UICommon/Settings/Catalog.h"

namespace UICommon::Settings
{
// Owner-thread operations; publish copied value-only snapshots to other threads. External
// config changes, context switches and dynamic-option changes invalidate outstanding requests.
class Service
{
public:
  explicit Service(const Catalog& catalog = GetCatalog());
  Service(Catalog&&) = delete;
  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;
  bool SetContext(Context context);
  bool SetEnvironment(Environment environment);
  std::shared_ptr<const Snapshot> ReadSnapshot();
  Result Apply(const Request& request);
  bool Save();

private:
  bool IsOwner() const;
  void SynchronizeVersion();
  std::string Unavailable(const Entry& entry) const;
  const Catalog& m_catalog;
  const std::thread::id m_owner;
  Context m_context;
  Environment m_environment;
  u64 m_revision = 1;
  u64 m_config_version = 0;
};
}  // namespace UICommon::Settings
