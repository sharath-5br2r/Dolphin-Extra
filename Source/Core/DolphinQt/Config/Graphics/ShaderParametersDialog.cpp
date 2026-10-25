// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/ShaderParametersDialog.h"

#include <utility>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

#include "VideoCommon/PostProcessing/ShaderParameterEditor.h"


ShaderParametersDialog::ShaderParametersDialog(QWidget* parent, const QString& preset,
                                               bool opened_from_game_properties)
    : QDialog(parent)
{
  const std::string preset_id = preset.toStdString();

  // PCSX2 titles the window with Path::GetFileTitle(preset), the leaf name without its extension.
  // Dolphin's stored preset id carries no extension, so the leaf segment already *is* the file
  // title -- and it is the same string the picker's tree shows as a leaf label.
  const size_t leaf = preset_id.rfind('/');
  const std::string title = leaf == std::string::npos ? preset_id : preset_id.substr(leaf + 1);
  setWindowTitle(tr("Shader Parameters - %1").arg(QString::fromStdString(title)));
  resize(720, 560);

  CreateWidgets(opened_from_game_properties);

  m_editor = VideoCommon::ShaderParameterEditor::Open(preset_id);
  if (!ShowLoadStatus())
    return;

  BuildRows();
}

void ShaderParametersDialog::CreateWidgets(bool opened_from_game_properties)
{
  // Hand-built rather than ported from PCSX2's ShaderParametersDialog.ui because DolphinQt has no
  // uic step: there is no .ui file anywhere under Source/Core/DolphinQt and no AUTOUIC in its
  // CMake. The values below are that file's.
  m_status = new QLabel(this);
  m_status->setWordWrap(true);

  auto* const contents = new QWidget(this);
  m_grid = new QGridLayout(contents);
  m_grid->setContentsMargins(8, 8, 8, 8);
  m_grid->setHorizontalSpacing(10);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::StyledPanel);
  m_scroll->setWidget(contents);

  m_reset_all = new QPushButton(tr("Reset All"), this);
  auto* const close = new QPushButton(tr("Close"), this);
  close->setDefault(true);

  auto* const footer = new QHBoxLayout();
  footer->addWidget(m_reset_all);
  footer->addStretch();
  footer->addWidget(close);

  auto* const layout = new QVBoxLayout(this);
  if (opened_from_game_properties)
  {
    // Above the parameters, not on a tooltip: the values below are shared, and the one page that
    // makes that surprising is the one this was opened from. A tooltip would only reach the user
    // after they had already changed something for what they thought was one game.
    auto* const scope = new QLabel(
        tr("Note: parameter values are global. They apply to every game that uses this preset."),
        this);
    scope->setWordWrap(true);
    layout->addWidget(scope);
  }
  layout->addWidget(m_status);
  layout->addWidget(m_scroll);
  layout->addLayout(footer);

  connect(m_reset_all, &QPushButton::clicked, this, &ShaderParametersDialog::OnResetAllClicked);
  connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

bool ShaderParametersDialog::ShowLoadStatus()
{
  using Status = VideoCommon::ShaderParameterEditor::Status;
  switch (m_editor->GetStatus())
  {
  case Status::Error:
    m_status->setText(
        tr("Could not load preset: %1").arg(QString::fromStdString(m_editor->GetError())));
    m_reset_all->setEnabled(false);
    return false;
  case Status::NoParameters:
    m_status->setText(tr("This preset has no adjustable parameters."));
    m_reset_all->setEnabled(false);
    return false;
  case Status::Ok:
    break;
  }

  m_status->hide();
  return true;
}

void ShaderParametersDialog::BuildRows()
{
  const auto& rows = m_editor->GetRows();
  m_rows.resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++)
  {
    const VideoCommon::LibrashaderParameters::ParameterInfo& info = rows[i].info;
    RowWidgets& widgets = m_rows[i];
    const int grid_row = static_cast<int>(i);

    // The label is the human-readable description, falling back to the id when a shader declares
    // none; the tooltip is always the id, so it stays discoverable.
    auto* const label =
        new QLabel(QString::fromStdString(info.description.empty() ? info.name : info.description));
    // Pack data, so pin the format: left at Qt::AutoText a description shaped like an HTML tag
    // would render with the "tag" swallowed and the row become unidentifiable. No preset in the
    // libretro pack declares one today, and the reference leaves this at the default.
    label->setTextFormat(Qt::PlainText);
    label->setToolTip(QString::fromStdString(info.name));
    m_grid->addWidget(label, grid_row, 0);

    // The slider's positions and what they mean are the editor's (VideoCommon::SliderMapping); a
    // parameter whose range has no usable positions gets the spin box alone.
    if (rows[i].slider.positions > 0)
    {
      widgets.slider = new QSlider(Qt::Horizontal);
      widgets.slider->setRange(0, rows[i].slider.positions);
      widgets.slider->setMinimumWidth(180);
      widgets.slider->setFocusPolicy(Qt::StrongFocus);
      widgets.slider->installEventFilter(this);
      m_grid->addWidget(widgets.slider, grid_row, 1);
      connect(widgets.slider, &QSlider::valueChanged, this, [this, i](int position) {
        if (!m_updating && m_editor->SetSliderPosition(i, position))
          RefreshRow(i);
      });
    }

    widgets.spin = new QDoubleSpinBox();
    // Decimals first: QDoubleSpinBox rounds the range and the step to the current precision, so
    // setting them the other way round quantises both.
    widgets.spin->setDecimals(VideoCommon::LibrashaderParameters::DecimalsForStep(info.step));
    if (info.step > 0.0f && info.maximum > info.minimum)
    {
      // Includes the sub-step ranges that lost their slider: the preset's range still applies.
      widgets.spin->setRange(info.minimum, info.maximum);
      widgets.spin->setSingleStep(info.step);
    }
    else
    {
      widgets.spin->setRange(-1.0e9, 1.0e9);
      widgets.spin->setSingleStep(info.step > 0.0f ? info.step : 1.0);
    }
    widgets.spin->setMinimumWidth(90);
    // Typing a multi-digit value would otherwise fire an edit per keystroke.
    widgets.spin->setKeyboardTracking(false);
    widgets.spin->setFocusPolicy(Qt::StrongFocus);
    widgets.spin->installEventFilter(this);
    m_grid->addWidget(widgets.spin, grid_row, 2);
    connect(widgets.spin, &QDoubleSpinBox::valueChanged, this, [this, i](double value) {
      if (!m_updating && m_editor->SetValue(i, static_cast<float>(value)))
        RefreshRow(i);
    });

    widgets.reset = new QPushButton(tr("Reset"));
    m_grid->addWidget(widgets.reset, grid_row, 3);
    connect(widgets.reset, &QPushButton::clicked, this, [this, i] {
      if (m_editor->Reset(i))
        RefreshRow(i);
    });

    RefreshRow(i);
  }
  m_grid->setColumnStretch(1, 1);
  m_grid->setRowStretch(static_cast<int>(rows.size()), 1);
}

bool ShaderParametersDialog::eventFilter(QObject* watched, QEvent* event)
{
  // A wheel event over an unfocused slider or spin box scrolls the list instead of editing the
  // parameter. On a preset with a hundred rows this is the difference between scrolling past them
  // and silently changing every value on the way.
  QWidget* const widget = qobject_cast<QWidget*>(watched);
  if (event->type() == QEvent::Wheel && widget != nullptr && !widget->hasFocus())
  {
    QApplication::sendEvent(m_scroll->viewport(), event);
    return true;
  }
  return QDialog::eventFilter(watched, event);
}

void ShaderParametersDialog::RefreshRow(size_t index)
{
  // The slider and the spin box each drive the other, so without the guard an edit to either
  // oscillates between them.
  const bool was_updating = m_updating;
  m_updating = true;
  RowWidgets& widgets = m_rows[index];
  if (widgets.slider != nullptr)
    widgets.slider->setValue(m_editor->GetSliderPosition(index));
  widgets.spin->setValue(m_editor->GetRows()[index].value);
  widgets.reset->setEnabled(!m_editor->IsDefault(index));
  m_updating = was_updating;
}

void ShaderParametersDialog::OnResetAllClicked()
{
  // Edits are stored as they happen (ShaderParameterEditor persists every change to the base layer
  // in memory; the INI is written by Config::Save() when the settings window closes) and reach a
  // running chain through the generation counter it polls per frame, so a preset can be tuned while
  // a game is on screen. The values are global whichever page opened this dialog -- see the note
  // CreateWidgets adds when the caller is a game's own graphics page.
  if (!m_editor->ResetAll())
    return;
  for (size_t i = 0; i < m_rows.size(); i++)
    RefreshRow(i);
}
