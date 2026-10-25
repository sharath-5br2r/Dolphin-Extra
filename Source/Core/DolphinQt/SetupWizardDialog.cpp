// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/SetupWizardDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSize>
#include <QStackedWidget>

#include "Common/Config/Config.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/DolphinAnalytics.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/InterfaceSettingsUtils.h"
#include "DolphinQt/SetupWizardPolicy.h"

#include "ui_SetupWizardDialog.h"

SetupWizardDialog::SetupWizardDialog(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::SetupWizardDialog>())
{
  m_ui->setupUi(this);
  setWindowIcon(Resources::GetAppIcon());
  m_ui->logoLabel->setPixmap(Resources::GetAppIcon().pixmap(QSize(112, 112)));

  m_page_labels = {
      m_ui->appearancePageLabel, m_ui->gameFoldersPageLabel, m_ui->controllersPageLabel,
      m_ui->privacyPageLabel,    m_ui->completePageLabel,
  };

  PopulateAppearance();
  PopulateGameFolders();
  ConnectWidgets();
  UpdatePage();
}

SetupWizardDialog::~SetupWizardDialog() = default;

bool SetupWizardDialog::ShouldOpenControllerSettings() const
{
  return m_ui->openControllerSettingsCheckBox->isChecked();
}

void SetupWizardDialog::PopulateAppearance()
{
  for (const auto& [name, code] : InterfaceSettings::GetLanguageChoices())
    m_ui->languageComboBox->addItem(name, code);

  const QString language = QString::fromStdString(Config::Get(Config::MAIN_INTERFACE_LANGUAGE));
  const int language_index = m_ui->languageComboBox->findData(language);
  m_ui->languageComboBox->setCurrentIndex(language_index >= 0 ? language_index : 0);

  m_ui->styleComboBox->addItem(tr("(System)"), static_cast<int>(Settings::StyleType::System));
  m_ui->styleComboBox->addItem(tr("(Light)"), static_cast<int>(Settings::StyleType::Light));
  m_ui->styleComboBox->addItem(tr("(Dark Gray)"), static_cast<int>(Settings::StyleType::DarkGray));
  m_ui->styleComboBox->addItem(tr("(Dark)"), static_cast<int>(Settings::StyleType::Dark));

  const int style_index =
      m_ui->styleComboBox->findData(static_cast<int>(Settings::Instance().GetStyleType()));
  m_ui->styleComboBox->setCurrentIndex(style_index >= 0 ? style_index : 0);
}

void SetupWizardDialog::PopulateGameFolders()
{
  m_ui->gameFoldersListWidget->addItems(Settings::Instance().GetPaths());
  m_ui->recursivePathsCheckBox->setChecked(Config::Get(Config::MAIN_RECURSIVE_ISO_PATHS));
}

void SetupWizardDialog::ConnectWidgets()
{
  connect(m_ui->backButton, &QPushButton::clicked, this, &SetupWizardDialog::ShowPreviousPage);
  connect(m_ui->nextButton, &QPushButton::clicked, this, &SetupWizardDialog::ShowNextPage);
  connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  connect(m_ui->languageComboBox, &QComboBox::currentIndexChanged, this, [this] {
    Config::SetBase(Config::MAIN_INTERFACE_LANGUAGE,
                    m_ui->languageComboBox->currentData().toString().toStdString());
  });
  connect(m_ui->styleComboBox, &QComboBox::currentIndexChanged, this, [this] {
    Settings::Instance().SetStyleType(
        static_cast<Settings::StyleType>(m_ui->styleComboBox->currentData().toInt()));
    Settings::Instance().ApplyStyle();
  });

  connect(m_ui->addGameFolderButton, &QPushButton::clicked, this,
          &SetupWizardDialog::AddGameFolder);
  connect(m_ui->removeGameFolderButton, &QPushButton::clicked, this,
          &SetupWizardDialog::RemoveGameFolder);
  connect(m_ui->gameFoldersListWidget, &QListWidget::itemSelectionChanged, this, [this] {
    m_ui->removeGameFolderButton->setEnabled(
        !m_ui->gameFoldersListWidget->selectedItems().isEmpty());
  });
  connect(m_ui->recursivePathsCheckBox, &QCheckBox::toggled, this,
          [](bool enabled) { Config::SetBase(Config::MAIN_RECURSIVE_ISO_PATHS, enabled); });
}

void SetupWizardDialog::AddGameFolder()
{
  const QString path = QDir::toNativeSeparators(
      DolphinFileDialog::getExistingDirectory(this, tr("Select a Game Folder"), QDir::homePath()));
  if (path.isEmpty() || m_ui->gameFoldersListWidget->findItems(path, Qt::MatchExactly).size() > 0)
    return;

  Settings::Instance().AddPath(path);
  m_ui->gameFoldersListWidget->addItem(path);
}

void SetupWizardDialog::RemoveGameFolder()
{
  const auto selected_items = m_ui->gameFoldersListWidget->selectedItems();
  if (selected_items.isEmpty())
    return;

  QListWidgetItem* const item = selected_items.front();
  Settings::Instance().RemovePath(item->text());
  delete item;
}

void SetupWizardDialog::ShowPreviousPage()
{
  const int page = m_ui->pages->currentIndex();
  if (page > 0)
    m_ui->pages->setCurrentIndex(page - 1);
  UpdatePage();
}

void SetupWizardDialog::ShowNextPage()
{
  const int page = m_ui->pages->currentIndex();
  if (page == static_cast<int>(Page::Complete))
  {
    FinishSetup();
    return;
  }

  m_ui->pages->setCurrentIndex(page + 1);
  UpdatePage();
}

void SetupWizardDialog::UpdatePage()
{
  const int current_page = m_ui->pages->currentIndex();
  for (std::size_t i = 0; i < m_page_labels.size(); ++i)
  {
    QFont font = m_page_labels[i]->font();
    font.setBold(static_cast<int>(i) == current_page);
    m_page_labels[i]->setFont(font);
  }

  m_ui->backButton->setEnabled(current_page > 0);
  m_ui->nextButton->setText(current_page == static_cast<int>(Page::Complete) ? tr("&Finish") :
                                                                               tr("&Next"));

  switch (static_cast<Page>(current_page))
  {
  case Page::Appearance:
    m_ui->languageComboBox->setFocus();
    break;
  case Page::GameFolders:
    m_ui->addGameFolderButton->setFocus();
    break;
  case Page::Controllers:
    m_ui->openControllerSettingsCheckBox->setFocus();
    break;
  case Page::Privacy:
    m_ui->analyticsCheckBox->setFocus();
    break;
  case Page::Complete:
    m_ui->nextButton->setFocus();
    break;
  case Page::Count:
    break;
  }
}

void SetupWizardDialog::FinishSetup()
{
  Config::SetBase(Config::MAIN_ANALYTICS_PERMISSION_ASKED, true);
  Settings::Instance().SetAnalyticsEnabled(m_ui->analyticsCheckBox->isChecked());
  DolphinAnalytics::Instance().ReloadConfig();
  Config::Save();

  QSettings& settings = Settings::GetQSettings();
  settings.remove(QString::fromLatin1(SetupWizard::INCOMPLETE_SETTING));
  settings.sync();

  accept();
}
