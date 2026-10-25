// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>
#include <QWidget>

namespace Ui
{
class NKitWarningDialog;
}

class NKitWarningDialog final : public QDialog
{
  Q_OBJECT

public:
  static bool ShowUnlessDisabled(QWidget* parent = nullptr);

private:
  explicit NKitWarningDialog(QWidget* parent = nullptr);
  ~NKitWarningDialog() override;

  std::unique_ptr<Ui::NKitWarningDialog> m_ui;
};
