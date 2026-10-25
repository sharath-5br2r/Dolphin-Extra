// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlayBrowser.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "Common/Version.h"

#include "Core/Config/NetplaySettings.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

#include "ui_NetPlayBrowser.h"

NetPlayBrowser::NetPlayBrowser(QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::NetPlayBrowser>())
{
  m_ui->setupUi(this);

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_ui->regionCombo->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }
  m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
  RestoreSettings();
  ConnectWidgets();

  m_ui->tableWidget->verticalHeader()->setHidden(true);
  m_ui->tableWidget->setAlternatingRowColors(true);

  m_refresh_run.Set(true);
  m_refresh_thread = std::thread([this] { RefreshLoop(); });

  UpdateList();
  Refresh();
}

NetPlayBrowser::~NetPlayBrowser()
{
  m_refresh_run.Set(false);
  m_refresh_event.Set();
  if (m_refresh_thread.joinable())
    m_refresh_thread.join();

  SaveSettings();
}

void NetPlayBrowser::ConnectWidgets()
{
  connect(m_ui->regionCombo, &QComboBox::currentIndexChanged, this, &NetPlayBrowser::Refresh);

  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &NetPlayBrowser::accept);
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &NetPlayBrowser::reject);
  connect(m_ui->refreshButton, &QPushButton::clicked, this, &NetPlayBrowser::Refresh);

  connect(m_ui->radioAll, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);
  connect(m_ui->radioPrivate, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);
  connect(m_ui->hideIncompatibleCheckBox, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);
  connect(m_ui->hideInGameCheckBox, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);

  connect(m_ui->nameEdit, &QLineEdit::textChanged, this, &NetPlayBrowser::Refresh);
  connect(m_ui->gameIdEdit, &QLineEdit::textChanged, this, &NetPlayBrowser::Refresh);

  connect(m_ui->tableWidget, &QTableWidget::itemSelectionChanged, this,
          &NetPlayBrowser::OnSelectionChanged);
  connect(m_ui->tableWidget, &QTableWidget::itemDoubleClicked, this, &NetPlayBrowser::accept);

  connect(this, &NetPlayBrowser::UpdateStatusRequested, this,
          &NetPlayBrowser::OnUpdateStatusRequested, Qt::QueuedConnection);
  connect(this, &NetPlayBrowser::UpdateListRequested, this, &NetPlayBrowser::OnUpdateListRequested,
          Qt::QueuedConnection);
}

void NetPlayBrowser::Refresh()
{
  std::map<std::string, std::string> filters;

  if (m_ui->hideIncompatibleCheckBox->isChecked())
    filters["version"] = Common::GetScmDescStr();

  if (!m_ui->nameEdit->text().isEmpty())
    filters["name"] = m_ui->nameEdit->text().toStdString();

  if (!m_ui->gameIdEdit->text().isEmpty())
    filters["game"] = m_ui->gameIdEdit->text().toStdString();

  if (!m_ui->radioAll->isChecked())
    filters["password"] = std::to_string(m_ui->radioPrivate->isChecked());

  if (m_ui->regionCombo->currentIndex() != 0)
    filters["region"] = m_ui->regionCombo->currentData().toString().toStdString();

  if (m_ui->hideInGameCheckBox->isChecked())
    filters["in_game"] = "0";

  std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
  m_refresh_filters = std::move(filters);
  m_refresh_event.Set();
}

void NetPlayBrowser::RefreshLoop()
{
  while (m_refresh_run.IsSet())
  {
    m_refresh_event.Wait();

    std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
    if (m_refresh_filters)
    {
      auto filters = std::move(*m_refresh_filters);
      m_refresh_filters.reset();

      lock.unlock();

      emit UpdateStatusRequested(tr("Refreshing..."));

      NetPlayIndex client;

      auto entries = client.List(filters);

      if (entries)
      {
        emit UpdateListRequested(std::move(*entries));
      }
      else
      {
        emit UpdateStatusRequested(tr("Error obtaining session list: %1")
                                       .arg(QString::fromStdString(client.GetLastError())));
      }
    }
  }
}

void NetPlayBrowser::UpdateList()
{
  const int session_count = static_cast<int>(m_sessions.size());

  m_ui->tableWidget->clear();
  m_ui->tableWidget->setColumnCount(7);
  m_ui->tableWidget->setHorizontalHeaderLabels({tr("Region"), tr("Name"), tr("Password?"),
                                                tr("In-Game?"), tr("Game"), tr("Players"),
                                                tr("Version")});

  auto* hor_header = m_ui->tableWidget->horizontalHeader();

  hor_header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(1, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(4, QHeaderView::Stretch);
  hor_header->setHighlightSections(false);

  m_ui->tableWidget->setRowCount(session_count);

  for (int i = 0; i < session_count; i++)
  {
    const auto& entry = m_sessions[i];

    auto* region = new QTableWidgetItem(QString::fromStdString(entry.region));
    auto* name = new QTableWidgetItem(QString::fromStdString(entry.name));
    auto* password = new QTableWidgetItem(entry.has_password ? tr("Yes") : tr("No"));
    auto* in_game = new QTableWidgetItem(entry.in_game ? tr("Yes") : tr("No"));
    auto* game_id = new QTableWidgetItem(QString::fromStdString(entry.game_id));
    auto* player_count = new QTableWidgetItem(QStringLiteral("%1").arg(entry.player_count));
    auto* version = new QTableWidgetItem(QString::fromStdString(entry.version));

    const bool enabled = Common::GetScmDescStr() == entry.version;

    for (const auto& item : {region, name, password, in_game, game_id, player_count, version})
      item->setFlags(enabled ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);

    m_ui->tableWidget->setItem(i, 0, region);
    m_ui->tableWidget->setItem(i, 1, name);
    m_ui->tableWidget->setItem(i, 2, password);
    m_ui->tableWidget->setItem(i, 3, in_game);
    m_ui->tableWidget->setItem(i, 4, game_id);
    m_ui->tableWidget->setItem(i, 5, player_count);
    m_ui->tableWidget->setItem(i, 6, version);
  }

  m_ui->statusLabel->setText(
      (session_count == 1 ? tr("%1 session found") : tr("%1 sessions found")).arg(session_count));
}

void NetPlayBrowser::OnSelectionChanged()
{
  m_ui->buttonBox->button(QDialogButtonBox::Ok)
      ->setEnabled(!m_ui->tableWidget->selectedItems().isEmpty());
}

void NetPlayBrowser::OnUpdateStatusRequested(const QString& status)
{
  m_ui->statusLabel->setText(status);
}

void NetPlayBrowser::OnUpdateListRequested(std::vector<NetPlaySession> sessions)
{
  m_sessions = std::move(sessions);
  UpdateList();
}

void NetPlayBrowser::accept()
{
  if (m_ui->tableWidget->selectedItems().isEmpty())
    return;

  const int index = m_ui->tableWidget->selectedItems()[0]->row();

  const NetPlaySession& session = m_sessions[index];

  std::string server_id = session.server_id;

  if (m_sessions[index].has_password)
  {
    QInputDialog dialog(this);

    dialog.setWindowTitle(tr("Enter password"));
    dialog.setLabelText(tr("This session requires a password:"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setTextEchoMode(QLineEdit::Password);

    if (dialog.exec() != QDialog::Accepted)
      return;

    const std::string password = dialog.textValue().toStdString();

    const auto decrypted_id = session.DecryptID(password);

    if (!decrypted_id)
    {
      ModalMessageBox::warning(this, tr("Error"), tr("Invalid password provided."));
      return;
    }

    server_id = decrypted_id.value();
  }

  QDialog::accept();

  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, session.method);

  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT, session.port);

  if (session.method == "traversal")
    Config::SetBaseOrCurrent(Config::NETPLAY_HOST_CODE, server_id);
  else
    Config::SetBaseOrCurrent(Config::NETPLAY_ADDRESS, server_id);

  emit Join();
}

void NetPlayBrowser::SaveSettings() const
{
  auto& settings = Settings::Instance().GetQSettings();

  settings.setValue(QStringLiteral("netplaybrowser/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("netplaybrowser/region"), m_ui->regionCombo->currentText());
  settings.setValue(QStringLiteral("netplaybrowser/name"), m_ui->nameEdit->text());
  settings.setValue(QStringLiteral("netplaybrowser/game_id"), m_ui->gameIdEdit->text());

  QString visibility(QStringLiteral("all"));
  if (m_ui->radioPublic->isChecked())
    visibility = QStringLiteral("public");
  else if (m_ui->radioPrivate->isChecked())
    visibility = QStringLiteral("private");
  settings.setValue(QStringLiteral("netplaybrowser/visibility"), visibility);

  settings.setValue(QStringLiteral("netplaybrowser/hide_incompatible"),
                    m_ui->hideIncompatibleCheckBox->isChecked());
  settings.setValue(QStringLiteral("netplaybrowser/hide_ingame"),
                    m_ui->hideInGameCheckBox->isChecked());
}

void NetPlayBrowser::RestoreSettings()
{
  const auto& settings = Settings::Instance().GetQSettings();

  const QByteArray geometry =
      settings.value(QStringLiteral("netplaybrowser/geometry")).toByteArray();
  if (!geometry.isEmpty())
    restoreGeometry(geometry);

  const QString region = settings.value(QStringLiteral("netplaybrowser/region")).toString();
  const bool valid_region = m_ui->regionCombo->findText(region) != -1;
  if (valid_region)
    m_ui->regionCombo->setCurrentText(region);

  m_ui->nameEdit->setText(settings.value(QStringLiteral("netplaybrowser/name")).toString());
  m_ui->gameIdEdit->setText(settings.value(QStringLiteral("netplaybrowser/game_id")).toString());

  const QString visibility = settings.value(QStringLiteral("netplaybrowser/visibility")).toString();
  if (visibility == QStringLiteral("public"))
    m_ui->radioPublic->setChecked(true);
  else if (visibility == QStringLiteral("private"))
    m_ui->radioPrivate->setChecked(true);

  m_ui->hideIncompatibleCheckBox->setChecked(
      settings.value(QStringLiteral("netplaybrowser/hide_incompatible"), true).toBool());
  m_ui->hideInGameCheckBox->setChecked(
      settings.value(QStringLiteral("netplaybrowser/hide_ingame")).toBool());
}
