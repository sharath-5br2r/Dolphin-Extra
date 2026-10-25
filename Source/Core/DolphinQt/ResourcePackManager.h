// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

namespace Ui
{
class ResourcePackManager;
}

class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class ResourcePackManager : public QDialog
{
public:
  explicit ResourcePackManager(QWidget* parent = nullptr);
  ~ResourcePackManager() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void OpenResourcePackDir();
  void RepopulateTable();
  void Change();
  void Install();
  void Uninstall();
  void Remove();
  void PriorityUp();
  void PriorityDown();
  void Refresh();

  void SelectionChanged();
  void ItemDoubleClicked(QTableWidgetItem* item);

  int GetResourcePackIndex(QTableWidgetItem* item) const;

  std::unique_ptr<Ui::ResourcePackManager> m_ui;
  QPushButton* m_open_directory_button;
  QPushButton* m_change_button;
  QPushButton* m_remove_button;
  QPushButton* m_refresh_button;
  QPushButton* m_priority_up_button;
  QPushButton* m_priority_down_button;

  QTableWidget* m_table_widget;
};
