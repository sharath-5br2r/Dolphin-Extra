// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatSearchFactoryWidget.h"

#include <string>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

#include "Common/StringUtil.h"
#include "Core/CheatSearch.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include "ui_CheatSearchFactoryWidget.h"

CheatSearchFactoryWidget::CheatSearchFactoryWidget()
    : m_ui(std::make_unique<Ui::CheatSearchFactoryWidget>())
{
  CreateWidgets();
  ConnectWidgets();
  RefreshGui();
}

CheatSearchFactoryWidget::~CheatSearchFactoryWidget() = default;

Q_DECLARE_METATYPE(Cheats::DataType);

void CheatSearchFactoryWidget::CreateWidgets()
{
  m_ui->setupUi(this);

  m_ui->dataTypeComboBox->addItem(tr("8-bit Unsigned Integer"),
                                  QVariant::fromValue(Cheats::DataType::U8));
  m_ui->dataTypeComboBox->addItem(tr("16-bit Unsigned Integer"),
                                  QVariant::fromValue(Cheats::DataType::U16));
  m_ui->dataTypeComboBox->addItem(tr("32-bit Unsigned Integer"),
                                  QVariant::fromValue(Cheats::DataType::U32));
  m_ui->dataTypeComboBox->addItem(tr("64-bit Unsigned Integer"),
                                  QVariant::fromValue(Cheats::DataType::U64));
  m_ui->dataTypeComboBox->addItem(tr("8-bit Signed Integer"),
                                  QVariant::fromValue(Cheats::DataType::S8));
  m_ui->dataTypeComboBox->addItem(tr("16-bit Signed Integer"),
                                  QVariant::fromValue(Cheats::DataType::S16));
  m_ui->dataTypeComboBox->addItem(tr("32-bit Signed Integer"),
                                  QVariant::fromValue(Cheats::DataType::S32));
  m_ui->dataTypeComboBox->addItem(tr("64-bit Signed Integer"),
                                  QVariant::fromValue(Cheats::DataType::S64));
  m_ui->dataTypeComboBox->addItem(tr("32-bit Float"), QVariant::fromValue(Cheats::DataType::F32));
  m_ui->dataTypeComboBox->addItem(tr("64-bit Float"), QVariant::fromValue(Cheats::DataType::F64));
  m_ui->dataTypeComboBox->setCurrentIndex(6);
}

void CheatSearchFactoryWidget::ConnectWidgets()
{
  connect(m_ui->newSearchButton, &QPushButton::clicked, this,
          &CheatSearchFactoryWidget::OnNewSearchClicked);
  connect(m_ui->standardAddressSpaceRadioButton, &QPushButton::toggled, this,
          &CheatSearchFactoryWidget::OnAddressSpaceRadioChanged);
  connect(m_ui->customAddressSpaceRadioButton, &QRadioButton::toggled, this,
          &CheatSearchFactoryWidget::OnAddressSpaceRadioChanged);
}

void CheatSearchFactoryWidget::RefreshGui()
{
  const bool enable_custom = m_ui->customAddressSpaceRadioButton->isChecked();
  m_ui->customVirtualAddressSpaceRadioButton->setEnabled(enable_custom);
  m_ui->customPhysicalAddressSpaceRadioButton->setEnabled(enable_custom);
  m_ui->customEffectiveAddressSpaceRadioButton->setEnabled(enable_custom);
  m_ui->customAddressStartLineEdit->setEnabled(enable_custom);
  m_ui->customAddressEndLineEdit->setEnabled(enable_custom);
}

void CheatSearchFactoryWidget::OnAddressSpaceRadioChanged()
{
  RefreshGui();
}

void CheatSearchFactoryWidget::OnNewSearchClicked()
{
  std::vector<Cheats::MemoryRange> memory_ranges;
  PowerPC::RequestedAddressSpace address_space;
  if (m_ui->standardAddressSpaceRadioButton->isChecked())
  {
    auto& system = Core::System::GetInstance();
    if (!Core::IsRunning(system))
    {
      ModalMessageBox::warning(
          this, tr("No game running."),
          tr("Please start a game before starting a search with standard memory regions."));
      return;
    }

    auto& memory = system.GetMemory();
    memory_ranges.emplace_back(0x80000000, memory.GetRamSizeReal());
    if (system.IsWii())
      memory_ranges.emplace_back(0x90000000, memory.GetExRamSizeReal());
    address_space = PowerPC::RequestedAddressSpace::Virtual;
  }
  else
  {
    const std::string address_start_str = m_ui->customAddressStartLineEdit->text().toStdString();
    const std::string address_end_str = m_ui->customAddressEndLineEdit->text().toStdString();

    u64 address_start;
    u64 address_end;
    if (!TryParse(address_start_str, &address_start) || !TryParse(address_end_str, &address_end))
      return;
    if (address_end <= address_start || address_end > 0x1'0000'0000)
      return;

    memory_ranges.emplace_back(static_cast<u32>(address_start), address_end - address_start);

    if (m_ui->customVirtualAddressSpaceRadioButton->isChecked())
      address_space = PowerPC::RequestedAddressSpace::Virtual;
    else if (m_ui->customPhysicalAddressSpaceRadioButton->isChecked())
      address_space = PowerPC::RequestedAddressSpace::Physical;
    else
      address_space = PowerPC::RequestedAddressSpace::Effective;
  }

  const bool aligned = m_ui->alignedCheckBox->isChecked();
  const auto data_type = m_ui->dataTypeComboBox->currentData().value<Cheats::DataType>();
  auto session = Cheats::MakeSession(std::move(memory_ranges), address_space, aligned, data_type);
  if (session)
    emit NewSessionCreated(*session);
}
