// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Ui
{
class TriforcePane;
}

class TriforcePane final : public QWidget
{
  Q_OBJECT

public:
  TriforcePane();
  ~TriforcePane() override;

private:
  std::unique_ptr<Ui::TriforcePane> m_ui;
};
