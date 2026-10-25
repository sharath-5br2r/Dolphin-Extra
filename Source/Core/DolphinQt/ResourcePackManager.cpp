// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ResourcePackManager.h"

#include <string>

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>

#include "Common/FileUtil.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/ResourcePack.h"

#include "ui_ResourcePackManager.h"

ResourcePackManager::ResourcePackManager(QWidget* widget)
    : QDialog(widget), m_ui(std::make_unique<Ui::ResourcePackManager>())
{
  CreateWidgets();
  ConnectWidgets();
  RepopulateTable();
}

ResourcePackManager::~ResourcePackManager() = default;

void ResourcePackManager::CreateWidgets()
{
  m_ui->setupUi(this);
  m_table_widget = m_ui->tableWidget;
  m_open_directory_button = m_ui->openDirectoryButton;
  m_change_button = m_ui->changeButton;
  m_remove_button = m_ui->removeButton;
  m_refresh_button = m_ui->refreshButton;
  m_priority_up_button = m_ui->priorityUpButton;
  m_priority_down_button = m_ui->priorityDownButton;

  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

void ResourcePackManager::ConnectWidgets()
{
  connect(m_open_directory_button, &QPushButton::clicked, this,
          &ResourcePackManager::OpenResourcePackDir);
  connect(m_refresh_button, &QPushButton::clicked, this, &ResourcePackManager::Refresh);
  connect(m_change_button, &QPushButton::clicked, this, &ResourcePackManager::Change);
  connect(m_remove_button, &QPushButton::clicked, this, &ResourcePackManager::Remove);
  connect(m_priority_up_button, &QPushButton::clicked, this, &ResourcePackManager::PriorityUp);
  connect(m_priority_down_button, &QPushButton::clicked, this, &ResourcePackManager::PriorityDown);

  connect(m_table_widget, &QTableWidget::itemSelectionChanged, this,
          &ResourcePackManager::SelectionChanged);

  connect(m_table_widget, &QTableWidget::itemDoubleClicked, this,
          &ResourcePackManager::ItemDoubleClicked);
}

void ResourcePackManager::OpenResourcePackDir()
{
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdString(File::GetUserPath(D_RESOURCEPACK_IDX))));
}

void ResourcePackManager::RepopulateTable()
{
  m_table_widget->clear();
  m_table_widget->setColumnCount(6);

  m_table_widget->setHorizontalHeaderLabels(
      {QString{}, tr("Name"), tr("Version"), tr("Description"), tr("Author"), tr("Website")});

  auto* header = m_table_widget->horizontalHeader();

  for (int i = 0; i < 4; i++)
    header->setSectionResizeMode(i, QHeaderView::ResizeToContents);

  header->setStretchLastSection(true);
  header->setHighlightSections(false);

  int size = static_cast<int>(ResourcePack::GetPacks().size());

  m_table_widget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table_widget->setSelectionMode(QAbstractItemView::SingleSelection);

  m_table_widget->setRowCount(size);
  m_table_widget->setIconSize(QSize(32, 32));

  for (int i = 0; i < size; i++)
  {
    const auto& pack = ResourcePack::GetPacks()[size - 1 - i];
    const auto* manifest = pack.GetManifest();
    const auto& authors = manifest->GetAuthors();

    auto* logo_item = new QTableWidgetItem;
    auto* name_item = new QTableWidgetItem(QString::fromStdString(manifest->GetName()));
    auto* version_item = new QTableWidgetItem(QString::fromStdString(manifest->GetVersion()));
    auto* author_item =
        new QTableWidgetItem(authors ? QString::fromStdString(*authors) : tr("Unknown author"));
    auto* description_item =
        new QTableWidgetItem(QString::fromStdString(manifest->GetDescription().value_or("")));
    auto* website_item =
        new QTableWidgetItem(QString::fromStdString(manifest->GetWebsite().value_or("")));

    QPixmap logo;

    logo.loadFromData(reinterpret_cast<const uchar*>(pack.GetLogo().data()),
                      (int)pack.GetLogo().size());

    logo_item->setIcon(QIcon(logo));

    QFont link_font = website_item->font();

    link_font.setUnderline(true);

    website_item->setFont(link_font);
    website_item->setForeground(QBrush(Qt::blue));
    website_item->setData(Qt::UserRole, website_item->text());

    for (auto* item :
         {logo_item, name_item, version_item, author_item, description_item, website_item})
    {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

      if (ResourcePack::IsInstalled(pack))
      {
        item->setBackground(QColor(Qt::green));

        auto font = item->font();
        font.setBold(true);
        item->setFont(font);
      }
    }

    m_table_widget->setItem(i, 0, logo_item);
    m_table_widget->setItem(i, 1, name_item);
    m_table_widget->setItem(i, 2, version_item);
    m_table_widget->setItem(i, 3, description_item);
    m_table_widget->setItem(i, 4, author_item);
    m_table_widget->setItem(i, 5, website_item);
  }

  SelectionChanged();
}

// Revert the indices as to be more intuitive for users
int ResourcePackManager::GetResourcePackIndex(QTableWidgetItem* item) const
{
  return m_table_widget->rowCount() - 1 - item->row();
}

void ResourcePackManager::Change()
{
  auto items = m_table_widget->selectedItems();

  if (items.empty())
    return;

  if (ResourcePack::IsInstalled(ResourcePack::GetPacks()[GetResourcePackIndex(items[0])]))
  {
    Uninstall();
  }
  else
  {
    Install();
  }
}

void ResourcePackManager::Install()
{
  auto items = m_table_widget->selectedItems();

  if (items.empty())
    return;

  auto& item = ResourcePack::GetPacks()[GetResourcePackIndex(items[0])];

  bool success = item.Install(File::GetUserPath(D_LOAD_IDX));

  if (!success)
  {
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("Failed to install pack: %1").arg(QString::fromStdString(item.GetError())));
  }

  RepopulateTable();
}

void ResourcePackManager::Uninstall()
{
  auto items = m_table_widget->selectedItems();

  if (items.empty())
    return;

  auto& item = ResourcePack::GetPacks()[GetResourcePackIndex(items[0])];

  bool success = item.Uninstall(File::GetUserPath(D_LOAD_IDX));

  if (!success)
  {
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("Failed to uninstall pack: %1").arg(QString::fromStdString(item.GetError())));
  }

  RepopulateTable();
}

void ResourcePackManager::Remove()
{
  auto items = m_table_widget->selectedItems();

  if (items.empty())
    return;

  ModalMessageBox box(this);
  box.setWindowTitle(tr("Confirmation"));
  box.setText(tr("Are you sure you want to delete this pack?"));
  box.setIcon(QMessageBox::Warning);
  box.setStandardButtons(QMessageBox::Yes | QMessageBox::Abort);

  if (box.exec() != QMessageBox::Yes)
    return;

  ResourcePack::ResourcePack& selected_pack =
      ResourcePack::GetPacks()[GetResourcePackIndex(items[0])];
  const std::string selected_pack_path = selected_pack.GetPath();
  Uninstall();
  ResourcePack::Remove(selected_pack);
  File::Delete(selected_pack_path);
  RepopulateTable();
}

void ResourcePackManager::PriorityDown()
{
  auto items = m_table_widget->selectedItems();

  if (items.empty())
    return;

  auto row = GetResourcePackIndex(items[0]);

  if (items[0]->row() >= m_table_widget->rowCount())
    return;

  auto& pack = ResourcePack::GetPacks()[row];
  std::string path = pack.GetPath();

  row--;

  ResourcePack::Remove(pack);
  ResourcePack::Add(path, row);

  RepopulateTable();

  m_table_widget->selectRow(row == 0 ? m_table_widget->rowCount() - 1 : row);
}

void ResourcePackManager::PriorityUp()
{
  auto items = m_table_widget->selectedItems();

  if (items.empty())
    return;

  auto row = GetResourcePackIndex(items[0]);

  if (items[0]->row() == 0)
    return;

  auto& pack = ResourcePack::GetPacks()[row];
  std::string path = pack.GetPath();

  row++;

  ResourcePack::Remove(pack);
  ResourcePack::Add(path, items[0]->row() == m_table_widget->rowCount() ? -1 : row);

  RepopulateTable();

  m_table_widget->selectRow(row == m_table_widget->rowCount() - 1 ? 0 : row);
}

void ResourcePackManager::Refresh()
{
  ResourcePack::Init();
  RepopulateTable();
}

void ResourcePackManager::SelectionChanged()
{
  auto items = m_table_widget->selectedItems();

  const bool has_selection = !items.empty();

  if (has_selection)
  {
    m_change_button->setText(
        ResourcePack::IsInstalled(ResourcePack::GetPacks()[GetResourcePackIndex(items[0])]) ?
            tr("Uninstall") :
            tr("Install"));
  }

  for (auto* item : {m_change_button, m_remove_button})
    item->setEnabled(has_selection);

  m_priority_down_button->setEnabled(has_selection &&
                                     items[0]->row() < m_table_widget->rowCount() - 1);
  m_priority_up_button->setEnabled(has_selection && items[0]->row() != 0);
}

void ResourcePackManager::ItemDoubleClicked(QTableWidgetItem* item)
{
  auto item_data = item->data(Qt::UserRole);

  if (item_data.isNull())
    return;

  QDesktopServices::openUrl(QUrl(item_data.toString()));
}
