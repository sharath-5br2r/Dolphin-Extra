// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QListWidget>
#include <QWidget>

#include "DolphinQt/QtUtils/QtUtils.h"

using MinimumSizeHintListWidget = QtUtils::MinimumSizeHintWidget<QListWidget>;

namespace Ui
{
class WiiPane;
}

class WiiPane : public QWidget
{
  Q_OBJECT
public:
  explicit WiiPane(QWidget* parent = nullptr);
  ~WiiPane() override;

private:
  void ConfigureWidgets();
  void BindSettings();
  void ConnectWidgets();
  void AddDescriptions();

  void PopulateUSBPassthroughListWidget();

  void OnEmulationStateChanged(bool running);

  void ValidateSelectionState();

  void OnUSBWhitelistAddButton();
  void OnUSBWhitelistRemoveButton();

  void BrowseSDRaw();
  void BrowseSDSyncFolder();

  std::unique_ptr<Ui::WiiPane> m_ui;
};
