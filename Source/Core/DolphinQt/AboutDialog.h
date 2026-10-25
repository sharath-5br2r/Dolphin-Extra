// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

namespace Ui
{
class AboutDialog;
}

class AboutDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit AboutDialog(QWidget* parent = nullptr);
  ~AboutDialog() override;

private:
  std::unique_ptr<Ui::AboutDialog> m_ui;
};
