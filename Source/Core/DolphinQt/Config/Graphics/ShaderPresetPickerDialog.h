// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QDialog>
#include <QString>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

namespace VideoCommon
{
struct PresetTreeNode;
}

// Tree view over the Shaders folders (packs > folders > presets) with a live search box, ported
// from PCSX2's ShaderPresetPickerDialog. It replaces a flat combo box that held every preset found
// on disk -- around 30,000 entries with a full libretro pack installed.
class ShaderPresetPickerDialog final : public QDialog
{
  Q_OBJECT
public:
  // `current_preset` is the relative preset path to open on, or empty for none.
  ShaderPresetPickerDialog(QWidget* parent, const QString& current_preset);

  // Relative preset path ('/' separators, no ".slangp") chosen by the user; empty if none. Only
  // meaningful after the dialog was accepted.
  const QString& SelectedPreset() const { return m_selected; }

private:
  // The relative preset path on leaves, the folder path on folders. The proxy filters on this
  // rather than on the display name, so typing "crt" matches every preset under "crt/" even
  // though a leaf's label is only its last path segment.
  static constexpr int ROLE_PATH = Qt::UserRole;
  static constexpr int ROLE_IS_PRESET = Qt::UserRole + 1;

  void CreateWidgets();
  void ConnectWidgets();
  void BuildModel();
  void AppendNodes(const std::vector<VideoCommon::PresetTreeNode>& nodes, QStandardItem* parent);
  QStandardItem* FindPresetItem(const QString& preset) const;
  void SelectPreset(const QString& preset);
  void OnFilterChanged(const QString& text);
  void OnCurrentChanged(const QModelIndex& current);
  void OnActivated(const QModelIndex& index);

  QLineEdit* m_filter;
  QTreeView* m_tree;
  QLabel* m_selection;
  QDialogButtonBox* m_buttons;
  QStandardItemModel* m_model;
  QSortFilterProxyModel* m_proxy;
  QString m_selected;
};
