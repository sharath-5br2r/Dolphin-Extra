// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QLabel;

class CloudSavesPane final : public QWidget
{
  Q_OBJECT

public:
  CloudSavesPane();

private:
  void RunCheck();
  void MigrateRemoteFolder(const QString& old_folder, const QString& new_folder);

  QCheckBox* m_enable_checkbox;
  QLabel* m_status_label;
  QLineEdit* m_remote_folder_edit;
  QLineEdit* m_remote_edit;
  QString m_previous_remote_folder;
};
