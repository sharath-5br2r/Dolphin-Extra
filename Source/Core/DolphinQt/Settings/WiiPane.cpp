// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/WiiPane.h"

#include <array>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "Core/USBUtils.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/USBDevicePicker.h"

#include "ui_WiiPane.h"

namespace
{
constexpr u64 MebibytesToBytes(u64 mebibytes)
{
  return mebibytes * 1024u * 1024u;
}

constexpr u64 GibibytesToBytes(u64 gibibytes)
{
  return MebibytesToBytes(gibibytes * 1024u);
}

constexpr std::array<u64, 11> SD_CARD_SIZES{
    0,
    MebibytesToBytes(64),
    MebibytesToBytes(128),
    MebibytesToBytes(256),
    MebibytesToBytes(512),
    GibibytesToBytes(1),
    GibibytesToBytes(2),
    GibibytesToBytes(4),
    GibibytesToBytes(8),
    GibibytesToBytes(16),
    GibibytesToBytes(32),
};

void CommitUserPath(QLineEdit* line_edit, const QString& path)
{
  line_edit->setText(path);
  line_edit->editingFinished();
}
}  // namespace

WiiPane::WiiPane(QWidget* parent) : QWidget(parent), m_ui(std::make_unique<Ui::WiiPane>())
{
  m_ui->setupUi(this);
  ConfigureWidgets();
  BindSettings();
  PopulateUSBPassthroughListWidget();
  ConnectWidgets();
  AddDescriptions();
  ValidateSelectionState();
  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

WiiPane::~WiiPane() = default;

void WiiPane::ConfigureWidgets()
{
  m_ui->sdPackButton->setText(tr(Common::SD_PACK_TEXT));
  m_ui->sdUnpackButton->setText(tr(Common::SD_UNPACK_TEXT));
}

void WiiPane::BindSettings()
{
  ConfigWidget::Bind(m_ui->pal60ModeCheckBox, Config::SYSCONF_PAL60);
  ConfigWidget::Bind(m_ui->screenSaverCheckBox, Config::SYSCONF_SCREENSAVER);
  ConfigWidget::Bind(m_ui->wiiLinkCheckBox, Config::MAIN_WII_WIILINK_ENABLE);
  ConfigWidget::Bind(m_ui->connectKeyboardCheckBox, Config::MAIN_WII_KEYBOARD);

  constexpr std::array aspect_ratio_values{false, true};
  ConfigWidget::BindMapped(m_ui->aspectRatioComboBox, Config::SYSCONF_WIDESCREEN,
                           std::span<const bool>{aspect_ratio_values});
  ConfigWidget::Bind(m_ui->systemLanguageComboBox, Config::SYSCONF_LANGUAGE);
  ConfigWidget::Bind(m_ui->soundModeComboBox, Config::SYSCONF_SOUND_MODE);

  ConfigWidget::Bind(m_ui->insertSdCardCheckBox, Config::MAIN_WII_SD_CARD);
  ConfigWidget::Bind(m_ui->allowSdWritesCheckBox, Config::MAIN_ALLOW_SD_WRITES);
  ConfigWidget::BindUserPath(m_ui->sdCardPathLineEdit, F_WIISDCARDIMAGE_IDX,
                             Config::MAIN_WII_SD_CARD_IMAGE_PATH);
  ConfigWidget::Bind(m_ui->syncSdFolderCheckBox, Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC);
  ConfigWidget::BindUserPath(m_ui->sdSyncFolderLineEdit, D_WIISDCARDSYNCFOLDER_IDX,
                             Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH);
  ConfigWidget::BindMapped(m_ui->sdCardSizeComboBox, Config::MAIN_WII_SD_CARD_FILESIZE,
                           std::span<const u64>{SD_CARD_SIZES});

  constexpr std::array<u32, 2> sensor_bar_position_values{1, 0};
  ConfigWidget::BindMapped(m_ui->sensorBarPositionComboBox, Config::SYSCONF_SENSOR_BAR_POSITION,
                           std::span<const u32>{sensor_bar_position_values});
  ConfigWidget::BindScaled(m_ui->irSensitivitySlider, Config::SYSCONF_SENSOR_BAR_SENSITIVITY, 1);
  ConfigWidget::BindScaled(m_ui->speakerVolumeSlider, Config::SYSCONF_SPEAKER_VOLUME, 1);
  ConfigWidget::Bind(m_ui->wiiRemoteRumbleCheckBox, Config::SYSCONF_WIIMOTE_MOTOR);

  ConfigWidget::MirrorFont(m_ui->aspectRatioLabel, m_ui->aspectRatioComboBox);
  ConfigWidget::MirrorFont(m_ui->systemLanguageLabel, m_ui->systemLanguageComboBox);
  ConfigWidget::MirrorFont(m_ui->soundModeLabel, m_ui->soundModeComboBox);
  ConfigWidget::MirrorFont(m_ui->sdCardPathLabel, m_ui->sdCardPathLineEdit);
  ConfigWidget::MirrorFont(m_ui->sdSyncFolderLabel, m_ui->sdSyncFolderLineEdit);
  ConfigWidget::MirrorFont(m_ui->sdCardSizeLabel, m_ui->sdCardSizeComboBox);
  ConfigWidget::MirrorFont(m_ui->sensorBarPositionLabel, m_ui->sensorBarPositionComboBox);
  ConfigWidget::MirrorFont(m_ui->irSensitivityLabel, m_ui->irSensitivitySlider);
  ConfigWidget::MirrorFont(m_ui->speakerVolumeLabel, m_ui->speakerVolumeSlider);
}

void WiiPane::ConnectWidgets()
{
  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &WiiPane::PopulateUSBPassthroughListWidget);
  connect(m_ui->usbPassthroughList, &QListWidget::itemClicked, this,
          &WiiPane::ValidateSelectionState);
  connect(m_ui->usbPassthroughAddButton, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistAddButton);
  connect(m_ui->usbPassthroughRemoveButton, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistRemoveButton);

  connect(m_ui->sdCardPathBrowseButton, &QPushButton::clicked, this, &WiiPane::BrowseSDRaw);
  connect(m_ui->sdSyncFolderBrowseButton, &QPushButton::clicked, this,
          &WiiPane::BrowseSDSyncFolder);

  connect(m_ui->sdPackButton, &QPushButton::clicked, this, [this] {
    const auto result = ModalMessageBox::warning(
        this, tr(Common::SD_PACK_TEXT),
        tr("You are about to pack the content of the folder at %1 into the file at %2. All "
           "current content of the file will be deleted. Are you sure you want to continue?")
            .arg(QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX)))
            .arg(QString::fromStdString(File::GetUserPath(F_WIISDCARDIMAGE_IDX))),
        QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes)
      return;

    ParallelProgressDialog progress_dialog(tr("Converting..."), tr("Cancel"), 0, 0, this);
    progress_dialog.GetRaw()->setWindowModality(Qt::WindowModal);
    progress_dialog.GetRaw()->setWindowTitle(tr("Progress"));
    auto success = std::async(std::launch::async, [&] {
      const bool good = Common::SyncSDFolderToSDImage(
          [&progress_dialog] { return progress_dialog.WasCanceled(); }, false);
      progress_dialog.Reset();
      return good;
    });
    progress_dialog.GetRaw()->exec();
    if (!success.get())
      ModalMessageBox::warning(this, tr(Common::SD_PACK_TEXT), tr("Conversion failed."));
  });

  connect(m_ui->sdUnpackButton, &QPushButton::clicked, this, [this] {
    const auto result = ModalMessageBox::warning(
        this, tr(Common::SD_UNPACK_TEXT),
        tr("You are about to unpack the content of the file at %2 into the folder at %1. All "
           "current content of the folder will be deleted. Are you sure you want to continue?")
            .arg(QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX)))
            .arg(QString::fromStdString(File::GetUserPath(F_WIISDCARDIMAGE_IDX))),
        QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes)
      return;

    ParallelProgressDialog progress_dialog(tr("Converting..."), tr("Cancel"), 0, 0, this);
    progress_dialog.GetRaw()->setWindowModality(Qt::WindowModal);
    progress_dialog.GetRaw()->setWindowTitle(tr("Progress"));
    auto success = std::async(std::launch::async, [&] {
      const bool good = Common::SyncSDImageToSDFolder(
          [&progress_dialog] { return progress_dialog.WasCanceled(); });
      progress_dialog.Reset();
      return good;
    });
    progress_dialog.GetRaw()->exec();
    if (!success.get())
      ModalMessageBox::warning(this, tr(Common::SD_UNPACK_TEXT), tr("Conversion failed."));
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
}

void WiiPane::AddDescriptions()
{
  ConfigWidget::SetDescription(m_ui->pal60ModeCheckBox, QString{},
                               tr("Sets the Wii display mode to 60Hz (480i) instead of 50Hz "
                                  "(576i) for PAL games.\nMay not work for all games."));
  ConfigWidget::SetDescription(m_ui->screenSaverCheckBox, QString{},
                               tr("Dims the screen after five minutes of inactivity."));
  ConfigWidget::SetDescription(
      m_ui->wiiLinkCheckBox, QString{},
      tr("Enables the WiiLink service for WiiConnect24 channels.\nWiiLink is an alternate provider "
         "for the discontinued WiiConnect24 Channels such as the Forecast and Nintendo Channels\n"
         "Read the Terms of Service at: https://www.wiilink24.com/tos"));
  ConfigWidget::SetDescription(m_ui->systemLanguageComboBox, QString{},
                               tr("Sets the Wii system language."));
  ConfigWidget::SetDescription(m_ui->connectKeyboardCheckBox, QString{},
                               tr("May cause slow down in Wii Menu and some games."));
  ConfigWidget::SetDescription(m_ui->insertSdCardCheckBox, QString{},
                               tr("Supports SD and SDHC. Default size is 128 MB."));
  ConfigWidget::SetDescription(
      m_ui->syncSdFolderCheckBox, QString{},
      tr("Synchronizes the SD Card with the SD Sync Folder when starting and ending emulation."));
}

void WiiPane::OnEmulationStateChanged(bool running)
{
  m_ui->screenSaverCheckBox->setEnabled(!running);
  m_ui->pal60ModeCheckBox->setEnabled(!running);
  m_ui->systemLanguageComboBox->setEnabled(!running);
  m_ui->aspectRatioComboBox->setEnabled(!running);
  m_ui->soundModeComboBox->setEnabled(!running);
  m_ui->sdPackButton->setEnabled(!running);
  m_ui->sdUnpackButton->setEnabled(!running);
  m_ui->wiiRemoteRumbleCheckBox->setEnabled(!running);
  m_ui->speakerVolumeSlider->setEnabled(!running);
  m_ui->irSensitivitySlider->setEnabled(!running);
  m_ui->sensorBarPositionComboBox->setEnabled(!running);
  m_ui->wiiLinkCheckBox->setEnabled(!running);
}

void WiiPane::ValidateSelectionState()
{
  m_ui->usbPassthroughRemoveButton->setEnabled(m_ui->usbPassthroughList->currentIndex().isValid());
}

void WiiPane::OnUSBWhitelistAddButton()
{
  auto whitelist = Config::GetUSBDeviceWhitelist();

  const std::optional<USBUtils::DeviceInfo> usb_device = USBDevicePicker::Run(
      this, tr("Add New USB Device"),
      [&whitelist](const USBUtils::DeviceInfo& device) { return !whitelist.contains(device); });
  if (!usb_device)
    return;

  if (whitelist.contains(*usb_device))
  {
    ModalMessageBox::critical(this, tr("USB Whitelist Error"),
                              tr("This USB device is already whitelisted."));
    return;
  }
  whitelist.emplace(*usb_device);
  Config::SetUSBDeviceWhitelist(whitelist);
  PopulateUSBPassthroughListWidget();
}

void WiiPane::OnUSBWhitelistRemoveButton()
{
  auto* const current_item = m_ui->usbPassthroughList->currentItem();
  if (!current_item)
    return;

  const QVariant item_data = current_item->data(Qt::UserRole);
  const USBUtils::DeviceInfo device = item_data.value<USBUtils::DeviceInfo>();

  auto whitelist = Config::GetUSBDeviceWhitelist();
  whitelist.erase(device);
  Config::SetUSBDeviceWhitelist(whitelist);
  PopulateUSBPassthroughListWidget();
}

void WiiPane::PopulateUSBPassthroughListWidget()
{
  m_ui->usbPassthroughList->clear();
  const auto whitelist = Config::GetUSBDeviceWhitelist();
  for (const auto& device : whitelist)
  {
    auto* const item = new QListWidgetItem(QString::fromStdString(device.ToDisplayString()),
                                           m_ui->usbPassthroughList);
    item->setData(Qt::UserRole, QVariant::fromValue(device));
  }
  ValidateSelectionState();
}

void WiiPane::BrowseSDRaw()
{
  const QString file = QDir::toNativeSeparators(DolphinFileDialog::getOpenFileName(
      this, tr("Select SD Card Image"),
      QString::fromStdString(Config::Get(Config::MAIN_WII_SD_CARD_IMAGE_PATH)),
      tr("SD Card Image (*.raw);;"
         "All Files (*)")));
  if (!file.isEmpty())
    CommitUserPath(m_ui->sdCardPathLineEdit, file);
}

void WiiPane::BrowseSDSyncFolder()
{
  const QString file = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select a Folder to Sync with the SD Card Image"),
      QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX))));
  if (!file.isEmpty())
    CommitUserPath(m_ui->sdSyncFolderLineEdit, file);
}
