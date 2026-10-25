// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/MemoryWidget.h"

#include <limits>
#include <optional>
#include <string>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSplitter>
#include <QTableWidget>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/AddressSpace.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/System.h"
#include "DolphinQt/Debugger/MemoryViewWidget.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

#include "ui_MemoryWidget.h"

using Type = MemoryViewWidget::Type;

MemoryWidget::MemoryWidget(Core::System& system, QWidget* parent)
    : QDockWidget(parent), m_system(system), m_ppc_symbol_db(system.GetPPCSymbolDB())
{
  setWindowTitle(tr("Memory"));
  setObjectName(QStringLiteral("memory"));

  setHidden(!Settings::Instance().IsMemoryVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  QSettings& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("memorywidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("memorywidget/floating")).toBool());
  m_splitter->restoreState(settings.value(QStringLiteral("memorywidget/splitter")).toByteArray());

  connect(&Settings::Instance(), &Settings::MemoryVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsMemoryVisible()); });

  connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    // Stop auto-update if MemoryView is tabbed out.
    if (visible && m_auto_update_enabled)
      RegisterAfterFrameEventCallback();
    else
      RemoveAfterFrameEventCallback();
  });
  LoadSettings();

  ConnectWidgets();
  OnAddressSpaceChanged();
  OnDisplayChanged();
}

MemoryWidget::~MemoryWidget()
{
  QSettings& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("memorywidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("memorywidget/floating"), isFloating());
  settings.setValue(QStringLiteral("memorywidget/splitter"), m_splitter->saveState());

  SaveSettings();
}

void MemoryWidget::CreateWidgets()
{
  auto* widget = new QWidget;
  Ui::MemoryWidget ui;
  ui.setupUi(widget);
  m_splitter = ui.splitter;
  m_search_address = ui.searchAddressCombo;
  m_search_offset = ui.searchOffsetEdit;
  m_data_edit = ui.dataEdit;
  m_base_check = ui.hexCheckBox;
  m_set_value = ui.setValueButton;
  m_data_preview = ui.dataPreviewLabel;
  m_input_combo = ui.inputTypeCombo;
  m_find_next = ui.findNextButton;
  m_find_previous = ui.findPreviousButton;
  m_result_label = ui.resultLabel;
  m_display_combo = ui.displayTypeCombo;
  m_align_combo = ui.alignmentCombo;
  m_row_length_combo = ui.rowLengthCombo;
  m_dual_check = ui.dualViewCheckBox;
  m_address_space_effective = ui.effectiveAddressRadio;
  m_address_space_auxiliary = ui.auxiliaryAddressRadio;
  m_address_space_physical = ui.physicalAddressRadio;
  m_bp_read_write = ui.readWriteRadio;
  m_bp_read_only = ui.readOnlyRadio;
  m_bp_write_only = ui.writeOnlyRadio;
  m_bp_log_check = ui.breakpointLogCheckBox;
  m_labels_group = ui.labelsGroup;
  m_search_labels = ui.labelsFilterEdit;
  m_note_list = ui.notesList;
  m_data_list = ui.dataList;
  m_symbols_list = ui.symbolsList;

  ui.addressSplitter->setCollapsible(0, false);
  ui.addressSplitter->setStretchFactor(1, 2);

  // Order here determines combo list order.
  m_input_combo->addItem(tr("Hex Byte String"), int(Type::HexString));
  m_input_combo->addItem(tr("ASCII"), int(Type::ASCII));
  m_input_combo->addItem(tr("Float"), int(Type::Float32));
  m_input_combo->addItem(tr("Double"), int(Type::Double));
  m_input_combo->addItem(tr("Unsigned 8"), int(Type::Unsigned8));
  m_input_combo->addItem(tr("Unsigned 16"), int(Type::Unsigned16));
  m_input_combo->addItem(tr("Unsigned 32"), int(Type::Unsigned32));
  m_input_combo->addItem(tr("Signed 8"), int(Type::Signed8));
  m_input_combo->addItem(tr("Signed 16"), int(Type::Signed16));
  m_input_combo->addItem(tr("Signed 32"), int(Type::Signed32));

  m_display_combo->addItem(tr("Hex 8"), int(Type::Hex8));
  m_display_combo->addItem(tr("Hex 16"), int(Type::Hex16));
  m_display_combo->addItem(tr("Hex 32"), int(Type::Hex32));
  m_display_combo->addItem(tr("Unsigned 8"), int(Type::Unsigned8));
  m_display_combo->addItem(tr("Unsigned 16"), int(Type::Unsigned16));
  m_display_combo->addItem(tr("Unsigned 32"), int(Type::Unsigned32));
  m_display_combo->addItem(tr("Signed 8"), int(Type::Signed8));
  m_display_combo->addItem(tr("Signed 16"), int(Type::Signed16));
  m_display_combo->addItem(tr("Signed 32"), int(Type::Signed32));
  m_display_combo->addItem(tr("ASCII"), int(Type::ASCII));
  m_display_combo->addItem(tr("Float"), int(Type::Float32));
  m_display_combo->addItem(tr("Double"), int(Type::Double));

  // i18n: "Fixed" here means that the alignment is always the same
  m_align_combo->addItem(tr("Fixed Alignment"));
  m_align_combo->addItem(tr("Type-based Alignment"), 0);
  m_align_combo->addItem(tr("No Alignment"), 1);

  m_row_length_combo->addItem(tr("4 Bytes"), 4);
  m_row_length_combo->addItem(tr("8 Bytes"), 8);
  m_row_length_combo->addItem(tr("16 Bytes"), 16);

  m_row_length_combo->setCurrentIndex(2);

  // Sidebar top menu
  QMenu* menu_views = ui.menuBar->addMenu(tr("&View"));

  QMenu* menu_import = ui.menuBar->addMenu(tr("&Import"));
  menu_import->addAction(tr("&Load file to current address"), this,
                         &MemoryWidget::OnSetValueFromFile);

  // View Menu
  auto* auto_update_action =
      menu_views->addAction(tr("&Auto update memory values"), this, [this](bool checked) {
        m_auto_update_enabled = checked;
        if (checked)
          RegisterAfterFrameEventCallback();
        else
          RemoveAfterFrameEventCallback();
      });
  auto_update_action->setCheckable(true);
  auto_update_action->setChecked(true);

  auto* highlight_update_action =
      // i18n: Highlight is a verb (this is the label of a checkbox)
      menu_views->addAction(tr("&Highlight recently changed values"), this,
                            [this](bool checked) { m_memory_view->ToggleHighlights(checked); });
  highlight_update_action->setCheckable(true);
  highlight_update_action->setChecked(true);

  // i18n: Highlight is a noun (clicking this lets you select a color)
  menu_views->addAction(tr("Highlight &color"), this,
                        [this] { m_memory_view->SetHighlightColor(); });

  auto* show_notes =
      menu_views->addAction(tr("&Show symbols and notes"), this, [this](bool checked) {
        m_labels_visible = checked;
        m_memory_view->ShowSymbols(checked);
        UpdateNotes();
      });
  show_notes->setCheckable(true);
  show_notes->setChecked(true);

  QMenu* menu_export = ui.menuBar->addMenu(tr("&Export"));
  menu_export->addAction(tr("Dump &MRAM"), this, &MemoryWidget::OnDumpMRAM);
  menu_export->addAction(tr("Dump &ExRAM"), this, &MemoryWidget::OnDumpExRAM);
  menu_export->addAction(tr("Dump &ARAM"), this, &MemoryWidget::OnDumpARAM);
  menu_export->addAction(tr("Dump &FakeVMEM"), this, &MemoryWidget::OnDumpFakeVMEM);

  m_memory_view = new MemoryViewWidget(m_system, this);
  ui.memoryViewLayout->addWidget(m_memory_view);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);

  setWidget(widget);
  UpdateNotes();
}

void MemoryWidget::ConnectWidgets()
{
  connect(m_search_address, &QComboBox::currentTextChanged, this, &MemoryWidget::OnSearchAddress);
  connect(m_search_offset, &QLineEdit::textChanged, this, &MemoryWidget::OnSearchAddress);
  connect(m_data_edit, &QLineEdit::textChanged, this, &MemoryWidget::ValidateAndPreviewInputValue);

  connect(m_input_combo, &QComboBox::currentIndexChanged, this,
          &MemoryWidget::ValidateAndPreviewInputValue);
  connect(m_set_value, &QPushButton::clicked, this, &MemoryWidget::OnSetValue);

  connect(m_find_next, &QPushButton::clicked, this, &MemoryWidget::OnFindNextValue);
  connect(m_find_previous, &QPushButton::clicked, this, &MemoryWidget::OnFindPreviousValue);

  for (auto* radio :
       {m_address_space_effective, m_address_space_auxiliary, m_address_space_physical})
  {
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnAddressSpaceChanged);
  }
  for (auto* combo : {m_display_combo, m_align_combo, m_row_length_combo})
  {
    connect(combo, &QComboBox::currentIndexChanged, this, &MemoryWidget::OnDisplayChanged);
  }

  connect(m_dual_check, &QCheckBox::toggled, this, &MemoryWidget::OnDisplayChanged);

  for (auto* radio : {m_bp_read_write, m_bp_read_only, m_bp_write_only})
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnBPTypeChanged);

  connect(m_base_check, &QCheckBox::toggled, this, &MemoryWidget::ValidateAndPreviewInputValue);
  connect(m_bp_log_check, &QCheckBox::toggled, this, &MemoryWidget::OnBPLogChanged);

  for (auto* list : {m_symbols_list, m_data_list, m_note_list})
    connect(list, &QListWidget::itemClicked, this, &MemoryWidget::OnSelectLabel);

  connect(Host::GetInstance(), &Host::PPCSymbolsChanged, this, &MemoryWidget::RefreshLabelBox);
  connect(m_search_labels, &QLineEdit::textChanged, this, &MemoryWidget::RefreshLabelBox);
  connect(m_memory_view, &MemoryViewWidget::ShowCode, this, &MemoryWidget::ShowCode);
  connect(m_memory_view, &MemoryViewWidget::RequestWatch, this, &MemoryWidget::RequestWatch);
  connect(m_memory_view, &MemoryViewWidget::ActivateSearch, this,
          &MemoryWidget::ActivateSearchAddress);
}

void MemoryWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetMemoryVisible(false);
  RemoveAfterFrameEventCallback();
}

void MemoryWidget::showEvent(QShowEvent* event)
{
  if (m_auto_update_enabled)
    RegisterAfterFrameEventCallback();

  Update();
}

void MemoryWidget::hideEvent(QHideEvent* event)
{
  RemoveAfterFrameEventCallback();
}

void MemoryWidget::RegisterAfterFrameEventCallback()
{
  m_vi_end_field_event =
      m_system.GetVideoEvents().vi_end_field_event.Register([this] { AutoUpdateTable(); });
}

void MemoryWidget::RemoveAfterFrameEventCallback()
{
  m_vi_end_field_event.reset();
}

void MemoryWidget::AutoUpdateTable()
{
  m_memory_view->UpdateOnFrameEnd();
}

void MemoryWidget::Update()
{
  if (!isVisible())
    return;

  m_memory_view->UpdateDispatcher(MemoryViewWidget::UpdateType::Addresses);
  update();
}

void MemoryWidget::LoadSettings()
{
  QSettings& settings = Settings::GetQSettings();

  const int combo_index = settings.value(QStringLiteral("memorywidget/inputcombo"), 1).toInt();

  m_input_combo->setCurrentIndex(combo_index);

  const bool address_space_effective =
      settings.value(QStringLiteral("memorywidget/addrspace_effective"), true).toBool();
  const bool address_space_auxiliary =
      settings.value(QStringLiteral("memorywidget/addrspace_auxiliary"), false).toBool();
  const bool address_space_physical =
      settings.value(QStringLiteral("memorywidget/addrspace_physical"), false).toBool();

  m_address_space_effective->setChecked(address_space_effective);
  m_address_space_auxiliary->setChecked(address_space_auxiliary);
  m_address_space_physical->setChecked(address_space_physical);

  const int display_index = settings.value(QStringLiteral("memorywidget/display_type"), 1).toInt();

  m_display_combo->setCurrentIndex(display_index);

  bool bp_rw = settings.value(QStringLiteral("memorywidget/bpreadwrite"), true).toBool();
  bool bp_r = settings.value(QStringLiteral("memorywidget/bpread"), false).toBool();
  bool bp_w = settings.value(QStringLiteral("memorywidget/bpwrite"), false).toBool();
  bool bp_log = settings.value(QStringLiteral("memorywidget/bplog"), true).toBool();

  if (bp_rw)
    m_memory_view->SetBPType(MemoryViewWidget::BPType::ReadWrite);
  else if (bp_r)
    m_memory_view->SetBPType(MemoryViewWidget::BPType::ReadOnly);
  else
    m_memory_view->SetBPType(MemoryViewWidget::BPType::WriteOnly);

  m_bp_read_write->setChecked(bp_rw);
  m_bp_read_only->setChecked(bp_r);
  m_bp_write_only->setChecked(bp_w);
  m_bp_log_check->setChecked(bp_log);
}

void MemoryWidget::SaveSettings()
{
  QSettings& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("memorywidget/inputcombo"), m_input_combo->currentIndex());

  settings.setValue(QStringLiteral("memorywidget/addrspace_effective"),
                    m_address_space_effective->isChecked());
  settings.setValue(QStringLiteral("memorywidget/addrspace_auxiliary"),
                    m_address_space_auxiliary->isChecked());
  settings.setValue(QStringLiteral("memorywidget/addrspace_physical"),
                    m_address_space_physical->isChecked());

  settings.setValue(QStringLiteral("memorywidget/display_type"), m_display_combo->currentIndex());

  settings.setValue(QStringLiteral("memorywidget/bpreadwrite"), m_bp_read_write->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bpread"), m_bp_read_only->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bpwrite"), m_bp_write_only->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bplog"), m_bp_log_check->isChecked());
}

void MemoryWidget::OnAddressSpaceChanged()
{
  AddressSpace::Type space;

  if (m_address_space_effective->isChecked())
    space = AddressSpace::Type::Effective;
  else if (m_address_space_auxiliary->isChecked())
    space = AddressSpace::Type::Auxiliary;
  else
    space = AddressSpace::Type::Physical;

  m_memory_view->SetAddressSpace(space);

  SaveSettings();
}

void MemoryWidget::OnDisplayChanged()
{
  const auto type = static_cast<Type>(m_display_combo->currentData().toInt());
  int bytes_per_row = m_row_length_combo->currentData().toInt();
  int alignment;
  bool dual_view = m_dual_check->isChecked();

  if (type == Type::Double && bytes_per_row == 4)
    bytes_per_row = 8;

  // Alignment: First (fixed) option equals bytes per row. 'currentData' is correct for other
  // options. Type-based must be calculated in memoryviewwidget and is left at 0. "No alignment" is
  // equivalent to a value of 1.
  if (m_align_combo->currentIndex() == 0)
    alignment = bytes_per_row;
  else
    alignment = m_align_combo->currentData().toInt();

  m_memory_view->SetDisplay(type, bytes_per_row, alignment, dual_view);

  SaveSettings();
}

void MemoryWidget::OnBPLogChanged()
{
  m_memory_view->SetBPLoggingEnabled(m_bp_log_check->isChecked());
  SaveSettings();
}

void MemoryWidget::OnBPTypeChanged()
{
  bool read_write = m_bp_read_write->isChecked();
  bool read_only = m_bp_read_only->isChecked();

  MemoryViewWidget::BPType type;

  if (read_write)
    type = MemoryViewWidget::BPType::ReadWrite;
  else if (read_only)
    type = MemoryViewWidget::BPType::ReadOnly;
  else
    type = MemoryViewWidget::BPType::WriteOnly;

  m_memory_view->SetBPType(type);

  SaveSettings();
}

void MemoryWidget::SetAddress(u32 address)
{
  // Store current and target address in combo box
  const QSignalBlocker blocker(m_search_address);
  const QString current_text = m_search_address->currentText();
  const QString target_addr = QString::number(address, 16);
  bool good;
  const u32 current_addr = current_text.toUInt(&good, 16);

  if (good)
  {
    AddressSpace::Accessors* accessors =
        AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

    const Core::CPUThreadGuard guard(m_system);
    good = accessors->IsValidAddress(guard, current_addr);
  }

  if (m_search_address->findText(current_text) == -1 && good)
    m_search_address->insertItem(0, current_text);

  if (m_search_address->findText(target_addr) == -1)
    m_search_address->insertItem(0, target_addr);

  m_search_address->setCurrentText(target_addr);

  m_memory_view->SetAddress(address);
  Settings::Instance().SetMemoryVisible(true);
  raise();

  m_memory_view->setFocus();
}

void MemoryWidget::ActivateSearchAddress()
{
  m_search_address->setFocus();
  m_search_address->lineEdit()->selectAll();
}

void MemoryWidget::OnSearchAddress()
{
  const auto target_addr = GetTargetAddress();

  if (target_addr.is_good_address && target_addr.is_good_offset)
    m_memory_view->SetAddress(target_addr.address);

  QFont addr_font, offset_font;
  QPalette addr_palette, offset_palette;

  if (!target_addr.is_good_address)
  {
    addr_font.setBold(true);
    addr_palette.setColor(QPalette::Text, Qt::red);
  }

  if (!target_addr.is_good_offset)
  {
    offset_font.setBold(true);
    offset_palette.setColor(QPalette::Text, Qt::red);
  }

  m_search_address->setFont(addr_font);
  m_search_address->setPalette(addr_palette);
  m_search_offset->setFont(offset_font);
  m_search_offset->setPalette(offset_palette);
}

void MemoryWidget::ValidateAndPreviewInputValue()
{
  m_data_preview->clear();
  QString input_text = m_data_edit->text();
  const auto input_type = static_cast<Type>(m_input_combo->currentData().toInt());

  m_base_check->setEnabled(input_type == Type::Unsigned32 || input_type == Type::Signed32 ||
                           input_type == Type::Unsigned16 || input_type == Type::Signed16 ||
                           input_type == Type::Unsigned8 || input_type == Type::Signed8);

  if (input_text.isEmpty())
    return;

  // Remove any spaces
  if (input_type != Type::ASCII)
    input_text.remove(QLatin1Char(' '));

  if (input_type == Type::HexString)
  {
    // Hex strings are parsed using QByteArray::fromHex which doesn't expect a 0x prefix. If the
    // user has inserted one, remove it.
    if (input_text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
      input_text.remove(0, 2);
  }
  else if (m_base_check->isChecked())
  {
    // Add 0x to the front of the input (after the '-', if present), but only if the user didn't
    // already do so.
    if (input_text.startsWith(QLatin1Char('-')))
    {
      if (!input_text.startsWith(QStringLiteral("-0x"), Qt::CaseInsensitive))
        input_text.insert(1, QStringLiteral("0x"));
    }
    else if (!input_text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
      input_text.prepend(QStringLiteral("0x"));
    }
  }

  QFont font;
  QPalette palette;
  std::vector<u8> bytes = m_memory_view->ConvertTextToBytes(input_type, input_text);

  if (!bytes.empty())
  {
    QString hex_string;
    std::string s;

    for (const u8 c : bytes)
      s.append(fmt::format("{:02x}", c));

    hex_string = QString::fromStdString(s);
    int output_length = hex_string.length();

    if (output_length > 16)
    {
      hex_string.truncate(16);
      output_length = hex_string.length();
      hex_string.append(QStringLiteral("..."));
    }

    for (int i = 2; i < output_length; i += 2)
      hex_string.insert(output_length - i, QLatin1Char{' '});

    m_data_preview->setText(hex_string);
  }
  else
  {
    font.setBold(true);
    palette.setColor(QPalette::Text, Qt::red);
  }

  m_data_edit->setFont(font);
  m_data_edit->setPalette(palette);
}

QByteArray MemoryWidget::GetInputData() const
{
  // Empty or invalid input data returns an empty array.
  if (m_data_preview->text().isEmpty())
    return QByteArray();

  const auto input_type = static_cast<Type>(m_input_combo->currentData().toInt());

  // Ascii might be truncated, pull from data edit box.
  if (input_type == Type::ASCII)
    return QByteArray(m_data_edit->text().toUtf8());

  // If we are doing a large array of hex bytes
  if (input_type == Type::HexString)
    return QByteArray::fromHex(m_data_edit->text().toUtf8());

  // Data preview has exactly what we want to input, so pull it from there.
  return QByteArray::fromHex(m_data_preview->text().toUtf8());
}

void MemoryWidget::OnSetValue()
{
  if (!Core::IsRunning(m_system))
    return;

  auto target_addr = GetTargetAddress();

  if (!target_addr.is_good_address)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad address provided."));
    return;
  }

  if (!target_addr.is_good_offset)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad offset provided."));
    return;
  }

  const QByteArray bytes = GetInputData();

  // Invalid input will give an empty array.
  if (bytes.isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad value provided."));
    return;
  }

  const Core::CPUThreadGuard guard(m_system);

  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());
  u32 end_address = target_addr.address + static_cast<u32>(bytes.size()) - 1;

  if (!accessors->IsValidAddress(guard, target_addr.address) ||
      !accessors->IsValidAddress(guard, end_address))
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Target address range is invalid."));
    return;
  }

  for (const char c : bytes)
    accessors->WriteU8(guard, target_addr.address++, static_cast<u8>(c));

  Update();
}

void MemoryWidget::OnSetValueFromFile()
{
  if (!Core::IsRunning(m_system))
    return;

  auto target_addr = GetTargetAddress();

  if (!target_addr.is_good_address)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad address provided."));
    return;
  }

  if (!target_addr.is_good_offset)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad offset provided."));
    return;
  }

  QString path = QFileDialog::getOpenFileName(this, tr("Select a file"), QDir::currentPath(),
                                              tr("All files (*)"));
  if (path.isNull())
  {
    return;
  }

  File::IOFile f(path.toStdString(), "rb");

  if (!f)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Unable to open file."));
    return;
  }

  const u64 file_length = f.GetSize();
  std::vector<u8> file_contents(file_length);
  if (!f.ReadBytes(file_contents.data(), file_length))
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Unable to read file."));
    return;
  }

  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

  const Core::CPUThreadGuard guard(m_system);

  for (u8 b : file_contents)
    accessors->WriteU8(guard, target_addr.address++, b);

  Update();
}

void MemoryWidget::RefreshLabelBox()
{
  if (!m_labels_visible || m_ppc_symbol_db.IsEmpty())
  {
    m_labels_group->hide();
    return;
  }

  m_labels_group->show();

  UpdateSymbols();
  UpdateNotes();
}

void MemoryWidget::OnSelectLabel()
{
  QList<QListWidgetItem*> items;
  if (m_note_list->isVisible())
    items = m_note_list->selectedItems();
  else if (m_symbols_list->isVisible())
    items = m_symbols_list->selectedItems();
  else if (m_data_list->isVisible())
    items = m_data_list->selectedItems();

  if (items.isEmpty())
    return;

  const u32 address = items[0]->data(Qt::UserRole).toUInt();

  SetAddress(address);
}

void MemoryWidget::UpdateSymbols()
{
  const QString selection = m_symbols_list->selectedItems().isEmpty() ?
                                QString{} :
                                m_symbols_list->selectedItems()[0]->text();
  m_symbols_list->clear();
  m_data_list->clear();

  m_ppc_symbol_db.ForEachSymbol([&](const Common::Symbol& symbol) {
    QString name = QString::fromStdString(symbol.name);

    // If the symbol has an object name, add it to the entry name.
    if (!symbol.object_name.empty())
    {
      name += QString::fromStdString(fmt::format(" ({})", symbol.object_name));
    }

    auto* item = new QListWidgetItem(name);
    if (name == selection)
      item->setSelected(true);

    item->setData(Qt::UserRole, symbol.address);

    if (!name.contains(m_search_labels->text(), Qt::CaseInsensitive))
      return;

    if (symbol.type != Common::Symbol::Type::Function)
      m_data_list->addItem(item);
    else
      m_symbols_list->addItem(item);
  });

  m_symbols_list->sortItems();
}

void MemoryWidget::UpdateNotes()
{
  // Save selection to re-apply.
  const QString selection = m_note_list->selectedItems().isEmpty() ?
                                QStringLiteral("") :
                                m_note_list->selectedItems()[0]->text();
  m_note_list->clear();

  m_ppc_symbol_db.ForEachNote([&](const Common::Note& note) {
    const QString name = QString::fromStdString(note.name);

    auto* item = new QListWidgetItem(name);
    if (name == selection)
      item->setSelected(true);

    item->setData(Qt::UserRole, note.address);

    // Filter notes based on the search text.
    if (name.contains(m_search_labels->text(), Qt::CaseInsensitive))
      m_note_list->addItem(item);
  });

  m_note_list->sortItems();
}

static void DumpArray(const std::string& filename, const u8* data, size_t length)
{
  if (!data)
    return;

  File::IOFile f(filename, "wb");

  if (!f)
  {
    ModalMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("Failed to dump %1: Can't open file").arg(QString::fromStdString(filename)));
    return;
  }

  if (!f.WriteBytes(data, length))
  {
    ModalMessageBox::critical(nullptr, QObject::tr("Error"),
                              QObject::tr("Failed to dump %1: Failed to write to file")
                                  .arg(QString::fromStdString(filename)));
  }
}

void MemoryWidget::OnDumpMRAM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Mem1);
  DumpArray(File::GetUserPath(F_MEM1DUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

void MemoryWidget::OnDumpExRAM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Mem2);
  DumpArray(File::GetUserPath(F_MEM2DUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

void MemoryWidget::OnDumpARAM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Auxiliary);
  DumpArray(File::GetUserPath(F_ARAMDUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

void MemoryWidget::OnDumpFakeVMEM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Fake);
  DumpArray(File::GetUserPath(F_FAKEVMEMDUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

MemoryWidget::TargetAddress MemoryWidget::GetTargetAddress() const
{
  TargetAddress target;

  // Returns 0 if conversion fails
  const u32 addr = m_search_address->currentText().toUInt(&target.is_good_address, 16);
  target.is_good_address |= m_search_address->currentText().isEmpty();
  const s32 offset = m_search_offset->text().toInt(&target.is_good_offset, 16);
  const u32 neg_offset = offset != std::numeric_limits<s32>::min() ?
                             -offset :
                             u32(std::numeric_limits<s32>::max()) + 1;
  target.is_good_offset |= m_search_offset->text().isEmpty();
  target.is_good_offset &= offset >= 0 || neg_offset <= addr;
  target.is_good_offset &= offset <= 0 || (std::numeric_limits<u32>::max() - u32(offset)) >= addr;

  if (!target.is_good_address || !target.is_good_offset)
    return target;

  if (offset < 0)
    target.address = addr - neg_offset;
  else
    target.address = addr + u32(offset);
  return target;
}

void MemoryWidget::FindValue(bool next)
{
  auto target_addr = GetTargetAddress();

  if (!target_addr.is_good_address)
  {
    m_result_label->setText(tr("Bad address provided."));
    return;
  }

  if (!target_addr.is_good_offset)
  {
    m_result_label->setText(tr("Bad offset provided."));
    return;
  }

  const QByteArray search_for = GetInputData();

  if (search_for.isEmpty())
  {
    m_result_label->setText(tr("Bad Value Given"));
    return;
  }

  if (!m_search_address->currentText().isEmpty())
  {
    // skip the quoted address so we don't potentially refind the last result
    target_addr.address += next ? 1 : -1;
  }

  const std::optional<u32> found_addr = [&] {
    AddressSpace::Accessors* accessors =
        AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

    const Core::CPUThreadGuard guard(m_system);
    return accessors->Search(guard, target_addr.address,
                             reinterpret_cast<const u8*>(search_for.data()),
                             static_cast<u32>(search_for.size()), next);
  }();

  if (found_addr.has_value())
  {
    m_result_label->setText(tr("Match Found"));

    u32 offset = *found_addr;

    m_search_address->setCurrentText(QStringLiteral("%1").arg(offset, 8, 16, QLatin1Char('0')));
    m_search_offset->clear();

    m_memory_view->SetAddress(offset);

    return;
  }

  m_result_label->setText(tr("No Match"));
}

void MemoryWidget::OnFindNextValue()
{
  FindValue(true);
}

void MemoryWidget::OnFindPreviousValue()
{
  FindValue(false);
}
