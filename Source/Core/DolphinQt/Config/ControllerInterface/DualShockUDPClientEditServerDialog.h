// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>

#include <QDialog>

namespace Ui
{
class DualShockUDPClientEditServerDialog;
}

class DualShockUDPClientEditServerDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit DualShockUDPClientEditServerDialog(QWidget* parent,
                                              std::optional<size_t> existing_index);
  ~DualShockUDPClientEditServerDialog() override;

private:
  void CreateWidgets();
  void OnServerFinished();

  std::unique_ptr<Ui::DualShockUDPClientEditServerDialog> m_ui;
  std::optional<size_t> m_existing_index;
};
