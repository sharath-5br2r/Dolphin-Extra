// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class QPushButton;

class CloudSavesPane final : public QWidget
{
  Q_OBJECT

public:
  CloudSavesPane();

private:
  void RunCheck();
  void RefreshRemotesList();
  void MigrateRemoteFolder(const QString& old_folder, const QString& new_folder);
  void OpenRemoteWizard();
  void BrowseConfigFile();

  struct ProviderOption
  {
    QString name;
    QString help;
    QString type;
    QString default_val;
    bool is_password = false;
    bool required = false;
    bool advanced = false;
    struct Example
    {
      QString value;
      QString help;
    };
    std::vector<Example> examples;
  };

  struct ProviderInfo
  {
    QString name;
    QString description;
    std::vector<ProviderOption> options;
  };

  std::vector<ProviderInfo> FetchProviders();

  QCheckBox* m_enable_checkbox;
  QLabel* m_status_label;
  QLineEdit* m_remote_folder_edit;
  QComboBox* m_remote_combo;
  QLineEdit* m_remote_edit;
  QLineEdit* m_config_file_edit;
  QPushButton* m_browse_config_button;
  QPushButton* m_new_remote_button;
  QPushButton* m_check_button;
  QString m_previous_remote_folder;
};
