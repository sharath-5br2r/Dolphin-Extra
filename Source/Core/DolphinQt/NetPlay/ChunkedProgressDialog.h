// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <span>

#include <QDialog>

#include "Common/CommonTypes.h"

class QLabel;
class QProgressBar;
class QWidget;

namespace Ui
{
class ChunkedProgressDialog;
}

class ChunkedProgressDialog : public QDialog
{
  Q_OBJECT
public:
  explicit ChunkedProgressDialog(QWidget* parent);
  ~ChunkedProgressDialog() override;

  void show(const QString& title, u64 data_size, std::span<const int> players);
  void SetProgress(int pid, u64 progress);

  void reject() override;

private:
  void ConnectWidgets();

  std::map<int, QProgressBar*> m_progress_bars;
  std::map<int, QLabel*> m_status_labels;
  u64 m_data_size = 0;
  std::unique_ptr<Ui::ChunkedProgressDialog> m_ui;
};
