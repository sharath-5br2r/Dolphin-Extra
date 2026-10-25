// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/UTF8CodePointCountValidator.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"
#include "UICommon/NetPlayIndex.h"

#include "ui_NetPlaySetupDialog.h"

NetPlaySetupDialog::NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model),
      m_ui(std::make_unique<Ui::NetPlaySetupDialog>())
{
  m_ui->setupUi(this);
  m_ui->nicknameEdit->setValidator(
      new UTF8CodePointCountValidator(NetPlay::MAX_NAME_LENGTH, m_ui->nicknameEdit));

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_ui->hostRegionCombo->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }

#ifndef USE_UPNP
  m_ui->hostUpnpCheckBox->hide();
#endif

  const bool use_index = Config::Get(Config::NETPLAY_USE_INDEX);
  const std::string index_region = Config::Get(Config::NETPLAY_INDEX_REGION);
  const std::string index_name = Config::Get(Config::NETPLAY_INDEX_NAME);
  const std::string index_password = Config::Get(Config::NETPLAY_INDEX_PASSWORD);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const int connect_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  const int host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  const int host_listen_port = Config::Get(Config::NETPLAY_LISTEN_PORT);
  const bool enable_chunked_upload_limit = Config::Get(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT);
  const u32 chunked_upload_limit = Config::Get(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT);
#ifdef USE_UPNP
  const bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  m_ui->hostUpnpCheckBox->setChecked(use_upnp);
#endif

  m_ui->nicknameEdit->setText(QString::fromStdString(nickname));
  m_ui->connectionTypeCombo->setCurrentIndex(traversal_choice == "direct" ? 0 : 1);
  m_ui->connectPortSpinBox->setValue(connect_port);
  m_ui->hostPortSpinBox->setValue(host_port);

  m_ui->forceListenPortSpinBox->setValue(host_listen_port);
  m_ui->forceListenPortSpinBox->setEnabled(false);

  m_ui->showInBrowserCheckBox->setChecked(use_index);

  m_ui->hostRegionCombo->setEnabled(use_index);
  m_ui->hostRegionCombo->setCurrentIndex(
      m_ui->hostRegionCombo->findData(QString::fromStdString(index_region)));

  m_ui->hostNameEdit->setEnabled(use_index);
  m_ui->hostNameEdit->setText(QString::fromStdString(index_name));

  m_ui->hostPasswordEdit->setEnabled(use_index);
  m_ui->hostPasswordEdit->setText(QString::fromStdString(index_password));

  m_ui->chunkedUploadLimitCheckBox->setChecked(enable_chunked_upload_limit);
  m_ui->chunkedUploadLimitSpinBox->setValue(chunked_upload_limit);
  m_ui->chunkedUploadLimitSpinBox->setEnabled(enable_chunked_upload_limit);

  OnConnectionTypeChanged(m_ui->connectionTypeCombo->currentIndex());

  ConnectWidgets();
}

NetPlaySetupDialog::~NetPlaySetupDialog() = default;

void NetPlaySetupDialog::ConnectWidgets()
{
  connect(m_ui->connectionTypeCombo, &QComboBox::currentIndexChanged, this,
          &NetPlaySetupDialog::OnConnectionTypeChanged);
  connect(m_ui->nicknameEdit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);

  // Connect widget
  connect(m_ui->ipEdit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_ui->connectPortSpinBox, &QSpinBox::valueChanged, this,
          &NetPlaySetupDialog::SaveSettings);
  // Host widget
  connect(m_ui->hostPortSpinBox, &QSpinBox::valueChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_ui->hostGamesList, &QListWidget::currentRowChanged, [this](int index) {
    Settings::GetQSettings().setValue(QStringLiteral("netplay/hostgame"),
                                      m_ui->hostGamesList->item(index)->text());
  });

  connect(m_ui->hostGamesList, &QListWidget::itemDoubleClicked, this, &NetPlaySetupDialog::accept);

  connect(m_ui->forceListenPortCheckBox, &QCheckBox::toggled,
          [this](bool value) { m_ui->forceListenPortSpinBox->setEnabled(value); });
  connect(m_ui->chunkedUploadLimitCheckBox, &QCheckBox::toggled, this, [this](bool value) {
    m_ui->chunkedUploadLimitSpinBox->setEnabled(value);
    SaveSettings();
  });
  connect(m_ui->chunkedUploadLimitSpinBox, &QSpinBox::valueChanged, this,
          &NetPlaySetupDialog::SaveSettings);

  connect(m_ui->showInBrowserCheckBox, &QCheckBox::toggled, this,
          &NetPlaySetupDialog::SaveSettings);
  connect(m_ui->hostNameEdit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_ui->hostPasswordEdit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_ui->hostRegionCombo,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::SaveSettings);

#ifdef USE_UPNP
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(m_ui->hostUpnpCheckBox, &QCheckBox::checkStateChanged, this,
          &NetPlaySetupDialog::SaveSettings);
#else
  connect(m_ui->hostUpnpCheckBox, &QCheckBox::stateChanged, this,
          &NetPlaySetupDialog::SaveSettings);
#endif
#endif

  connect(m_ui->connectButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_ui->hostButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_ui->resetTraversalButton, &QPushButton::clicked, this,
          &NetPlaySetupDialog::ResetTraversalHost);
  connect(m_ui->showInBrowserCheckBox, &QCheckBox::toggled, this, [this](bool value) {
    m_ui->hostRegionCombo->setEnabled(value);
    m_ui->hostNameEdit->setEnabled(value);
    m_ui->hostPasswordEdit->setEnabled(value);
  });
}

void NetPlaySetupDialog::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(Config::NETPLAY_NICKNAME, m_ui->nicknameEdit->text().toStdString());
  Config::SetBaseOrCurrent(m_ui->connectionTypeCombo->currentIndex() == 0 ?
                               Config::NETPLAY_ADDRESS :
                               Config::NETPLAY_HOST_CODE,
                           m_ui->ipEdit->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT,
                           static_cast<u16>(m_ui->connectPortSpinBox->value()));
  Config::SetBaseOrCurrent(Config::NETPLAY_HOST_PORT,
                           static_cast<u16>(m_ui->hostPortSpinBox->value()));
#ifdef USE_UPNP
  Config::SetBaseOrCurrent(Config::NETPLAY_USE_UPNP, m_ui->hostUpnpCheckBox->isChecked());
#endif

  if (m_ui->forceListenPortCheckBox->isChecked())
    Config::SetBaseOrCurrent(Config::NETPLAY_LISTEN_PORT,
                             static_cast<u16>(m_ui->forceListenPortSpinBox->value()));

  Config::SetBaseOrCurrent(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT,
                           m_ui->chunkedUploadLimitCheckBox->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT,
                           m_ui->chunkedUploadLimitSpinBox->value());

  Config::SetBaseOrCurrent(Config::NETPLAY_USE_INDEX, m_ui->showInBrowserCheckBox->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_REGION,
                           m_ui->hostRegionCombo->currentData().toString().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_NAME, m_ui->hostNameEdit->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_PASSWORD,
                           m_ui->hostPasswordEdit->text().toStdString());
}

void NetPlaySetupDialog::OnConnectionTypeChanged(int index)
{
  m_ui->connectPortSpinBox->setHidden(index != 0);
  m_ui->connectPortLabel->setHidden(index != 0);

  m_ui->hostPortLabel->setHidden(index != 0);
  m_ui->hostPortSpinBox->setHidden(index != 0);
#ifdef USE_UPNP
  m_ui->hostUpnpCheckBox->setHidden(index != 0);
#endif
  m_ui->forceListenPortCheckBox->setHidden(index == 0);
  m_ui->forceListenPortSpinBox->setHidden(index == 0);

  m_ui->resetTraversalButton->setHidden(index == 0);

  const std::string address =
      index == 0 ? Config::Get(Config::NETPLAY_ADDRESS) : Config::Get(Config::NETPLAY_HOST_CODE);

  m_ui->ipLabel->setText(index == 0 ? tr("IP Address:") : tr("Host Code:"));
  m_ui->ipEdit->setText(QString::fromStdString(address));

  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE,
                           std::string(index == 0 ? "direct" : "traversal"));
}

void NetPlaySetupDialog::show()
{
  PopulateGameList();
  QDialog::show();
}

void NetPlaySetupDialog::accept()
{
  SaveSettings();
  if (m_ui->tabWidget->currentIndex() == 0)
  {
    emit Join();
  }
  else
  {
    auto items = m_ui->hostGamesList->selectedItems();
    if (items.empty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must select a game to host!"));
      return;
    }

    if (m_ui->showInBrowserCheckBox->isChecked() && m_ui->hostNameEdit->text().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must provide a name for your session!"));
      return;
    }

    if (m_ui->showInBrowserCheckBox->isChecked() &&
        m_ui->hostRegionCombo->currentData().toString().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"),
                                tr("You must provide a region for your session!"));
      return;
    }

    emit Host(*items[0]->data(Qt::UserRole).value<std::shared_ptr<const UICommon::GameFile>>());
  }
}

void NetPlaySetupDialog::PopulateGameList()
{
  QSignalBlocker blocker(m_ui->hostGamesList);

  m_ui->hostGamesList->clear();
  for (int i = 0; i < m_game_list_model.rowCount(QModelIndex()); i++)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list_model.GetGameFile(i);

    auto* item =
        new QListWidgetItem(QString::fromStdString(m_game_list_model.GetNetPlayName(*game)));
    item->setData(Qt::UserRole, QVariant::fromValue(std::move(game)));
    m_ui->hostGamesList->addItem(item);
  }

  m_ui->hostGamesList->sortItems();

  const QString selected_game =
      Settings::GetQSettings().value(QStringLiteral("netplay/hostgame"), QString{}).toString();
  const auto find_list = m_ui->hostGamesList->findItems(selected_game, Qt::MatchFlag::MatchExactly);

  if (find_list.count() > 0)
    m_ui->hostGamesList->setCurrentItem(find_list[0]);
}

void NetPlaySetupDialog::ResetTraversalHost()
{
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_SERVER,
                           Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue());
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_PORT,
                           Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue());

  ModalMessageBox::information(
      this, tr("Reset Traversal Server"),
      tr("Reset Traversal Server to %1:%2")
          .arg(QString::fromStdString(Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue()),
               QString::number(Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue())));
}
