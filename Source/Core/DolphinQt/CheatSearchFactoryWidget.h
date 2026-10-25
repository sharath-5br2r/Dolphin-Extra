// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

#include "Core/CheatSearch.h"

namespace Ui
{
class CheatSearchFactoryWidget;
}

class CheatSearchFactoryWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CheatSearchFactoryWidget();
  ~CheatSearchFactoryWidget() override;

signals:
  void NewSessionCreated(const Cheats::CheatSearchSessionBase& session);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void RefreshGui();

  void OnAddressSpaceRadioChanged();
  void OnNewSearchClicked();

  std::unique_ptr<Ui::CheatSearchFactoryWidget> m_ui;
};
