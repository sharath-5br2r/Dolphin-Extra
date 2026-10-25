// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Ui
{
class PathPane;
}

class PathPane final : public QWidget
{
  Q_OBJECT
public:
  explicit PathPane(QWidget* parent = nullptr);
  ~PathPane() override;

private:
  void PopulatePaths();
  void BindSettings();
  void ConnectWidgets();

  void Browse();
  void BrowseDefaultGame();
  void BrowseWiiNAND();
  void BrowseDump();
  void BrowseLoad();
  void BrowseResourcePack();
  void BrowseWFS();
  void RemovePath();

  std::unique_ptr<Ui::PathPane> m_ui;
};
