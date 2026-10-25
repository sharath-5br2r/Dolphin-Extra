// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

namespace Ui
{
class GCPadWiiUConfigDialog;
}

class GCPadWiiUConfigDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit GCPadWiiUConfigDialog(int port, QWidget* parent = nullptr);
  ~GCPadWiiUConfigDialog() override;

private:
  void LoadSettings();
  void SaveSettings();

  void CreateLayout();
  void ConnectWidgets();

private:
  void UpdateAdapterStatus();

  std::unique_ptr<Ui::GCPadWiiUConfigDialog> m_ui;
  int m_port;
};
