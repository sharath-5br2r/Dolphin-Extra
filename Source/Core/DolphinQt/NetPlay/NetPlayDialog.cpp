// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlayDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>

#include <algorithm>
#include <utility>

#ifdef HAS_LIBMGBA
#include <fmt/ranges.h>
#endif

#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayCommon.h"
#include "Core/NetPlayProto.h"

#include "Core/Boot/Boot.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#ifdef HAS_LIBMGBA
#include "Core/HW/GBACore.h"
#endif
#include "Core/IOS/FS/FileSystem.h"
#include "Core/NetPlayServer.h"
#include "Core/SyncIdentifier.h"
#include "Core/System.h"

#include "DolphinQt/NetPlay/ChunkedProgressDialog.h"
#include "DolphinQt/NetPlay/GameDigestDialog.h"
#include "DolphinQt/NetPlay/GameListDialog.h"
#include "DolphinQt/NetPlay/PadMappingDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/GameCubePane.h"

#include "UICommon/DiscordPresence.h"
#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"

#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"

#include "ui_NetPlayDialog.h"

namespace
{
QString InetAddressToString(const Common::TraversalInetAddress& addr)
{
  QString ip;

  if (addr.isIPV6)
  {
    ip = QStringLiteral("IPv6-Not-Implemented");
  }
  else
  {
    const auto ipv4 = reinterpret_cast<const u8*>(addr.address);
    ip = QString::number(ipv4[0]);
    for (u32 i = 1; i != 4; ++i)
    {
      ip += QStringLiteral(".");
      ip += QString::number(ipv4[i]);
    }
  }

  return QStringLiteral("%1:%2").arg(ip, QString::number(ntohs(addr.port)));
}
}  // namespace

NetPlayDialog::NetPlayDialog(const GameListModel& game_list_model,
                             StartGameCallback start_game_callback, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model),
      m_start_game_callback(std::move(start_game_callback)),
      m_ui(std::make_unique<Ui::NetPlayDialog>())
{
  m_ui->setupUi(this);
  setWindowIcon(Resources::GetAppIcon());

  m_savedata_style_group = new QActionGroup(this);
  m_savedata_style_group->setExclusive(true);
  m_savedata_style_group->addAction(m_ui->actionSavedataNone);
  m_savedata_style_group->addAction(m_ui->actionSavedataLoadOnly);
  m_savedata_style_group->addAction(m_ui->actionSavedataLoadWrite);

  m_network_mode_group = new QActionGroup(this);
  m_network_mode_group->setExclusive(true);
  m_network_mode_group->addAction(m_ui->actionFixedDelay);
  m_network_mode_group->addAction(m_ui->actionHostInputAuthority);
  m_network_mode_group->addAction(m_ui->actionGolfMode);

  connect(m_ui->actionChecksumCurrentGame, &QAction::triggered, this, [this] {
    Settings::Instance().GetNetPlayServer()->ComputeGameDigest(m_current_game_identifier);
  });
  connect(m_ui->actionChecksumOtherGame, &QAction::triggered, this, [this] {
    GameListDialog game_list_dialog(m_game_list_model, this);

    if (game_list_dialog.exec() != QDialog::Accepted)
      return;
    Settings::Instance().GetNetPlayServer()->ComputeGameDigest(
        game_list_dialog.GetSelectedGame().GetSyncIdentifier());
  });
  connect(m_ui->actionChecksumSdCard, &QAction::triggered, this, [] {
    Settings::Instance().GetNetPlayServer()->ComputeGameDigest(
        NetPlay::NetPlayClient::GetSDCardIdentifier());
  });

  m_ui->playersList->verticalHeader()->hide();
  m_ui->playersList->horizontalHeader()->setStretchLastSection(true);
  m_ui->playersList->horizontalHeader()->setHighlightSections(false);
  for (int i = 0; i < 4; i++)
    m_ui->playersList->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

  m_ui->bufferSizeSpinBox->setMaximum(NetPlay::MAX_TARGET_PAD_BUFFER_SIZE);

  m_pad_mapping = new PadMappingDialog(this);
  m_game_digest_dialog = new GameDigestDialog(this);
  m_chunked_progress_dialog = new ChunkedProgressDialog(this);

  ResetExternalIP();
  LoadSettings();
  ConnectWidgets();

  const auto& settings = Settings::Instance().GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("netplaydialog/geometry")).toByteArray());
  m_ui->splitter->restoreState(
      settings.value(QStringLiteral("netplaydialog/splitter")).toByteArray());
}

NetPlayDialog::~NetPlayDialog()
{
  auto& settings = Settings::Instance().GetQSettings();

  settings.setValue(QStringLiteral("netplaydialog/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("netplaydialog/splitter"), m_ui->splitter->saveState());
}

void NetPlayDialog::ConnectWidgets()
{
  // Players
  connect(m_ui->roomBox, &QComboBox::currentIndexChanged, this, &NetPlayDialog::UpdateGUI);
  connect(m_ui->hostcodeActionButton, &QPushButton::clicked, [this] {
    if (m_is_copy_button_retry)
      Common::g_TraversalClient->ReconnectToServer();
    else
      QApplication::clipboard()->setText(m_ui->hostcodeLabel->text());
  });
  connect(m_ui->playersList, &QTableWidget::itemSelectionChanged, [this] {
    const int row = m_ui->playersList->currentRow();
    m_ui->kickButton->setEnabled(row > 0 &&
                                 !m_ui->playersList->currentItem()->data(Qt::UserRole).isNull());
  });
  connect(m_ui->kickButton, &QPushButton::clicked, [this] {
    const auto id = m_ui->playersList->currentItem()->data(Qt::UserRole).toInt();
    Settings::Instance().GetNetPlayServer()->KickPlayer(id);
  });
  connect(m_ui->assignPortsButton, &QPushButton::clicked, [this] {
    m_pad_mapping->exec();

    Settings::Instance().GetNetPlayServer()->SetPadMapping(m_pad_mapping->GetGCPadArray());
    Settings::Instance().GetNetPlayServer()->SetGBAConfig(m_pad_mapping->GetGBAArray(), true);
    Settings::Instance().GetNetPlayServer()->SetWiimoteMapping(m_pad_mapping->GetWiimoteArray());
  });

  // Chat
  connect(m_ui->chatSendButton, &QPushButton::clicked, this, &NetPlayDialog::OnChat);
  connect(m_ui->chatTypeEdit, &QLineEdit::returnPressed, this, &NetPlayDialog::OnChat);
  connect(m_ui->chatTypeEdit, &QLineEdit::textChanged, this,
          [this] { m_ui->chatSendButton->setEnabled(!m_ui->chatTypeEdit->text().isEmpty()); });

  // Other
  connect(m_ui->bufferSizeSpinBox, &QSpinBox::valueChanged, [this](int value) {
    if (value == m_buffer_size)
      return;

    const auto client = Settings::Instance().GetNetPlayClient();
    const auto server = Settings::Instance().GetNetPlayServer();
    if (server && !m_host_input_authority)
      server->AdjustPadBufferSize(value);
    else
      client->AdjustPadBufferSize(value);
  });

  const auto hia_function = [this](bool enable) {
    if (m_host_input_authority != enable)
    {
      const auto server = Settings::Instance().GetNetPlayServer();
      if (server)
        server->SetHostInputAuthority(enable);
    }
  };

  connect(m_ui->actionHostInputAuthority, &QAction::toggled, this,
          [hia_function] { hia_function(true); });
  connect(m_ui->actionGolfMode, &QAction::toggled, this, [hia_function] { hia_function(true); });
  connect(m_ui->actionFixedDelay, &QAction::toggled, this, [hia_function] { hia_function(false); });

  connect(m_ui->startButton, &QPushButton::clicked, this, &NetPlayDialog::OnStart);
  connect(m_ui->quitButton, &QPushButton::clicked, this, &NetPlayDialog::reject);

  connect(m_ui->gameButton, &QPushButton::clicked, [this] {
    GameListDialog gld(m_game_list_model, this);
    if (gld.exec() == QDialog::Accepted)
    {
      Settings& settings = Settings::Instance();

      const UICommon::GameFile& game = gld.GetSelectedGame();
      const std::string netplay_name = m_game_list_model.GetNetPlayName(game);

      settings.GetNetPlayServer()->ChangeGame(game.GetSyncIdentifier(), netplay_name);
      Settings::GetQSettings().setValue(QStringLiteral("netplay/hostgame"),
                                        QString::fromStdString(netplay_name));
    }
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    if (isVisible())
    {
      GameStatusChanged(state != Core::State::Uninitialized);
      if ((state == Core::State::Uninitialized || state == Core::State::Stopping) &&
          !m_got_stop_request)
      {
        Settings::Instance().GetNetPlayClient()->RequestStopGame();
      }
      if (state == Core::State::Uninitialized)
        DisplayMessage(tr("Stopped game"), "red");
    }
  });

  // SaveSettings() - Save Hosting-Dialog Settings

  connect(m_ui->bufferSizeSpinBox, &QSpinBox::valueChanged, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionSavedataNone, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionSavedataLoadOnly, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionSavedataLoadWrite, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionSavedataAllWii, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionSyncCodes, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionRecordInputs, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionStrictSettingsSync, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionHostInputAuthority, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionGolfMode, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionGolfModeOverlay, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionFixedDelay, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
  connect(m_ui->actionHideRemoteGBAs, &QAction::toggled, this, &NetPlayDialog::SaveSettings);
}

void NetPlayDialog::SendMessage(const std::string& msg)
{
  Settings::Instance().GetNetPlayClient()->SendChatMessage(msg);

  DisplayMessage(
      QStringLiteral("%1: %2").arg(QString::fromStdString(m_nickname), QString::fromStdString(msg)),
      "");
}

void NetPlayDialog::OnChat()
{
  QueueOnObject(this, [this] {
    const auto msg = m_ui->chatTypeEdit->text().toStdString();

    if (msg.empty())
      return;

    m_ui->chatTypeEdit->clear();

    SendMessage(msg);
  });
}

void NetPlayDialog::OnIndexAdded(bool success, const std::string error)
{
  DisplayMessage(success ? tr("Successfully added to the NetPlay index") :
                           tr("Failed to add this session to the NetPlay index: %1")
                               .arg(QString::fromStdString(error)),
                 success ? "green" : "red");
}

void NetPlayDialog::OnIndexRefreshFailed(const std::string error)
{
  DisplayMessage(QString::fromStdString(error), "red");
}

void NetPlayDialog::OnStart()
{
  if (!Settings::Instance().GetNetPlayClient()->DoAllPlayersHaveGame())
  {
    if (ModalMessageBox::question(
            this, tr("Warning"),
            tr("Not all players have the game. Do you really want to start?")) == QMessageBox::No)
      return;
  }

  if (m_ui->actionStrictSettingsSync->isChecked() && Config::Get(Config::GFX_EFB_SCALE) == 0)
  {
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("Auto internal resolution is not allowed in strict sync mode, as it depends on window "
           "size.\n\nPlease select a specific internal resolution."));
    return;
  }

  const auto game = FindGameFile(m_current_game_identifier);
  if (!game)
  {
    PanicAlertFmtT("Selected game doesn't exist in game list!");
    return;
  }

  if (Settings::Instance().GetNetPlayServer()->RequestStartGame())
    SetOptionsEnabled(false);
}

void NetPlayDialog::reject()
{
  if (ModalMessageBox::question(this, tr("Confirmation"),
                                tr("Are you sure you want to quit NetPlay?")) == QMessageBox::Yes)
  {
    QDialog::reject();
  }
}

void NetPlayDialog::show(std::string nickname, bool use_traversal)
{
  m_nickname = std::move(nickname);
  m_use_traversal = use_traversal;
  m_buffer_size = 0;
  m_old_player_count = 0;

  m_ui->roomBox->clear();
  m_ui->chatEdit->clear();
  m_ui->chatTypeEdit->clear();

  const bool is_hosting = Settings::Instance().GetNetPlayServer() != nullptr;

  if (is_hosting)
  {
    if (use_traversal)
      m_ui->roomBox->addItem(tr("Room ID"));
    m_ui->roomBox->addItem(tr("External"));

    for (const auto& iface : Settings::Instance().GetNetPlayServer()->GetInterfaceSet())
    {
      const auto interface = QString::fromStdString(iface);
      m_ui->roomBox->addItem(iface == "!local!" ? tr("Local") : interface, interface);
    }
  }

  m_ui->dataMenu->menuAction()->setVisible(is_hosting);
  m_ui->networkMenu->menuAction()->setVisible(is_hosting);
  m_ui->gameDigestMenu->menuAction()->setVisible(is_hosting);
#ifdef HAS_LIBMGBA
  m_ui->actionHideRemoteGBAs->setVisible(is_hosting);
#else
  m_ui->actionHideRemoteGBAs->setVisible(false);
#endif
  m_ui->startButton->setHidden(!is_hosting);
  m_ui->kickButton->setHidden(!is_hosting);
  m_ui->assignPortsButton->setHidden(!is_hosting);
  m_ui->roomBox->setHidden(!is_hosting);
  m_ui->hostcodeLabel->setHidden(!is_hosting);
  m_ui->hostcodeActionButton->setHidden(!is_hosting);
  m_ui->gameButton->setEnabled(is_hosting);
  m_ui->kickButton->setEnabled(false);

  SetOptionsEnabled(true);

  QDialog::show();
  UpdateGUI();
}

void NetPlayDialog::ResetExternalIP()
{
  m_external_ip_address =
      Common::Lazy<std::string>([]() -> std::string { return NetPlay::GetExternalIPAddress(); });
}

void NetPlayDialog::UpdateDiscordPresence()
{
#ifdef USE_DISCORD_PRESENCE
  // both m_current_game and m_player_count need to be set for the status to be displayed correctly
  if (m_player_count == 0 || m_current_game_name.empty())
    return;

  const auto use_default = [this] {
    Discord::UpdateDiscordPresence(m_player_count, Discord::SecretType::Empty, "",
                                   m_current_game_name);
  };

  if (Core::IsRunning(Core::System::GetInstance()))
    return use_default();

  if (IsHosting())
  {
    if (Common::g_TraversalClient)
    {
      const auto host_id = Common::g_TraversalClient->GetHostID();
      if (host_id[0] == '\0')
        return use_default();

      Discord::UpdateDiscordPresence(m_player_count, Discord::SecretType::RoomID,
                                     std::string(host_id.begin(), host_id.end()),
                                     m_current_game_name);
    }
    else
    {
      if (m_external_ip_address->empty())
        return use_default();
      const int port = Settings::Instance().GetNetPlayServer()->GetPort();

      Discord::UpdateDiscordPresence(
          m_player_count, Discord::SecretType::IPAddress,
          Discord::CreateSecretFromIPAddress(*m_external_ip_address, port), m_current_game_name);
    }
  }
  else
  {
    use_default();
  }
#endif
}

void NetPlayDialog::UpdateGUI()
{
  const auto client = Settings::Instance().GetNetPlayClient();
  const auto server = Settings::Instance().GetNetPlayServer();
  if (!client)
    return;

  // Update Player List
  const auto players = client->GetPlayers();

  if (static_cast<int>(players.size()) != m_player_count && m_player_count != 0)
    QApplication::alert(this);

  m_player_count = static_cast<int>(players.size());

  const int selection_pid = m_ui->playersList->currentItem() ?
                                m_ui->playersList->currentItem()->data(Qt::UserRole).toInt() :
                                -1;

  m_ui->playersList->clear();
  m_ui->playersList->setHorizontalHeaderLabels(
      {tr("Player"), tr("Game Status"), tr("Ping"), tr("Mapping"), tr("Revision")});
  m_ui->playersList->setRowCount(m_player_count);

  static const std::map<NetPlay::SyncIdentifierComparison, std::pair<QString, QString>>
      player_status{
          {NetPlay::SyncIdentifierComparison::SameGame, {tr("OK"), tr("OK")}},
          {NetPlay::SyncIdentifierComparison::DifferentHash,
           {tr("Wrong hash"),
            tr("Game file has a different hash; right-click it, select Properties, switch to the "
               "Verify tab, and select Verify Integrity to check the hash")}},
          {NetPlay::SyncIdentifierComparison::DifferentDiscNumber,
           {tr("Wrong disc number"), tr("Game has a different disc number")}},
          {NetPlay::SyncIdentifierComparison::DifferentRevision,
           {tr("Wrong revision"), tr("Game has a different revision")}},
          {NetPlay::SyncIdentifierComparison::DifferentRegion,
           {tr("Wrong region"), tr("Game region does not match")}},
          {NetPlay::SyncIdentifierComparison::DifferentGame,
           {tr("Not found"), tr("No matching game was found")}},
      };

  for (int i = 0; i < m_player_count; i++)
  {
    const auto* p = players[i];

    auto* name_item = new QTableWidgetItem(QString::fromStdString(p->name));
    name_item->setToolTip(name_item->text());
    const auto it = player_status.find(p->game_status);
    const auto& status_info = it != player_status.end() ?
                                  it->second :
                                  std::make_pair(QStringLiteral("?"), QStringLiteral("?"));
    auto* status_item = new QTableWidgetItem(status_info.first);
    status_item->setToolTip(status_info.second);
    auto* ping_item = new QTableWidgetItem(QStringLiteral("%1 ms").arg(p->ping));
    ping_item->setToolTip(ping_item->text());
    auto* mapping_item =
        new QTableWidgetItem(QString::fromStdString(NetPlay::GetPlayerMappingString(
            p->pid, client->GetPadMapping(), client->GetGBAConfig(), client->GetWiimoteMapping())));
    mapping_item->setToolTip(mapping_item->text());
    auto* revision_item = new QTableWidgetItem(QString::fromStdString(p->revision));
    revision_item->setToolTip(revision_item->text());

    for (auto* item : {name_item, status_item, ping_item, mapping_item, revision_item})
    {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setData(Qt::UserRole, static_cast<int>(p->pid));
    }

    m_ui->playersList->setItem(i, 0, name_item);
    m_ui->playersList->setItem(i, 1, status_item);
    m_ui->playersList->setItem(i, 2, ping_item);
    m_ui->playersList->setItem(i, 3, mapping_item);
    m_ui->playersList->setItem(i, 4, revision_item);

    if (p->pid == selection_pid)
      m_ui->playersList->selectRow(i);
  }

  if (m_old_player_count != m_player_count)
  {
    UpdateDiscordPresence();
    m_old_player_count = m_player_count;
  }

  if (!server)
    return;

  const bool is_local_ip_selected = m_ui->roomBox->currentIndex() > (m_use_traversal ? 1 : 0);
  if (is_local_ip_selected)
  {
    m_ui->hostcodeLabel->setText(QString::fromStdString(
        server->GetInterfaceHost(m_ui->roomBox->currentData().toString().toStdString())));
    m_ui->hostcodeActionButton->setEnabled(true);
    m_ui->hostcodeActionButton->setText(tr("Copy"));
    m_is_copy_button_retry = false;
  }
  else if (m_use_traversal)
  {
    switch (Common::g_TraversalClient->GetState())
    {
    case Common::TraversalClient::State::Connecting:
      m_ui->hostcodeLabel->setText(tr("Connecting"));
      m_ui->hostcodeActionButton->setEnabled(false);
      m_ui->hostcodeActionButton->setText(tr("..."));
      break;
    case Common::TraversalClient::State::Connected:
    {
      if (m_ui->roomBox->currentIndex() == 0)
      {
        // Display Room ID.
        const auto host_id = Common::g_TraversalClient->GetHostID();
        m_ui->hostcodeLabel->setText(
            QString::fromStdString(std::string(host_id.begin(), host_id.end())));
      }
      else
      {
        // Externally mapped IP and port are known when using the traversal server.
        m_ui->hostcodeLabel->setText(
            InetAddressToString(Common::g_TraversalClient->GetExternalAddress()));
      }

      m_ui->hostcodeActionButton->setEnabled(true);
      m_ui->hostcodeActionButton->setText(tr("Copy"));
      m_is_copy_button_retry = false;
      break;
    }
    case Common::TraversalClient::State::Failure:
      m_ui->hostcodeLabel->setText(tr("Error"));
      m_ui->hostcodeActionButton->setText(tr("Retry"));
      m_ui->hostcodeActionButton->setEnabled(true);
      m_is_copy_button_retry = true;
      break;
    }
  }
  else
  {
    // Display External IP.
    if (!m_external_ip_address->empty())
    {
      const int port = Settings::Instance().GetNetPlayServer()->GetPort();
      m_ui->hostcodeLabel->setText(QStringLiteral("%1:%2").arg(
          QString::fromStdString(*m_external_ip_address), QString::number(port)));
      m_ui->hostcodeActionButton->setEnabled(true);
    }
    else
    {
      m_ui->hostcodeLabel->setText(tr("Unknown"));
      m_ui->hostcodeActionButton->setEnabled(false);
    }

    m_ui->hostcodeActionButton->setText(tr("Copy"));
    m_is_copy_button_retry = false;
  }
}

// NetPlayUI methods

void NetPlayDialog::BootGame(const std::string& filename,
                             std::unique_ptr<BootSessionData> boot_session_data)
{
  m_got_stop_request = false;
  m_start_game_callback(filename, std::move(boot_session_data));
}

void NetPlayDialog::StopGame()
{
  if (m_got_stop_request)
    return;

  m_got_stop_request = true;
  emit Stop();
}

bool NetPlayDialog::IsHosting() const
{
  return Settings::Instance().GetNetPlayServer() != nullptr;
}

void NetPlayDialog::Update()
{
  QueueOnObject(this, &NetPlayDialog::UpdateGUI);
}

void NetPlayDialog::DisplayMessage(const QString& msg, const std::string& color, int duration)
{
  QueueOnObject(m_ui->chatEdit, [this, color, msg] {
    m_ui->chatEdit->append(QStringLiteral("<font color='%1'>%2</font>")
                               .arg(QString::fromStdString(color), msg.toHtmlEscaped()));
  });

  const QColor c(color.empty() ? QStringLiteral("white") : QString::fromStdString(color));

  if (Config::Get(Config::GFX_SHOW_NETPLAY_MESSAGES) &&
      Core::IsRunning(Core::System::GetInstance()))
  {
    if (const auto chat = g_netplay_chat_ui.load())
      chat->AppendChat(msg.toStdString(),
                       {static_cast<float>(c.redF()), static_cast<float>(c.greenF()),
                        static_cast<float>(c.blueF())});
  }
}

void NetPlayDialog::AppendChat(const std::string& msg)
{
  DisplayMessage(QString::fromStdString(msg), "");
  QApplication::alert(this);
}

void NetPlayDialog::OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                                    const std::string& netplay_name)
{
  QString qname = QString::fromStdString(netplay_name);
  QueueOnObject(this, [this, qname, netplay_name, &sync_identifier] {
    m_ui->gameButton->setText(qname);
    m_current_game_identifier = sync_identifier;
    m_current_game_name = netplay_name;
    UpdateDiscordPresence();
  });
  DisplayMessage(tr("Game changed to \"%1\"").arg(qname), "magenta");
}

void NetPlayDialog::OnMsgChangeGBARom(int pad, const NetPlay::GBAConfig& config)
{
  if (config.has_rom)
  {
    DisplayMessage(
        tr("GBA%1 ROM changed to \"%2\"").arg(pad + 1).arg(QString::fromStdString(config.title)),
        "magenta");
  }
  else
  {
    DisplayMessage(tr("GBA%1 ROM disabled").arg(pad + 1), "magenta");
  }
}

void NetPlayDialog::GameStatusChanged(bool running)
{
  QueueOnObject(this, [this, running] { SetOptionsEnabled(!running); });
}

void NetPlayDialog::SetOptionsEnabled(bool enabled)
{
  if (Settings::Instance().GetNetPlayServer())
  {
    m_ui->startButton->setEnabled(enabled);
    m_ui->gameButton->setEnabled(enabled);
    m_ui->actionSavedataNone->setEnabled(enabled);
    m_ui->actionSavedataLoadOnly->setEnabled(enabled);
    m_ui->actionSavedataLoadWrite->setEnabled(enabled);
    m_ui->actionSavedataAllWii->setEnabled(enabled);
    m_ui->actionSyncCodes->setEnabled(enabled);
    m_ui->assignPortsButton->setEnabled(enabled);
    m_ui->actionStrictSettingsSync->setEnabled(enabled);
    m_ui->actionHostInputAuthority->setEnabled(enabled);
    m_ui->actionGolfMode->setEnabled(enabled);
    m_ui->actionFixedDelay->setEnabled(enabled);
  }

  m_ui->actionRecordInputs->setEnabled(enabled);
}

void NetPlayDialog::OnMsgStartGame()
{
  DisplayMessage(tr("Started game"), "green");

  g_netplay_chat_ui = std::make_shared<NetPlayChatUI>([this](const std::string& message) {
    QueueOnObject(this, [this, message] { SendMessage(message); });
  });

  if (m_host_input_authority && Settings::Instance().GetNetPlayClient()->GetNetSettings().golf_mode)
  {
    g_netplay_golf_ui = std::make_shared<NetPlayGolfUI>(Settings::Instance().GetNetPlayClient());
  }

  QueueOnObject(this, [this] {
    const auto client = Settings::Instance().GetNetPlayClient();

    if (client)
    {
      if (const auto game = FindGameFile(m_current_game_identifier))
        client->StartGame(game->GetFilePath());
      else
        PanicAlertFmtT("Selected game doesn't exist in game list!");
    }
    UpdateDiscordPresence();
  });
}

void NetPlayDialog::OnMsgStopGame()
{
  g_netplay_chat_ui.store(nullptr);
  g_netplay_golf_ui.store(nullptr);
  QueueOnObject(this, [this] { UpdateDiscordPresence(); });
}

void NetPlayDialog::OnMsgPowerButton()
{
  if (!Core::IsRunning(Core::System::GetInstance()))
    return;
  QueueOnObject(this, [] { UICommon::TriggerSTMPowerEvent(); });
}

void NetPlayDialog::OnPlayerConnect(const std::string& player)
{
  DisplayMessage(tr("%1 has joined").arg(QString::fromStdString(player)), "darkcyan");
}

void NetPlayDialog::OnPlayerDisconnect(const std::string& player)
{
  DisplayMessage(tr("%1 has left").arg(QString::fromStdString(player)), "darkcyan");
}

void NetPlayDialog::OnPadBufferChanged(u32 buffer)
{
  QueueOnObject(this, [this, buffer] {
    const QSignalBlocker blocker(m_ui->bufferSizeSpinBox);
    m_ui->bufferSizeSpinBox->setValue(buffer);
  });
  DisplayMessage(m_host_input_authority ? tr("Max buffer size changed to %1").arg(buffer) :
                                          tr("Buffer size changed to %1").arg(buffer),
                 "darkcyan");

  m_buffer_size = static_cast<int>(buffer);
}

void NetPlayDialog::OnHostInputAuthorityChanged(bool enabled)
{
  m_host_input_authority = enabled;
  DisplayMessage(enabled ? tr("Host input authority enabled") : tr("Host input authority disabled"),
                 "");

  QueueOnObject(this, [this, enabled] {
    const bool is_hosting = IsHosting();
    const bool enable_buffer = is_hosting != enabled;

    if (is_hosting)
    {
      m_ui->bufferSizeSpinBox->setEnabled(enable_buffer);
      m_ui->bufferLabel->setEnabled(enable_buffer);
      m_ui->bufferSizeSpinBox->setHidden(false);
      m_ui->bufferLabel->setHidden(false);
    }
    else
    {
      m_ui->bufferSizeSpinBox->setEnabled(true);
      m_ui->bufferLabel->setEnabled(true);
      m_ui->bufferSizeSpinBox->setHidden(!enable_buffer);
      m_ui->bufferLabel->setHidden(!enable_buffer);
    }

    m_ui->bufferLabel->setText(enabled ? tr("Max Buffer:") : tr("Buffer:"));
    if (enabled)
    {
      const QSignalBlocker blocker(m_ui->bufferSizeSpinBox);
      m_ui->bufferSizeSpinBox->setValue(Config::Get(Config::NETPLAY_CLIENT_BUFFER_SIZE));
    }
  });
}

void NetPlayDialog::OnDesync(u32 frame, const std::string& player)
{
  DisplayMessage(tr("Possible desync detected: %1 might have desynced at frame %2")
                     .arg(QString::fromStdString(player), QString::number(frame)),
                 "red", OSD::Duration::VERY_LONG);
}

void NetPlayDialog::OnConnectionLost()
{
  DisplayMessage(tr("Lost connection to NetPlay server..."), "red");
}

void NetPlayDialog::OnConnectionError(const std::string& message)
{
  QueueOnObject(this, [this, message] {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Failed to connect to server: %1").arg(tr(message.c_str())));
  });
}

void NetPlayDialog::OnTraversalError(Common::TraversalClient::FailureReason error)
{
  QueueOnObject(this, [this, error] {
    switch (error)
    {
    case Common::TraversalClient::FailureReason::BadHost:
      ModalMessageBox::critical(this, tr("Traversal Error"), tr("Couldn't look up central server"));
      QDialog::reject();
      break;
    case Common::TraversalClient::FailureReason::VersionTooOld:
      ModalMessageBox::critical(this, tr("Traversal Error"),
                                tr("Dolphin is too old for traversal server"));
      QDialog::reject();
      break;
    case Common::TraversalClient::FailureReason::ServerForgotAboutUs:
    case Common::TraversalClient::FailureReason::SocketSendError:
    case Common::TraversalClient::FailureReason::ResendTimeout:
      UpdateGUI();
      break;
    }
  });
}

void NetPlayDialog::OnTraversalStateChanged(Common::TraversalClient::State state)
{
  switch (state)
  {
  case Common::TraversalClient::State::Connected:
  case Common::TraversalClient::State::Failure:
    UpdateDiscordPresence();
    break;
  default:
    break;
  }
}

void NetPlayDialog::OnGameStartAborted()
{
  QueueOnObject(this, [this] { SetOptionsEnabled(true); });
}

void NetPlayDialog::OnGolferChanged(const bool is_golfer, const std::string& golfer_name)
{
  if (m_host_input_authority)
  {
    QueueOnObject(this, [this, is_golfer] {
      m_ui->bufferSizeSpinBox->setEnabled(!is_golfer);
      m_ui->bufferLabel->setEnabled(!is_golfer);
    });
  }

  if (!golfer_name.empty())
    DisplayMessage(tr("%1 is now golfing").arg(QString::fromStdString(golfer_name)), "");
}

void NetPlayDialog::OnTtlDetermined(u8 ttl)
{
  DisplayMessage(tr("Using TTL %1 for probe packet").arg(QString::number(ttl)), "");
}

bool NetPlayDialog::IsRecording()
{
  const std::optional<bool> is_recording =
      RunOnObject(m_ui->actionRecordInputs, &QAction::isChecked);
  if (is_recording)
    return *is_recording;
  return false;
}

std::shared_ptr<const UICommon::GameFile>
NetPlayDialog::FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
                            NetPlay::SyncIdentifierComparison* found)
{
  NetPlay::SyncIdentifierComparison temp;
  if (!found)
    found = &temp;

  *found = NetPlay::SyncIdentifierComparison::DifferentGame;

  const std::optional<std::shared_ptr<const UICommon::GameFile>> game_file =
      RunOnObject(this, [this, &sync_identifier, found] {
        for (int i = 0; i < m_game_list_model.rowCount(QModelIndex()); i++)
        {
          auto file = m_game_list_model.GetGameFile(i);
          *found = std::min(*found, file->CompareSyncIdentifier(sync_identifier));
          if (*found == NetPlay::SyncIdentifierComparison::SameGame)
            return file;
        }
        return static_cast<std::shared_ptr<const UICommon::GameFile>>(nullptr);
      });
  if (game_file)
    return *game_file;
  return nullptr;
}

std::string NetPlayDialog::FindGBARomPath(const std::array<u8, 20>& hash, std::string_view title,
                                          int device_number)
{
#ifdef HAS_LIBMGBA
  const auto result = RunOnObject(this, [&, this] {
    std::string rom_path;
    std::array<u8, 20> rom_hash;
    std::string rom_title;
    for (size_t i = device_number; i < static_cast<size_t>(device_number) + 4; ++i)
    {
      rom_path = Config::Get(Config::MAIN_GBA_ROM_PATHS[i % 4]);
      if (!rom_path.empty() && HW::GBA::Core::GetRomInfo(rom_path.c_str(), rom_hash, rom_title) &&
          rom_hash == hash && rom_title == title)
      {
        return rom_path;
      }
    }
    while (!(rom_path = GameCubePane::GetOpenGBARom(title)).empty())
    {
      if (HW::GBA::Core::GetRomInfo(rom_path.c_str(), rom_hash, rom_title))
      {
        if (rom_hash == hash && rom_title == title)
          return rom_path;
        ModalMessageBox::critical(
            this, tr("Error"),
            QString::fromStdString(Common::FmtFormatT(
                "Mismatched ROMs\n"
                "Selected: {0}\n- Title: {1}\n- Hash: {2:02X}\n"
                "Expected:\n- Title: {3}\n- Hash: {4:02X}",
                rom_path, rom_title, fmt::join(rom_hash, ""), title, fmt::join(hash, ""))));
      }
      else
      {
        ModalMessageBox::critical(
            this, tr("Error"), tr("%1 is not a valid ROM").arg(QString::fromStdString(rom_path)));
      }
    }
    return std::string();
  });
  if (result)
    return *result;
#endif
  return {};
}

void NetPlayDialog::LoadSettings()
{
  const int buffer_size = Config::Get(Config::NETPLAY_BUFFER_SIZE);
  const bool savedata_load = Config::Get(Config::NETPLAY_SAVEDATA_LOAD);
  const bool savedata_write = Config::Get(Config::NETPLAY_SAVEDATA_WRITE);
  const bool sync_all_wii_saves = Config::Get(Config::NETPLAY_SAVEDATA_SYNC_ALL_WII);
  const bool sync_codes = Config::Get(Config::NETPLAY_SYNC_CODES);
  const bool record_inputs = Config::Get(Config::NETPLAY_RECORD_INPUTS);
  const bool strict_settings_sync = Config::Get(Config::NETPLAY_STRICT_SETTINGS_SYNC);
  const bool golf_mode_overlay = Config::Get(Config::NETPLAY_GOLF_MODE_OVERLAY);
  const bool hide_remote_gbas = Config::Get(Config::NETPLAY_HIDE_REMOTE_GBAS);

  m_ui->bufferSizeSpinBox->setValue(buffer_size);

  if (!savedata_load)
    m_ui->actionSavedataNone->setChecked(true);
  else if (!savedata_write)
    m_ui->actionSavedataLoadOnly->setChecked(true);
  else
    m_ui->actionSavedataLoadWrite->setChecked(true);
  m_ui->actionSavedataAllWii->setChecked(sync_all_wii_saves);

  m_ui->actionSyncCodes->setChecked(sync_codes);
  m_ui->actionRecordInputs->setChecked(record_inputs);
  m_ui->actionStrictSettingsSync->setChecked(strict_settings_sync);
  m_ui->actionGolfModeOverlay->setChecked(golf_mode_overlay);
  m_ui->actionHideRemoteGBAs->setChecked(hide_remote_gbas);

  const std::string network_mode = Config::Get(Config::NETPLAY_NETWORK_MODE);

  if (network_mode == "fixeddelay")
  {
    m_ui->actionFixedDelay->setChecked(true);
  }
  else if (network_mode == "hostinputauthority")
  {
    m_ui->actionHostInputAuthority->setChecked(true);
  }
  else if (network_mode == "golf")
  {
    m_ui->actionGolfMode->setChecked(true);
  }
  else
  {
    WARN_LOG_FMT(NETPLAY, "Unknown network mode '{}', using 'fixeddelay'", network_mode);
    m_ui->actionFixedDelay->setChecked(true);
  }
}

void NetPlayDialog::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  if (m_host_input_authority)
    Config::SetBase(Config::NETPLAY_CLIENT_BUFFER_SIZE, m_ui->bufferSizeSpinBox->value());
  else
    Config::SetBase(Config::NETPLAY_BUFFER_SIZE, m_ui->bufferSizeSpinBox->value());

  const bool write_savedata = m_ui->actionSavedataLoadWrite->isChecked();
  const bool load_savedata = write_savedata || m_ui->actionSavedataLoadOnly->isChecked();
  Config::SetBase(Config::NETPLAY_SAVEDATA_LOAD, load_savedata);
  Config::SetBase(Config::NETPLAY_SAVEDATA_WRITE, write_savedata);

  Config::SetBase(Config::NETPLAY_SAVEDATA_SYNC_ALL_WII, m_ui->actionSavedataAllWii->isChecked());
  Config::SetBase(Config::NETPLAY_SYNC_CODES, m_ui->actionSyncCodes->isChecked());
  Config::SetBase(Config::NETPLAY_RECORD_INPUTS, m_ui->actionRecordInputs->isChecked());
  Config::SetBase(Config::NETPLAY_STRICT_SETTINGS_SYNC,
                  m_ui->actionStrictSettingsSync->isChecked());
  Config::SetBase(Config::NETPLAY_GOLF_MODE_OVERLAY, m_ui->actionGolfModeOverlay->isChecked());
  Config::SetBase(Config::NETPLAY_HIDE_REMOTE_GBAS, m_ui->actionHideRemoteGBAs->isChecked());

  std::string network_mode;
  if (m_ui->actionFixedDelay->isChecked())
  {
    network_mode = "fixeddelay";
  }
  else if (m_ui->actionHostInputAuthority->isChecked())
  {
    network_mode = "hostinputauthority";
  }
  else if (m_ui->actionGolfMode->isChecked())
  {
    network_mode = "golf";
  }

  Config::SetBase(Config::NETPLAY_NETWORK_MODE, network_mode);
}

void NetPlayDialog::ShowGameDigestDialog(const std::string& title)
{
  QueueOnObject(this, [this, title] {
    m_ui->gameDigestMenu->setEnabled(false);

    if (m_game_digest_dialog->isVisible())
      m_game_digest_dialog->close();

    m_game_digest_dialog->show(QString::fromStdString(title));
  });
}

void NetPlayDialog::SetGameDigestProgress(int pid, int progress)
{
  QueueOnObject(this, [this, pid, progress] {
    if (m_game_digest_dialog->isVisible())
      m_game_digest_dialog->SetProgress(pid, progress);
  });
}

void NetPlayDialog::SetGameDigestResult(int pid, const std::string& result)
{
  QueueOnObject(this, [this, pid, result] {
    m_game_digest_dialog->SetResult(pid, result);
    m_ui->gameDigestMenu->setEnabled(true);
  });
}

void NetPlayDialog::AbortGameDigest()
{
  QueueOnObject(this, [this] {
    m_game_digest_dialog->close();
    m_ui->gameDigestMenu->setEnabled(true);
  });
}

void NetPlayDialog::ShowChunkedProgressDialog(const std::string& title, const u64 data_size,
                                              std::span<const int> players)
{
  QueueOnObject(this, [this, title, data_size, players] {
    if (m_chunked_progress_dialog->isVisible())
      m_chunked_progress_dialog->done(QDialog::Accepted);

    m_chunked_progress_dialog->show(QString::fromStdString(title), data_size, players);
  });
}

void NetPlayDialog::HideChunkedProgressDialog()
{
  QueueOnObject(this, [this] { m_chunked_progress_dialog->done(QDialog::Accepted); });
}

void NetPlayDialog::SetChunkedProgress(const int pid, const u64 progress)
{
  QueueOnObject(this, [this, pid, progress] {
    if (m_chunked_progress_dialog->isVisible())
      m_chunked_progress_dialog->SetProgress(pid, progress);
  });
}

void NetPlayDialog::SetHostWiiSyncData(std::vector<u64> titles, std::string redirect_folder)
{
  const auto client = Settings::Instance().GetNetPlayClient();
  if (client)
    client->SetWiiSyncData(nullptr, std::move(titles), std::move(redirect_folder));
}
