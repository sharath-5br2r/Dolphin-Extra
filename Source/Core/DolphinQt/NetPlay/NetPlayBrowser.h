// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <QDialog>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "UICommon/NetPlayIndex.h"

namespace Ui
{
class NetPlayBrowser;
}

class NetPlayBrowser : public QDialog
{
  Q_OBJECT
public:
  explicit NetPlayBrowser(QWidget* parent = nullptr);
  ~NetPlayBrowser() override;

  void accept() override;
signals:
  void Join();
  void UpdateStatusRequested(const QString& status);
  void UpdateListRequested(std::vector<NetPlaySession> sessions);

private:
  void ConnectWidgets();

  void Refresh();
  void RefreshLoop();
  void UpdateList();

  void OnSelectionChanged();

  void OnUpdateStatusRequested(const QString& status);
  void OnUpdateListRequested(std::vector<NetPlaySession> sessions);

  void SaveSettings() const;
  void RestoreSettings();

  std::vector<NetPlaySession> m_sessions;

  std::thread m_refresh_thread;
  std::optional<std::map<std::string, std::string>> m_refresh_filters;
  std::mutex m_refresh_filters_mutex;
  Common::Flag m_refresh_run;
  Common::Event m_refresh_event;
  std::unique_ptr<Ui::NetPlayBrowser> m_ui;
};

Q_DECLARE_METATYPE(std::vector<NetPlaySession>)
