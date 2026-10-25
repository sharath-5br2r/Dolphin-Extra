// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/ShaderPresetPickerDialog.h"

#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"
#include "VideoCommon/PostProcessing/PresetTree.h"

ShaderPresetPickerDialog::ShaderPresetPickerDialog(QWidget* parent, const QString& current_preset)
    : QDialog(parent)
{
  setWindowTitle(tr("Select Shader Preset"));
  resize(640, 540);

  CreateWidgets();
  BuildModel();
  ConnectWidgets();

  m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  SelectPreset(current_preset);
  m_filter->setFocus();
}

void ShaderPresetPickerDialog::CreateWidgets()
{
  m_filter = new QLineEdit(this);
  m_filter->setPlaceholderText(tr("Search presets..."));
  m_filter->setClearButtonEnabled(true);

  m_tree = new QTreeView(this);
  m_tree->setUniformRowHeights(true);
  m_tree->setHeaderHidden(true);
  m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);

  m_selection = new QLabel(tr("No preset selected."), this);
  m_selection->setWordWrap(true);

  m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

  auto* const layout = new QVBoxLayout(this);
  layout->addWidget(m_filter);
  layout->addWidget(m_tree);
  layout->addWidget(m_selection);
  layout->addWidget(m_buttons);
  setLayout(layout);

  setTabOrder(m_filter, m_tree);
}

void ShaderPresetPickerDialog::ConnectWidgets()
{
  connect(m_filter, &QLineEdit::textChanged, this, &ShaderPresetPickerDialog::OnFilterChanged);
  connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex& current, const QModelIndex&) { OnCurrentChanged(current); });
  connect(m_tree, &QTreeView::activated, this, &ShaderPresetPickerDialog::OnActivated);
  connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ShaderPresetPickerDialog::BuildModel()
{
  m_model = new QStandardItemModel(this);
  AppendNodes(VideoCommon::BuildPresetTree(VideoCommon::MultipassPostProcessing::GetPresetList()),
              m_model->invisibleRootItem());

  m_proxy = new QSortFilterProxyModel(this);
  m_proxy->setSourceModel(m_model);
  m_proxy->setRecursiveFilteringEnabled(true);
  m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_proxy->setFilterRole(ROLE_PATH);
  m_tree->setModel(m_proxy);
}

void ShaderPresetPickerDialog::AppendNodes(const std::vector<VideoCommon::PresetTreeNode>& nodes,
                                           QStandardItem* parent)
{
  for (const VideoCommon::PresetTreeNode& node : nodes)
  {
    auto* const item = new QStandardItem(QString::fromStdString(node.label));
    item->setEditable(false);
    item->setData(QString::fromStdString(node.path), ROLE_PATH);
    item->setData(node.is_preset, ROLE_IS_PRESET);
    if (node.is_preset)
    {
      // The label is only the last path segment, so the tooltip is how a user tells two presets
      // with the same file name in different packs apart.
      item->setToolTip(QString::fromStdString(node.path));
    }
    else
    {
      // A folder is a grouping node: picking one would not name a preset.
      item->setSelectable(false);
    }
    parent->appendRow(item);
    AppendNodes(node.children, item);
  }
}

QStandardItem* ShaderPresetPickerDialog::FindPresetItem(const QString& preset) const
{
  // An empty Shaders folder gives an empty model, whose index(0, 0) is invalid.
  if (preset.isEmpty() || m_model->rowCount() == 0)
    return nullptr;

  const QModelIndexList matches = m_model->match(m_model->index(0, 0), ROLE_PATH, preset, 1,
                                                 Qt::MatchExactly | Qt::MatchRecursive);
  return matches.isEmpty() ? nullptr : m_model->itemFromIndex(matches.first());
}

void ShaderPresetPickerDialog::SelectPreset(const QString& preset)
{
  QStandardItem* const item = FindPresetItem(preset);
  if (item == nullptr)
  {
    m_tree->collapseAll();
    return;
  }

  const QModelIndex proxy_index = m_proxy->mapFromSource(item->index());
  for (QModelIndex parent = proxy_index.parent(); parent.isValid(); parent = parent.parent())
    m_tree->expand(parent);
  m_tree->setCurrentIndex(proxy_index);
  m_tree->scrollTo(proxy_index, QAbstractItemView::PositionAtCenter);
}

void ShaderPresetPickerDialog::OnFilterChanged(const QString& text)
{
  m_proxy->setFilterFixedString(text);
  if (!text.isEmpty())
  {
    m_tree->expandAll();
    return;
  }

  // Collapse back down and scroll to whatever m_selected holds now, so clearing the box does not
  // also lose your place in the tree.
  //
  // "Now" is doing real work in that sentence, because filtering can change the selection without
  // the user touching the tree, in two shapes (measured on Qt 6.11.1 against this exact
  // model/proxy/tree wiring):
  //
  //   - The filter hides the selected row but leaves a sibling visible. QTreeView moves its current
  //     index to that sibling rather than dropping it, so OnCurrentChanged fires with a valid row
  //     and m_selected silently becomes a different preset -- type "guest" with crt/crt-royale
  //     selected and you leave with crt/crt-guest-advanced. OK stays enabled throughout and
  //     applies the substitute; clearing the box restores the substitute, not the original.
  //   - The filter hides the selected row's whole folder. Nothing remains for the current index to
  //     move to, OnCurrentChanged never fires, m_selected keeps the original, and clearing the box
  //     does restore it.
  //
  // So this is not a "restore what you had" path and must not be read as one. It is kept because
  // PCSX2's dialog does exactly this, down to the same three calls in the same order, and matching
  // it was the point of the port; the first shape is inherited, not introduced here. Changing it
  // means deciding what a filter should do to a selection it hides, which is a UI question for
  // both projects rather than a defect in this file.
  m_tree->collapseAll();
  SelectPreset(m_selected);
}

void ShaderPresetPickerDialog::OnCurrentChanged(const QModelIndex& current)
{
  const bool is_preset = current.isValid() && current.data(ROLE_IS_PRESET).toBool();
  m_selected = is_preset ? current.data(ROLE_PATH).toString() : QString();
  m_selection->setText(is_preset ? m_selected : tr("No preset selected."));
  m_buttons->button(QDialogButtonBox::Ok)->setEnabled(is_preset);
}

void ShaderPresetPickerDialog::OnActivated(const QModelIndex& index)
{
  // Double-click or Enter on a preset accepts; on a folder it does nothing.
  if (index.isValid() && index.data(ROLE_IS_PRESET).toBool())
  {
    m_selected = index.data(ROLE_PATH).toString();
    accept();
  }
}
