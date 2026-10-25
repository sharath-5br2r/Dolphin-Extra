// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GCMemcardCreateNewDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QRadioButton>

#include "Common/FileUtil.h"
#include "Common/Timer.h"

#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/Sram.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"

#include "ui_GCMemcardCreateNewDialog.h"

GCMemcardCreateNewDialog::GCMemcardCreateNewDialog(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::GCMemcardCreateNewDialog>())
{
  m_ui->setupUi(this);
  m_ui->cardSizeComboBox->setItemData(0, 4);
  m_ui->cardSizeComboBox->setItemData(1, 8);
  m_ui->cardSizeComboBox->setItemData(2, 16);
  m_ui->cardSizeComboBox->setItemData(3, 32);
  m_ui->cardSizeComboBox->setItemData(4, 64);
  m_ui->cardSizeComboBox->setItemData(5, 128);
  m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create..."));

  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, [this] {
    if (CreateCard())
      accept();
  });
}

GCMemcardCreateNewDialog::~GCMemcardCreateNewDialog() = default;

bool GCMemcardCreateNewDialog::CreateCard()
{
  const u16 size = static_cast<u16>(m_ui->cardSizeComboBox->currentData().toInt());
  const bool is_shift_jis = m_ui->shiftJisRadioButton->isChecked();

  const QString path = DolphinFileDialog::getSaveFileName(
      this, tr("Create New Memory Card"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("GameCube Memory Cards (*.raw *.gcp)") + QStringLiteral(";;") + tr("All Files (*)"));

  if (path.isEmpty())
    return false;

  const CardFlashId flash_id{};
  const u32 rtc_bias = 0;
  const u32 sram_language = 0;
  const u64 format_time =
      Common::Timer::GetLocalTimeSinceJan1970() - ExpansionInterface::CEXIIPL::GC_EPOCH;

  const std::string p = path.toStdString();
  auto memcard = Memcard::GCMemcard::Create(p, flash_id, size, is_shift_jis, rtc_bias,
                                            sram_language, format_time);
  if (memcard && memcard->Save())
  {
    m_card_path = p;
    return true;
  }

  return false;
}

std::string GCMemcardCreateNewDialog::GetMemoryCardPath() const
{
  return m_card_path;
}
