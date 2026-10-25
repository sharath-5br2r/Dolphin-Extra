// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/PathPane.h"

#include <memory>
#include <string>

#include <QCheckBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Settings.h"

#include "ui_PathPane.h"

namespace
{
void SetBoundPath(QLineEdit* line_edit, const Config::Info<std::string>& setting,
                  const QString& path)
{
  line_edit->setText(path);
  Config::SetBaseOrCurrent(setting, path.toStdString());
}

void SetBasePath(QLineEdit* line_edit, const Config::Info<std::string>& setting,
                 const QString& path)
{
  line_edit->setText(path);
  Config::SetBase(setting, path.toStdString());
}
}  // namespace

PathPane::PathPane(QWidget* parent) : QWidget(parent), m_ui(std::make_unique<Ui::PathPane>())
{
  m_ui->setupUi(this);
  PopulatePaths();
  BindSettings();
  ConnectWidgets();
}

PathPane::~PathPane() = default;

void PathPane::PopulatePaths()
{
  m_ui->pathListWidget->insertItems(0, Settings::Instance().GetPaths());
  m_ui->autoRefreshCheckBox->setChecked(Settings::Instance().IsAutoRefreshEnabled());
}

void PathPane::BindSettings()
{
  ConfigWidget::Bind(m_ui->recursivePathsCheckBox, Config::MAIN_RECURSIVE_ISO_PATHS);
  ConfigWidget::Bind(m_ui->defaultIsoLineEdit, Config::MAIN_DEFAULT_ISO);
  ConfigWidget::BindUserPath(m_ui->nandRootLineEdit, D_WIIROOT_IDX, Config::MAIN_FS_PATH);
  ConfigWidget::BindUserPath(m_ui->dumpPathLineEdit, D_DUMP_IDX, Config::MAIN_DUMP_PATH);
  ConfigWidget::BindUserPath(m_ui->loadPathLineEdit, D_LOAD_IDX, Config::MAIN_LOAD_PATH);
  ConfigWidget::BindUserPath(m_ui->resourcePackPathLineEdit, D_RESOURCEPACK_IDX,
                             Config::MAIN_RESOURCEPACK_PATH);
  ConfigWidget::BindUserPath(m_ui->wfsPathLineEdit, D_WFSROOT_IDX, Config::MAIN_WFS_PATH);
}

void PathPane::ConnectWidgets()
{
  connect(&Settings::Instance(), &Settings::PathAdded, this,
          [this](const QString& dir) { m_ui->pathListWidget->addItem(dir); });
  connect(&Settings::Instance(), &Settings::PathRemoved, this, [this](const QString& dir) {
    const auto items = m_ui->pathListWidget->findItems(dir, Qt::MatchExactly);
    for (auto* const item : items)
      delete item;
  });
  connect(m_ui->pathListWidget, &QListWidget::itemSelectionChanged, this, [this] {
    m_ui->removePathButton->setEnabled(!m_ui->pathListWidget->selectedItems().isEmpty());
  });
  connect(&Settings::Instance(), &Settings::DefaultGameChanged, this,
          [this](const QString& path) { m_ui->defaultIsoLineEdit->setText(path); });

  connect(m_ui->recursivePathsCheckBox, &QCheckBox::toggled,
          [](bool) { Settings::Instance().RefreshGameList(); });
  connect(m_ui->autoRefreshCheckBox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetAutoRefreshEnabled);

  connect(m_ui->addPathButton, &QPushButton::clicked, this, &PathPane::Browse);
  connect(m_ui->removePathButton, &QPushButton::clicked, this, &PathPane::RemovePath);
  connect(m_ui->defaultIsoBrowseButton, &QPushButton::clicked, this, &PathPane::BrowseDefaultGame);
  connect(m_ui->nandRootBrowseButton, &QPushButton::clicked, this, &PathPane::BrowseWiiNAND);
  connect(m_ui->dumpPathBrowseButton, &QPushButton::clicked, this, &PathPane::BrowseDump);
  connect(m_ui->loadPathBrowseButton, &QPushButton::clicked, this, &PathPane::BrowseLoad);
  connect(m_ui->resourcePackPathBrowseButton, &QPushButton::clicked, this,
          &PathPane::BrowseResourcePack);
  connect(m_ui->wfsPathBrowseButton, &QPushButton::clicked, this, &PathPane::BrowseWFS);
}

void PathPane::Browse()
{
  const QString dir = QDir::toNativeSeparators(
      DolphinFileDialog::getExistingDirectory(this, tr("Select a Directory"), QDir::currentPath()));
  if (!dir.isEmpty())
    Settings::Instance().AddPath(dir);
}

void PathPane::BrowseDefaultGame()
{
  const QString file = QDir::toNativeSeparators(DolphinFileDialog::getOpenFileName(
      this, tr("Select a Game"), Settings::Instance().GetDefaultGame(),
      QStringLiteral("%1 (*.elf *.dol *.gcm *.bin *.iso *.tgc *.wbfs *.ciso *.gcz *.wia *.rvz "
                     "hif_000000.nfs *.wad *.m3u *.json);;%2 (*)")
          .arg(tr("All GC/Wii files"))
          .arg(tr("All Files"))));

  if (!file.isEmpty())
    Settings::Instance().SetDefaultGame(file);
}

void PathPane::BrowseWiiNAND()
{
  const QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Wii NAND Root"), QString::fromStdString(File::GetUserPath(D_WIIROOT_IDX))));
  if (!dir.isEmpty())
    SetBoundPath(m_ui->nandRootLineEdit, Config::MAIN_FS_PATH, dir);
}

void PathPane::BrowseDump()
{
  const QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Dump Path"), QString::fromStdString(File::GetUserPath(D_DUMP_IDX))));
  if (!dir.isEmpty())
    SetBoundPath(m_ui->dumpPathLineEdit, Config::MAIN_DUMP_PATH, dir);
}

void PathPane::BrowseLoad()
{
  const QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Load Path"), QString::fromStdString(File::GetUserPath(D_LOAD_IDX))));
  if (!dir.isEmpty())
    SetBasePath(m_ui->loadPathLineEdit, Config::MAIN_LOAD_PATH, dir);
}

void PathPane::BrowseResourcePack()
{
  const QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Resource Pack Path"),
      QString::fromStdString(File::GetUserPath(D_RESOURCEPACK_IDX))));
  if (!dir.isEmpty())
    SetBasePath(m_ui->resourcePackPathLineEdit, Config::MAIN_RESOURCEPACK_PATH, dir);
}

void PathPane::BrowseWFS()
{
  const QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select WFS Path"), QString::fromStdString(File::GetUserPath(D_WFSROOT_IDX))));
  if (!dir.isEmpty())
    SetBasePath(m_ui->wfsPathLineEdit, Config::MAIN_WFS_PATH, dir);
}

void PathPane::RemovePath()
{
  const auto* const item = m_ui->pathListWidget->currentItem();
  if (!item)
    return;
  Settings::Instance().RemovePath(item->text());
}
