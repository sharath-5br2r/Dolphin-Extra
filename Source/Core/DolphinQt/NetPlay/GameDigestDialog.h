// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QDialog>

class QLabel;
class QProgressBar;
class QWidget;

namespace Ui
{
class GameDigestDialog;
}

class GameDigestDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GameDigestDialog(QWidget* parent);
  ~GameDigestDialog() override;

  void show(const QString& title);
  void SetProgress(int pid, int progress);
  void SetResult(int pid, const std::string& result);

  void reject() override;

private:
  void ConnectWidgets();

  std::map<int, QProgressBar*> m_progress_bars;
  std::map<int, QLabel*> m_status_labels;

  std::vector<std::string> m_results;
  std::unique_ptr<Ui::GameDigestDialog> m_ui;
};
