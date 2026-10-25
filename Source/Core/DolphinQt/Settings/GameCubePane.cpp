// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/GameCubePane.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "DolphinQt/Config/SettingsWindow.h"
#ifdef HAS_LIBMGBA
#include "Core/NetPlayServer.h"
#endif
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/GCMemcardManager.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/BroadbandAdapterSettingsDialog.h"
#include "DolphinQt/Settings/TriforcePane.h"

#include "ui_GameCubePane.h"

constexpr std::initializer_list<ExpansionInterface::Slot> GUI_SLOTS = {
    ExpansionInterface::Slot::A, ExpansionInterface::Slot::B, ExpansionInterface::Slot::SP1};

namespace
{
void SetUserPath(QLineEdit* line_edit, unsigned int dir_index,
                 const Config::Info<std::string>& setting, const QString& path)
{
  const std::string value = path.toStdString();
  line_edit->setText(path);
  File::SetUserPath(dir_index, value);
  Config::SetBaseOrCurrent(setting, value);
}

// The path a memory card slot uses for `device` (a raw memory card or a GCI folder), resolved for
// PAL. The paths of the other regions follow the same pattern, so if the PAL paths of two slots
// match, the others match too and vice versa.
std::string GetEUPathForDevice(ExpansionInterface::Slot slot,
                               ExpansionInterface::EXIDeviceType device)
{
  return device == ExpansionInterface::EXIDeviceType::MemoryCard ?
             Config::GetMemcardPath(slot, DiscIO::Region::PAL) :
             Config::GetGCIFolderPath(slot, DiscIO::Region::PAL);
}

// The other memory card slot, if it is set to `device` and that device uses `eu_path`. A path
// stored for a device type the slot isn't set to is not in use.
std::optional<ExpansionInterface::Slot> FindSlotUsingPath(ExpansionInterface::Slot slot,
                                                          ExpansionInterface::EXIDeviceType device,
                                                          const std::string& eu_path)
{
  for (ExpansionInterface::Slot other_slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    if (other_slot != slot && Config::Get(Config::GetInfoForEXIDevice(other_slot)) == device &&
        GetEUPathForDevice(other_slot, device) == eu_path)
    {
      return other_slot;
    }
  }
  return std::nullopt;
}

// In GameCubePane's translation context, where these messages have always been.
QString AlreadyUsedMessage(ExpansionInterface::EXIDeviceType device,
                           ExpansionInterface::Slot other_slot)
{
  const QString slot_name = QString::fromStdString(fmt::to_string(other_slot));
  if (device == ExpansionInterface::EXIDeviceType::MemoryCard)
  {
    return QCoreApplication::translate(
               "GameCubePane",
               "The same file can't be used in multiple slots; it is already used by %1.")
        .arg(slot_name);
  }
  return QCoreApplication::translate(
             "GameCubePane",
             "The same folder can't be used in multiple slots; it is already used by %1.")
      .arg(slot_name);
}
}  // namespace

GameCubePane::GameCubePane(MainWindow* main_window) : m_ui(std::make_unique<Ui::GameCubePane>())
{
  m_ui->setupUi(this);
  ConfigureWidgets();
  PopulateDeviceChoices();
  BindSettings();
  LoadSettings();
  ConnectWidgets();

  connect(this, &GameCubePane::ShowTriforceWindow, main_window, &MainWindow::ShowTriforceWindow);
}

GameCubePane::~GameCubePane() = default;

void GameCubePane::ConfigureWidgets()
{
  m_slot_combos[ExpansionInterface::Slot::A] = m_ui->slotAComboBox;
  m_slot_combos[ExpansionInterface::Slot::B] = m_ui->slotBComboBox;
  m_slot_combos[ExpansionInterface::Slot::SP1] = m_ui->sp1ComboBox;
  m_slot_buttons[ExpansionInterface::Slot::A] = m_ui->slotAConfigButton;
  m_slot_buttons[ExpansionInterface::Slot::B] = m_ui->slotBConfigButton;
  m_slot_buttons[ExpansionInterface::Slot::SP1] = m_ui->sp1ConfigButton;

  m_memcard_path_labels[ExpansionInterface::Slot::A] = m_ui->slotAMemcardPathLabel;
  m_memcard_path_labels[ExpansionInterface::Slot::B] = m_ui->slotBMemcardPathLabel;
  m_memcard_paths[ExpansionInterface::Slot::A] = m_ui->slotAMemcardPathLineEdit;
  m_memcard_paths[ExpansionInterface::Slot::B] = m_ui->slotBMemcardPathLineEdit;

  m_agp_path_labels[ExpansionInterface::Slot::A] = m_ui->slotAAgpPathLabel;
  m_agp_path_labels[ExpansionInterface::Slot::B] = m_ui->slotBAgpPathLabel;
  m_agp_paths[ExpansionInterface::Slot::A] = m_ui->slotAAgpPathLineEdit;
  m_agp_paths[ExpansionInterface::Slot::B] = m_ui->slotBAgpPathLineEdit;

  m_gci_override_labels[ExpansionInterface::Slot::A] = m_ui->slotAGciOverrideLabel;
  m_gci_override_labels[ExpansionInterface::Slot::B] = m_ui->slotBGciOverrideLabel;
  m_gci_path_labels[ExpansionInterface::Slot::A] = m_ui->slotAGciPathLabel;
  m_gci_path_labels[ExpansionInterface::Slot::B] = m_ui->slotBGciPathLabel;
  m_gci_paths[ExpansionInterface::Slot::A] = m_ui->slotAGciPathLineEdit;
  m_gci_paths[ExpansionInterface::Slot::B] = m_ui->slotBGciPathLineEdit;

  m_gba_rom_edits = {m_ui->gbaRom1LineEdit, m_ui->gbaRom2LineEdit, m_ui->gbaRom3LineEdit,
                     m_ui->gbaRom4LineEdit, m_ui->gbaRom5LineEdit};
  m_gba_browse_roms = {m_ui->gbaRom1BrowseButton, m_ui->gbaRom2BrowseButton,
                       m_ui->gbaRom3BrowseButton, m_ui->gbaRom4BrowseButton,
                       m_ui->gbaRom5BrowseButton};
  const std::array rom_labels{m_ui->gbaRom1Label, m_ui->gbaRom2Label, m_ui->gbaRom3Label,
                              m_ui->gbaRom4Label, m_ui->gbaRom5Label};
  for (size_t i = 0; i < rom_labels.size(); ++i)
  {
    rom_labels[i]->setText((i == Config::GBPLAYER_GBA_INDEX) ? tr("Game Boy Player ROM:") :
                                                               tr("Port %1 ROM:").arg(i + 1));
  }

#ifndef HAS_LIBMGBA
  m_ui->gbaGroup->hide();
#endif
}

void GameCubePane::BindSettings()
{
  ConfigWidget::Bind(m_ui->skipMainMenuCheckBox, Config::MAIN_SKIP_IPL);
  ConfigWidget::Bind(m_ui->systemLanguageComboBox, Config::MAIN_GC_LANGUAGE);

#ifdef HAS_LIBMGBA
  ConfigWidget::BindUserPath(m_ui->gbaBiosLineEdit, F_GBABIOS_IDX, Config::MAIN_GBA_BIOS_PATH);
  for (size_t i = 0; i < m_gba_rom_edits.size(); ++i)
    ConfigWidget::Bind(m_gba_rom_edits[i], Config::MAIN_GBA_ROM_PATHS[i]);
  ConfigWidget::Bind(m_ui->gbaSaveInRomPathCheckBox, Config::MAIN_GBA_SAVES_IN_ROM_PATH);
  ConfigWidget::BindUserPath(m_ui->gbaSavesLineEdit, D_GBASAVES_IDX, Config::MAIN_GBA_SAVES_PATH);
  SaveRomPathChanged();
#endif
}

void GameCubePane::PopulateDeviceChoices()
{
  using ExpansionInterface::EXIDeviceType;

  for (const auto device : {EXIDeviceType::None, EXIDeviceType::Dummy, EXIDeviceType::MemoryCard,
                            EXIDeviceType::MemoryCardFolder, EXIDeviceType::Gecko,
                            EXIDeviceType::AGP, EXIDeviceType::Microphone})
  {
    const QString name = tr(fmt::format("{:n}", device).c_str());
    const int value = static_cast<int>(device);
    m_slot_combos[ExpansionInterface::Slot::A]->addItem(name, value);
    m_slot_combos[ExpansionInterface::Slot::B]->addItem(name, value);
  }

  // Add SP1 devices
  for (const auto device : {
           EXIDeviceType::None,
           EXIDeviceType::Dummy,
           EXIDeviceType::Ethernet,
           EXIDeviceType::EthernetXLink,
           EXIDeviceType::EthernetTapServer,
           EXIDeviceType::EthernetBuiltIn,
#ifdef HAVE_CPPIPC
           EXIDeviceType::EthernetIPC,
#endif
           EXIDeviceType::ModemTapServer,
           EXIDeviceType::Baseboard,
       })
  {
    m_slot_combos[ExpansionInterface::Slot::SP1]->addItem(tr(fmt::format("{:n}", device).c_str()),
                                                          static_cast<int>(device));
  }
}

void GameCubePane::ConnectWidgets()
{
  // Device Settings
  for (ExpansionInterface::Slot slot : GUI_SLOTS)
  {
    connect(m_slot_combos[slot], &QComboBox::currentIndexChanged, this,
            [this, slot] { UpdateButton(slot); });
    connect(m_slot_combos[slot], &QComboBox::currentIndexChanged, this,
            &GameCubePane::SaveSettings);
    connect(m_slot_buttons[slot], &QPushButton::clicked, [this, slot] { OnConfigPressed(slot); });
  }

  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    connect(m_memcard_paths[slot], &QLineEdit::editingFinished, [this, slot] {
      // revert path change on failure
      if (!SetMemcard(slot, m_memcard_paths[slot]->text()))
        LoadSettings();
    });
    connect(m_agp_paths[slot], &QLineEdit::editingFinished,
            [this, slot] { SetAGPRom(slot, m_agp_paths[slot]->text()); });
    connect(m_gci_paths[slot], &QLineEdit::editingFinished, [this, slot] {
      // revert path change on failure
      if (!SetGCIFolder(slot, m_gci_paths[slot]->text()))
        LoadSettings();
    });
  }

#ifdef HAS_LIBMGBA
  // GBA Settings
  connect(m_ui->gbaBiosBrowseButton, &QPushButton::clicked, this, &GameCubePane::BrowseGBABios);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(m_ui->gbaSaveInRomPathCheckBox, &QCheckBox::checkStateChanged, this,
          &GameCubePane::SaveRomPathChanged);
#else
  connect(m_ui->gbaSaveInRomPathCheckBox, &QCheckBox::stateChanged, this,
          &GameCubePane::SaveRomPathChanged);
#endif
  connect(m_ui->gbaSavesBrowseButton, &QPushButton::clicked, this, &GameCubePane::BrowseGBASaves);
  for (size_t i = 0; i < m_gba_browse_roms.size(); ++i)
  {
    connect(m_gba_browse_roms[i], &QPushButton::clicked, this, [this, i] { BrowseGBARom(i); });
  }
#endif

  // Emulation State
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &GameCubePane::OnEmulationStateChanged);
  OnEmulationStateChanged();
}

void GameCubePane::OnEmulationStateChanged()
{
#ifdef HAS_LIBMGBA
  const bool gba_enabled = !NetPlay::IsNetPlayRunning();
  m_ui->gbaBiosLineEdit->setEnabled(gba_enabled);
  m_ui->gbaBiosBrowseButton->setEnabled(gba_enabled);
  m_ui->gbaSaveInRomPathCheckBox->setEnabled(gba_enabled);
  const bool saves_enabled = gba_enabled && !m_ui->gbaSaveInRomPathCheckBox->isChecked();
  m_ui->gbaSavesLineEdit->setEnabled(saves_enabled);
  m_ui->gbaSavesBrowseButton->setEnabled(saves_enabled);
  for (size_t i = 0; i < m_gba_browse_roms.size(); ++i)
  {
    m_gba_rom_edits[i]->setEnabled(gba_enabled);
    m_gba_browse_roms[i]->setEnabled(gba_enabled);
  }
#endif
}

void GameCubePane::UpdateButton(ExpansionInterface::Slot slot)
{
  const auto device =
      static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());
  bool has_config = false;

  switch (slot)
  {
  case ExpansionInterface::Slot::A:
  case ExpansionInterface::Slot::B:
  {
    has_config = (device == ExpansionInterface::EXIDeviceType::MemoryCard ||
                  device == ExpansionInterface::EXIDeviceType::MemoryCardFolder ||
                  device == ExpansionInterface::EXIDeviceType::AGP ||
                  device == ExpansionInterface::EXIDeviceType::Microphone);
    const bool hide_memory_card = device != ExpansionInterface::EXIDeviceType::MemoryCard ||
                                  Config::IsDefaultMemcardPathConfigured(slot);
    const bool hide_gci_path = device != ExpansionInterface::EXIDeviceType::MemoryCardFolder ||
                               Config::IsDefaultGCIFolderPathConfigured(slot);
    m_memcard_path_labels[slot]->setHidden(hide_memory_card);
    m_memcard_paths[slot]->setHidden(hide_memory_card);
    m_agp_path_labels[slot]->setHidden(device != ExpansionInterface::EXIDeviceType::AGP);
    m_agp_paths[slot]->setHidden(device != ExpansionInterface::EXIDeviceType::AGP);
    m_gci_path_labels[slot]->setHidden(hide_gci_path);
    m_gci_paths[slot]->setHidden(hide_gci_path);

    // In the years before we introduced the GCI folder configuration paths it has become somewhat
    // popular to use the GCI override path instead. Check if this is the case and display a warning
    // if it is set, so users aren't confused why configuring the normal GCI path doesn't do
    // anything.
    m_gci_override_labels[slot]->setHidden(
        device != ExpansionInterface::EXIDeviceType::MemoryCardFolder ||
        Config::Get(Config::GetInfoForGCIPathOverride(slot)).empty());

    break;
  }
  case ExpansionInterface::Slot::SP1:
    has_config = device == ExpansionInterface::EXIDeviceType::Ethernet ||
                 device == ExpansionInterface::EXIDeviceType::EthernetXLink ||
                 device == ExpansionInterface::EXIDeviceType::EthernetTapServer ||
                 device == ExpansionInterface::EXIDeviceType::EthernetBuiltIn ||
                 device == ExpansionInterface::EXIDeviceType::ModemTapServer ||
                 device == ExpansionInterface::EXIDeviceType::Baseboard;
    break;
  case ExpansionInterface::Slot::SP2:
    has_config = false;
    break;
  }

  m_slot_buttons[slot]->setEnabled(has_config);
}

void GameCubePane::OnConfigPressed(ExpansionInterface::Slot slot)
{
  const ExpansionInterface::EXIDeviceType device =
      static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());

  switch (device)
  {
  case ExpansionInterface::EXIDeviceType::MemoryCard:
    BrowseMemcard(slot);
    return;
  case ExpansionInterface::EXIDeviceType::MemoryCardFolder:
    BrowseGCIFolder(slot);
    return;
  case ExpansionInterface::EXIDeviceType::AGP:
    BrowseAGPRom(slot);
    return;
  case ExpansionInterface::EXIDeviceType::Microphone:
  {
    // TODO: convert MappingWindow to use Slot?
    MappingWindow dialog(this, MappingWindow::Type::MAPPING_GC_MICROPHONE, static_cast<int>(slot));
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::Ethernet:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::Ethernet);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::EthernetXLink:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::XLinkKai);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::EthernetTapServer:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::TapServer);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::ModemTapServer:
  {
    BroadbandAdapterSettingsDialog dialog(this,
                                          BroadbandAdapterSettingsDialog::Type::ModemTapServer);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::EthernetBuiltIn:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::BuiltIn);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::Baseboard:
  {
    ShowTriforceWindow();
    return;
  }
  default:
    PanicAlertFmt("Unknown settings pressed for {}", device);
    return;
  }
}

void GameCubePane::BrowseMemcard(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));

  const QString filename = DolphinFileDialog::getSaveFileName(
      this, tr("Choose a File to Open or Create"),
      QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("GameCube Memory Cards (*.raw *.gcp)"), nullptr, QFileDialog::DontConfirmOverwrite);

  if (!filename.isEmpty())
    SetMemcard(slot, filename);
}

bool GameCubePane::SetMemcard(ExpansionInterface::Slot slot, const QString& filename)
{
  if (filename.isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Cannot set memory card to an empty path."));
    return false;
  }

  const std::string raw_path =
      WithUnifiedPathSeparators(QFileInfo(filename).absoluteFilePath().toStdString());

  // Figure out if the user selected a card that has a valid region specifier in the filename.
  const std::string jp_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::NTSC_J);
  const std::string us_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::NTSC_U);
  const std::string eu_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::PAL);
  const std::string dv_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::DEV);
  const bool raw_path_valid =
      raw_path == jp_path || raw_path == us_path || raw_path == eu_path || raw_path == dv_path;

  if (!raw_path_valid)
  {
    // TODO: We could try to autodetect the card region here and offer automatic renaming.
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("The filename %1 does not conform to Dolphin's region code format "
           "for memory cards. Please rename this file to either %2, %3, %4, or "
           "%5, matching the region of the save files that are on it.")
            .arg(QString::fromStdString(PathToFileName(raw_path)))
            .arg(QString::fromStdString(PathToFileName(us_path)))
            .arg(QString::fromStdString(PathToFileName(eu_path)))
            .arg(QString::fromStdString(PathToFileName(jp_path)))
            .arg(QString::fromStdString(PathToFileName(dv_path))));
    return false;
  }

  // Memcard validity checks
  for (const std::string& path : {jp_path, us_path, eu_path})
  {
    if (File::Exists(path))
    {
      auto [error_code, mc] = Memcard::GCMemcard::Open(path);

      if (error_code.HasCriticalErrors() || !mc || !mc->IsValid())
      {
        ModalMessageBox::critical(
            this, tr("Error"),
            tr("The file\n%1\nis either corrupted or not a GameCube memory card file.\n%2")
                .arg(QString::fromStdString(path))
                .arg(GCMemcardManager::GetErrorMessagesForErrorCode(error_code)));
        return false;
      }
    }
  }

  // Refuse a card the other slot is using. A card path the other slot keeps while it is set to
  // another device doesn't count; SaveSettings checks it when that slot becomes a memory card.
  if (const auto other_slot =
          FindSlotUsingPath(slot, ExpansionInterface::EXIDeviceType::MemoryCard, eu_path))
  {
    ModalMessageBox::critical(
        this, tr("Error"),
        AlreadyUsedMessage(ExpansionInterface::EXIDeviceType::MemoryCard, *other_slot));
    return false;
  }

  const std::string old_eu_path = Config::GetMemcardPath(slot, DiscIO::Region::PAL);
  Config::SetBase(Config::GetInfoForMemcardPath(slot), raw_path);

  auto& system = Core::System::GetInstance();
  if (Core::IsRunning(system))
  {
    // If emulation is running and the new card is different from the old one, notify the system to
    // eject the old and insert the new card.
    // TODO: This should probably done by a config change callback instead.
    if (eu_path != old_eu_path)
    {
      // ChangeDevice unplugs the device for 1 second, which means that games should notice that
      // the path has changed and thus the memory card contents have changed
      system.GetExpansionInterface().ChangeDevice(slot,
                                                  ExpansionInterface::EXIDeviceType::MemoryCard);
    }
  }

  LoadSettings();
  return true;
}

void GameCubePane::BrowseGCIFolder(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));

  const QString path = DolphinFileDialog::getExistingDirectory(
      this, tr("Choose GCI Base Folder"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)));

  if (!path.isEmpty())
    SetGCIFolder(slot, path);
}

bool GameCubePane::SetGCIFolder(ExpansionInterface::Slot slot, const QString& path)
{
  if (path.isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Cannot set GCI folder to an empty path."));
    return false;
  }

  std::string raw_path =
      WithUnifiedPathSeparators(QFileInfo(path).absoluteFilePath().toStdString());
  while (raw_path.ends_with('/'))
    raw_path.pop_back();

  // The user might be attempting to reset this path to its default, check for this.
  const std::string default_jp_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::NTSC_J);
  const std::string default_us_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::NTSC_U);
  const std::string default_eu_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::PAL);
  const std::string default_dv_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::DEV);
  const bool is_default_path = raw_path == default_jp_path || raw_path == default_us_path ||
                               raw_path == default_eu_path || raw_path == default_dv_path;

  bool path_changed;
  if (is_default_path)
  {
    // Reset to default.
    // Note that this does not need to check if the same card is in the other slot, because that's
    // impossible given our constraints for folder names.
    raw_path = "";
    path_changed = !Config::IsDefaultGCIFolderPathConfigured(slot);
  }
  else
  {
    // Figure out if the user selected a folder that ends in a valid region specifier.
    const std::string jp_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::NTSC_J);
    const std::string us_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::NTSC_U);
    const std::string eu_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::PAL);
    const std::string dv_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::DEV);
    const bool raw_path_valid =
        raw_path == jp_path || raw_path == us_path || raw_path == eu_path || raw_path == dv_path;

    if (!raw_path_valid)
    {
      // TODO: We could try to autodetect the card region here and offer automatic renaming.
      ModalMessageBox::critical(
          this, tr("Error"),
          tr("The folder %1 does not conform to Dolphin's region code format "
             "for GCI folders. Please rename this folder to either %2, %3, %4, or "
             "%5, matching the region of the save files that are in it.")
              .arg(QString::fromStdString(PathToFileName(raw_path)))
              .arg(QString::fromStdString(PathToFileName(us_path)))
              .arg(QString::fromStdString(PathToFileName(eu_path)))
              .arg(QString::fromStdString(PathToFileName(jp_path)))
              .arg(QString::fromStdString(PathToFileName(dv_path))));
      return false;
    }

    // Refuse a folder the other slot is using. A folder path the other slot keeps while it is set
    // to another device doesn't count; SaveSettings checks it when that slot becomes a GCI folder.
    if (const auto other_slot =
            FindSlotUsingPath(slot, ExpansionInterface::EXIDeviceType::MemoryCardFolder, eu_path))
    {
      ModalMessageBox::critical(
          this, tr("Error"),
          AlreadyUsedMessage(ExpansionInterface::EXIDeviceType::MemoryCardFolder, *other_slot));
      return false;
    }

    path_changed = eu_path != Config::GetGCIFolderPath(slot, DiscIO::Region::PAL);
  }

  Config::SetBase(Config::GetInfoForGCIPath(slot), raw_path);

  auto& system = Core::System::GetInstance();
  if (Core::IsRunning(system))
  {
    // If emulation is running and the new card is different from the old one, notify the system to
    // eject the old and insert the new card.
    // TODO: This should probably be done by a config change callback instead.
    if (path_changed)
    {
      // ChangeDevice unplugs the device for 1 second, which means that games should notice that
      // the path has changed and thus the memory card contents have changed
      system.GetExpansionInterface().ChangeDevice(
          slot, ExpansionInterface::EXIDeviceType::MemoryCardFolder);
    }
  }

  LoadSettings();
  return true;
}

void GameCubePane::BrowseAGPRom(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));

  QString filename = DolphinFileDialog::getSaveFileName(
      this, tr("Choose a File to Open"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("Game Boy Advance Carts (*.gba)"), nullptr, QFileDialog::DontConfirmOverwrite);

  if (!filename.isEmpty())
    SetAGPRom(slot, filename);
}

void GameCubePane::SetAGPRom(ExpansionInterface::Slot slot, const QString& filename)
{
  QString path_abs = filename.isEmpty() ? QString() : QFileInfo(filename).absoluteFilePath();

  QString path_old =
      QFileInfo(QString::fromStdString(Config::Get(Config::GetInfoForAGPCartPath(slot))))
          .absoluteFilePath();

  Config::SetBase(Config::GetInfoForAGPCartPath(slot), path_abs.toStdString());

  auto& system = Core::System::GetInstance();
  if (Core::IsRunning(system) && path_abs != path_old)
  {
    // ChangeDevice unplugs the device for 1 second.  For an actual AGP, you can remove the
    // cartridge without unplugging it, and it's not clear if the AGP software actually notices
    // that it's been unplugged or the cartridge has changed, but this was done for memcards so
    // we might as well do it for the AGP too.
    system.GetExpansionInterface().ChangeDevice(slot, ExpansionInterface::EXIDeviceType::AGP);
  }

  LoadSettings();
}

#ifdef HAS_LIBMGBA

void GameCubePane::BrowseGBABios()
{
  QString file = QDir::toNativeSeparators(DolphinFileDialog::getOpenFileName(
      this, tr("Select GBA BIOS"), QString::fromStdString(Config::Get(Config::MAIN_GBA_BIOS_PATH)),
      tr("All Files (*)")));
  if (!file.isEmpty())
    SetUserPath(m_ui->gbaBiosLineEdit, F_GBABIOS_IDX, Config::MAIN_GBA_BIOS_PATH, file);
}

void GameCubePane::BrowseGBARom(size_t index)
{
  QString file = QString::fromStdString(GetOpenGBARom({}));
  if (!file.isEmpty())
  {
    m_gba_rom_edits[index]->setText(file);
    Config::SetBaseOrCurrent(Config::MAIN_GBA_ROM_PATHS[index], file.toStdString());
  }
}

void GameCubePane::SaveRomPathChanged()
{
  const bool saves_enabled =
      !NetPlay::IsNetPlayRunning() && !m_ui->gbaSaveInRomPathCheckBox->isChecked();
  m_ui->gbaSavesLineEdit->setEnabled(saves_enabled);
  m_ui->gbaSavesBrowseButton->setEnabled(saves_enabled);
}

void GameCubePane::BrowseGBASaves()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select GBA Saves Path"),
      QString::fromStdString(Config::Get(Config::MAIN_GBA_SAVES_PATH))));
  if (!dir.isEmpty())
  {
    SetUserPath(m_ui->gbaSavesLineEdit, D_GBASAVES_IDX, Config::MAIN_GBA_SAVES_PATH, dir);
    SaveSettings();
  }
}

#endif  // HAS_LIBMGBA

void GameCubePane::LoadSettings()
{
  bool have_menu = false;

  for (const std::string dir : {USA_DIR, JAP_DIR, EUR_DIR})
  {
    const auto path = DIR_SEP + dir + DIR_SEP GC_IPL;
    if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + path) ||
        File::Exists(File::GetSysDirectory() + GC_SYS_DIR + path))
    {
      have_menu = true;
      break;
    }
  }

  m_ui->skipMainMenuCheckBox->setEnabled(have_menu || !m_ui->skipMainMenuCheckBox->isChecked());
  ConfigWidget::SetDescription(m_ui->skipMainMenuCheckBox, QString{},
                               have_menu ? QString{} : tr("Put IPL ROMs in User/GC/<region>."));

  // Device Settings
  for (ExpansionInterface::Slot slot : GUI_SLOTS)
  {
    const ExpansionInterface::EXIDeviceType exi_device =
        Config::Get(Config::GetInfoForEXIDevice(slot));
    SignalBlocking(m_slot_combos[slot])
        ->setCurrentIndex(m_slot_combos[slot]->findData(static_cast<int>(exi_device)));
    UpdateButton(slot);
  }

  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    SignalBlocking(m_memcard_paths[slot])
        ->setText(QString::fromStdString(Config::GetMemcardPath(slot, std::nullopt)));
    SignalBlocking(m_agp_paths[slot])
        ->setText(QString::fromStdString(Config::Get(Config::GetInfoForAGPCartPath(slot))));
    SignalBlocking(m_gci_paths[slot])
        ->setText(QString::fromStdString(Config::GetGCIFolderPath(slot, std::nullopt)));
  }
}

void GameCubePane::SaveSettings()
{
  // A slot switched to a memory card or GCI folder uses the path configured for that device: the
  // default, since a slot keeps only the path of its current device (below), unless an older
  // configuration kept one. Refuse the switch if the other slot is using the same card or folder.
  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    const auto dev =
        static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());
    if (dev == Config::Get(Config::GetInfoForEXIDevice(slot)) ||
        (dev != ExpansionInterface::EXIDeviceType::MemoryCard &&
         dev != ExpansionInterface::EXIDeviceType::MemoryCardFolder))
    {
      continue;
    }
    if (const auto other_slot = FindSlotUsingPath(slot, dev, GetEUPathForDevice(slot, dev)))
    {
      ModalMessageBox::critical(this, tr("Error"), AlreadyUsedMessage(dev, *other_slot));
      LoadSettings();
      return;
    }
  }

  Config::ConfigChangeCallbackGuard config_guard;

  auto& system = Core::System::GetInstance();
  // Device Settings
  for (ExpansionInterface::Slot slot : GUI_SLOTS)
  {
    const auto dev =
        static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());
    const ExpansionInterface::EXIDeviceType current_exi_device =
        Config::Get(Config::GetInfoForEXIDevice(slot));

    if (Core::IsRunning(system) && current_exi_device != dev)
    {
      system.GetExpansionInterface().ChangeDevice(slot, dev);
    }

    Config::SetBaseOrCurrent(Config::GetInfoForEXIDevice(slot), dev);

    // A memory card slot keeps only the path of the device it is set to: its memory card file,
    // GCI folder or AGP cartridge ROM. The others would stay configured but hidden, still taking
    // up their card or folder, and come back when the slot is switched to that device again.
    // Skipped when the device comes from a game's settings, so the user's own paths are kept.
    if (ExpansionInterface::IsMemcardSlot(slot) &&
        Config::GetActiveLayerForConfig(Config::GetInfoForEXIDevice(slot)) ==
            Config::LayerType::Base)
    {
      if (dev != ExpansionInterface::EXIDeviceType::MemoryCard)
        Config::SetBase(Config::GetInfoForMemcardPath(slot), "");
      if (dev != ExpansionInterface::EXIDeviceType::MemoryCardFolder)
        Config::SetBase(Config::GetInfoForGCIPath(slot), "");
      if (dev != ExpansionInterface::EXIDeviceType::AGP)
        Config::SetBase(Config::GetInfoForAGPCartPath(slot), "");
    }
  }

#ifdef HAS_LIBMGBA
  // GBA Settings
  if (!NetPlay::IsNetPlayRunning())
  {
    auto server = Settings::Instance().GetNetPlayServer();
    if (server)
      server->SetGBAConfig(server->GetGBAConfig(), true);
  }
#endif

  LoadSettings();
}

std::string GameCubePane::GetOpenGBARom(std::string_view title)
{
  QString caption = tr("Select GBA ROM");
  if (!title.empty())
    caption += QStringLiteral(": %1").arg(QString::fromStdString(std::string(title)));
  return QDir::toNativeSeparators(
             DolphinFileDialog::getOpenFileName(
                 nullptr, caption, QString(),
                 tr("Game Boy Advance ROMs (*.gba *.gbc *.gb *.7z *.zip *.agb *.mb *.rom *.bin);;"
                    "All Files (*)")))
      .toStdString();
}
