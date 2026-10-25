// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NKitWarningDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "DolphinQt/Resources.h"

#include "ui_NKitWarningDialog.h"

bool NKitWarningDialog::ShowUnlessDisabled(QWidget* parent)
{
  if (Config::Get(Config::MAIN_SKIP_NKIT_WARNING))
    return true;

  NKitWarningDialog dialog(parent);
  return dialog.exec() == QDialog::Accepted;
}

NKitWarningDialog::NKitWarningDialog(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::NKitWarningDialog>())
{
  m_ui->setupUi(this);
  setWindowIcon(Resources::GetAppIcon());

  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
  m_ui->iconLabel->setPixmap(icon.pixmap(100));

  connect(m_ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(m_ui->acceptCheckBox, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState state) { m_ui->okButton->setEnabled(state == Qt::Checked); });
#else
  connect(m_ui->acceptCheckBox, &QCheckBox::stateChanged,
          [this](int state) { m_ui->okButton->setEnabled(state == Qt::Checked); });
#endif

  connect(this, &QDialog::accepted, [this] {
    Config::SetBase(Config::MAIN_SKIP_NKIT_WARNING, m_ui->skipCheckBox->isChecked());
  });
}

NKitWarningDialog::~NKitWarningDialog() = default;
