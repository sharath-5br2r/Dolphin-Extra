// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QCheckBox;
class QTabWidget;
class QDialogButtonBox;
class QTableWidget;
class QPushButton;

class SDLHintsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit SDLHintsWindow(QWidget* parent);

  QSize sizeHint() const override;

private:
  void CreateMainLayout();

  void PopulateTable();
  void SaveTable();
  void PopulateChecklist();
  void AddRow();
  void RemoveRow();
  void SelectionChanged();
  void OnClose();
  void TabChanged(int new_index);
  void SectionResized(int logical_index, int old_size, int new_size);

  bool eventFilter(QObject* obj, QEvent* event) override;

  QTabWidget* m_tab_widget;
  QDialogButtonBox* m_button_box;
  QTableWidget* m_hints_table;
  QPushButton* m_rem_row_btn;

  QCheckBox* m_directinput_detection;
  QCheckBox* m_combine_joy_cons;
  QCheckBox* m_horizontal_joy_cons;
  QCheckBox* m_dualsense_player_led;

  int m_current_tab_index;
};
