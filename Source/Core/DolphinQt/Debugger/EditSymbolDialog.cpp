// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/EditSymbolDialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSpinBox>

#include "ui_EditSymbolDialog.h"

EditSymbolDialog::EditSymbolDialog(QWidget* parent, const u32 symbol_address, u32* symbol_size,
                                   std::string* symbol_name, Type type)
    : QDialog(parent), m_type(type), m_symbol_name(symbol_name), m_symbol_size(symbol_size),
      m_symbol_address(symbol_address)
{
  setWindowTitle(m_type == Type::Symbol ? tr("Edit Symbol") : tr("Edit Note"));
  CreateWidgets();
  ConnectWidgets();
}

void EditSymbolDialog::CreateWidgets()
{
  Ui::EditSymbolDialog ui;
  ui.setupUi(this);
  m_buttons = ui.buttonBox;
  m_name_edit = ui.nameEdit;
  m_address_end_edit = ui.addressEndEdit;
  m_size_lines_spin = ui.sizeLinesSpinBox;
  m_size_hex_edit = ui.sizeHexEdit;

  m_buttons->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
  m_buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);

  // i18n: %1 is an address. %2 is a warning message.
  ui.infoLabel->setText((m_type == Type::Symbol ? tr("Editing symbol starting at: %1\n%2") :
                                                  tr("Editing note starting at: %1\n%2"))
                            .arg(QString::number(m_symbol_address, 16))
                            .arg(tr("Warning: Must save the symbol map for changes to be kept.")));
  m_name_edit->setPlaceholderText(m_type == Type::Symbol ? tr("Symbol name") : tr("Note name"));

  // Get system font and use to size boxes.
  QFont font;
  QFontMetrics fm(font);
  const int width = fm.horizontalAdvance(QLatin1Char('0')) * 2;
  m_address_end_edit->setFixedWidth(width * 6);
  m_size_hex_edit->setFixedWidth(width * 5);
  m_size_lines_spin->setFixedWidth(width * 5);

  // Accept hex input only
  QRegularExpression rx(QStringLiteral("[0-9a-fA-F]{0,8}"));
  QValidator* validator = new QRegularExpressionValidator(rx, this);
  m_address_end_edit->setValidator(validator);
  m_size_hex_edit->setValidator(validator);

  FillFunctionData();
}

void EditSymbolDialog::FillFunctionData()
{
  m_name_edit->setText(QString::fromStdString(*m_symbol_name));
  m_size_lines_spin->setValue(*m_symbol_size / 4);
  m_size_hex_edit->setText(QString::number(*m_symbol_size, 16));
  m_address_end_edit->setText(
      QStringLiteral("%1").arg(m_symbol_address + *m_symbol_size, 8, 16, QLatin1Char('0')));
}

void EditSymbolDialog::UpdateAddressData(u32 size)
{
  // Not sure what the max size should be. Definitely not a full 8, so set to 7.
  size = size & 0xFFFFFFF;

  m_size_lines_spin->setValue(size / 4);
  m_size_hex_edit->setText(QString::number(size, 16));
  m_address_end_edit->setText(
      QStringLiteral("%1").arg(m_symbol_address + size, 8, 16, QLatin1Char('0')));
}

void EditSymbolDialog::ConnectWidgets()
{
  connect(m_size_lines_spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) { UpdateAddressData(value * 4); });

  connect(m_size_hex_edit, &QLineEdit::editingFinished, this, [this] {
    bool good;
    const u32 size = m_size_hex_edit->text().toUInt(&good, 16);
    if (good)
      UpdateAddressData(size);
  });

  connect(m_address_end_edit, &QLineEdit::textEdited, this, [this] {
    bool good;
    const u32 end = m_address_end_edit->text().toUInt(&good, 16);
    if (good && end > m_symbol_address)
      UpdateAddressData(end - m_symbol_address);
  });

  connect(m_buttons, &QDialogButtonBox::accepted, this, &EditSymbolDialog::Accepted);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &EditSymbolDialog::reject);
  connect(m_buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton* btn) {
    const auto role = m_buttons->buttonRole(btn);
    if (role == QDialogButtonBox::ButtonRole::ResetRole)
    {
      FillFunctionData();
    }
    else if (role == QDialogButtonBox::ButtonRole::DestructiveRole)
    {
      m_delete_chosen = true;
      QDialog::accept();
    }
  });
}

void EditSymbolDialog::Accepted()
{
  const std::string name = m_name_edit->text().toStdString();

  if (*m_symbol_name != name)
    *m_symbol_name = name;

  bool good;
  const u32 size = m_size_hex_edit->text().toUInt(&good, 16);

  if (good && *m_symbol_size != size)
    *m_symbol_size = size;

  QDialog::accept();
}
