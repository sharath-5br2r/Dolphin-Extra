// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/NetworkWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include <bit>

#include "Common/FileUtil.h"
#include "Common/Network.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

#include "ui_NetworkWidget.h"

namespace
{
QTableWidgetItem* GetSocketDomain(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr sa;
  socklen_t sa_len = sizeof(sa);
  const int ret = getsockname(host_fd, &sa, &sa_len);
  if (ret != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  switch (sa.sa_family)
  {
  case 2:
    return new QTableWidgetItem(QStringLiteral("AF_INET"));
  case 23:
    return new QTableWidgetItem(QStringLiteral("AF_INET6"));
  default:
    return new QTableWidgetItem(QString::number(sa.sa_family));
  }
}

QTableWidgetItem* GetSocketType(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  int so_type;
  socklen_t opt_len = sizeof(so_type);
  const int ret =
      getsockopt(host_fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&so_type), &opt_len);
  if (ret != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  switch (so_type)
  {
  case 1:
    return new QTableWidgetItem(QStringLiteral("SOCK_STREAM"));
  case 2:
    return new QTableWidgetItem(QStringLiteral("SOCK_DGRAM"));
  default:
    return new QTableWidgetItem(QString::number(so_type));
  }
}

QTableWidgetItem* GetSocketState(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  if (getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) == 0)
    return new QTableWidgetItem(QTableWidget::tr("Connected"));

  int so_accept = 0;
  socklen_t opt_len = sizeof(so_accept);
  const int ret =
      getsockopt(host_fd, SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<char*>(&so_accept), &opt_len);
  if (ret == 0 && so_accept > 0)
    return new QTableWidgetItem(QTableWidget::tr("Listening"));
  return new QTableWidgetItem(QTableWidget::tr("Unbound"));
}

QTableWidgetItem* GetSocketBlocking(const IOS::HLE::WiiSockMan& socket_manager, s32 wii_fd)
{
  if (socket_manager.GetHostSocket(wii_fd) < 0)
    return new QTableWidgetItem();
  const bool is_blocking = socket_manager.IsSocketBlocking(wii_fd);
  return new QTableWidgetItem(is_blocking ? QTableWidget::tr("Yes") : QTableWidget::tr("No"));
}

QString GetAddressAndPort(const sockaddr_in& addr)
{
  char buffer[16];
  const char* addr_str = inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
  if (!addr_str)
    return {};

  return QStringLiteral("%1:%2").arg(QString::fromLatin1(addr_str)).arg(ntohs(addr.sin_port));
}

QTableWidgetItem* GetSocketName(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr_in sock_addr;
  socklen_t sock_addr_len = sizeof(sockaddr_in);
  if (getsockname(host_fd, reinterpret_cast<sockaddr*>(&sock_addr), &sock_addr_len) != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  const QString sock_name = GetAddressAndPort(sock_addr);
  if (sock_name.isEmpty())
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  if (getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) != 0)
    return new QTableWidgetItem(sock_name);

  const QString peer_name = GetAddressAndPort(peer_addr);
  if (peer_name.isEmpty())
    return new QTableWidgetItem(sock_name);

  return new QTableWidgetItem(QStringLiteral("%1->%2").arg(sock_name).arg(peer_name));
}

QTableWidgetItem* GetSocketRedirections(s32 host_fd,
                                        const AMMediaboard::IPRedirections& ip_redirections)
{
  if (host_fd < 0 || ip_redirections.empty())
    return new QTableWidgetItem();

  sockaddr_in sock_addr;
  socklen_t sock_addr_len = sizeof(sockaddr_in);
  if (getsockname(host_fd, reinterpret_cast<sockaddr*>(&sock_addr), &sock_addr_len) != 0)
    return new QTableWidgetItem();
  const Common::IPv4Port sock_ip_port{std::bit_cast<Common::IPAddress>(sock_addr.sin_addr),
                                      sock_addr.sin_port};

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  bool has_peer =
      getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) == 0;
  const Common::IPv4Port peer_ip_port =
      has_peer ? Common::IPv4Port{std::bit_cast<Common::IPAddress>(peer_addr.sin_addr),
                                  peer_addr.sin_port} :
                 Common::IPv4Port{};

  QStringList sock_rules;
  QStringList peer_rules;
  for (const auto& rule : ip_redirections)
  {
    if (rule.replacement.IsMatch(sock_ip_port))
      sock_rules << QString::fromStdString(rule.ToString());
    if (!has_peer)
      continue;
    if (rule.replacement.IsMatch(peer_ip_port))
      peer_rules << QString::fromStdString(rule.ToString());
  }

  if (sock_rules.isEmpty() && peer_rules.isEmpty())
    return new QTableWidgetItem();
  if (peer_rules.isEmpty())
    return new QTableWidgetItem(sock_rules.join(QStringLiteral("\n")));
  return new QTableWidgetItem(QStringLiteral("Sock rules:\n%1\n"
                                             "\nPeer rules:%2")
                                  .arg(sock_rules.join(QStringLiteral("\n")))
                                  .arg(peer_rules.join(QStringLiteral("\n"))));
}
}  // namespace

NetworkWidget::NetworkWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Network"));
  setObjectName(QStringLiteral("network"));

  setHidden(!Settings::Instance().IsNetworkVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("networkwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("networkwidget/floating")).toBool());

  ConnectWidgets();

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &NetworkWidget::Update);

  connect(&Settings::Instance(), &Settings::NetworkVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsNetworkVisible());
  });
}

NetworkWidget::~NetworkWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("networkwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("networkwidget/floating"), isFloating());
}

void NetworkWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetNetworkVisible(false);
}

void NetworkWidget::showEvent(QShowEvent*)
{
  Update();
}

void NetworkWidget::CreateWidgets()
{
  auto* widget = new QWidget;
  Ui::NetworkWidget ui;
  ui.setupUi(widget);
  m_socket_table = ui.socketTable;
  m_ssl_table = ui.sslTable;
  m_dump_format_combo = ui.dumpFormatCombo;
  m_dump_ssl_read_checkbox = ui.dumpSslReadCheckBox;
  m_dump_ssl_write_checkbox = ui.dumpSslWriteCheckBox;
  m_dump_root_ca_checkbox = ui.dumpRootCaCheckBox;
  m_dump_peer_cert_checkbox = ui.dumpPeerCertCheckBox;
  m_verify_certificates_checkbox = ui.verifyCertificatesCheckBox;
  m_dump_bba_checkbox = ui.dumpBbaCheckBox;
  m_open_dump_folder = ui.openDumpFolderButton;

  // i18n: FD stands for file descriptor (and in this case refers to sockets, not regular files)
  const QStringList socket_headers{tr("FD"),       tr("Domain"), tr("Type"),        tr("State"),
                                   tr("Blocking"), tr("Name"),   tr("Redirections")};
  m_socket_table->setColumnCount(static_cast<int>(socket_headers.size()));
  m_socket_table->setHorizontalHeaderLabels(socket_headers);
  m_socket_table->verticalHeader()->hide();

  const QStringList ssl_headers{tr("ID"),    tr("Domain"), tr("Type"),
                                tr("State"), tr("Name"),   tr("Hostname")};
  m_ssl_table->setColumnCount(static_cast<int>(ssl_headers.size()));
  m_ssl_table->setHorizontalHeaderLabels(ssl_headers);
  m_ssl_table->verticalHeader()->hide();

  setWidget(widget);

  Update();
}

void NetworkWidget::ConnectWidgets()
{
  connect(m_dump_format_combo, &QComboBox::currentIndexChanged, this,
          &NetworkWidget::OnDumpFormatComboChanged);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  using CheckState = Qt::CheckState;
  static constexpr auto checkStateChanged = &QCheckBox::checkStateChanged;
#else
  using CheckState = int;
  static constexpr auto checkStateChanged = &QCheckBox::stateChanged;
#endif

  connect(m_dump_ssl_read_checkbox, checkStateChanged, [](CheckState state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_READ, state == Qt::Checked);
  });
  connect(m_dump_ssl_write_checkbox, checkStateChanged, [](CheckState state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_WRITE, state == Qt::Checked);
  });
  connect(m_dump_root_ca_checkbox, checkStateChanged, [](CheckState state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA, state == Qt::Checked);
  });
  connect(m_dump_peer_cert_checkbox, checkStateChanged, [](CheckState state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_DUMP_PEER_CERT, state == Qt::Checked);
  });
  connect(m_verify_certificates_checkbox, checkStateChanged, [](CheckState state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES, state == Qt::Checked);
  });
  connect(m_dump_bba_checkbox, checkStateChanged, [](CheckState state) {
    Config::SetBaseOrCurrent(Config::MAIN_NETWORK_DUMP_BBA, state == Qt::Checked);
  });

  connect(m_open_dump_folder, &QPushButton::clicked, [] {
    const std::string location = File::GetUserPath(D_DUMPSSL_IDX);
    const QUrl url = QUrl::fromLocalFile(QString::fromStdString(location));
    QDesktopServices::openUrl(url);
  });
}

void NetworkWidget::UpdateWiiSocketTable(Core::System& system)
{
  // Show Wii socket blocking state
  m_socket_table->showColumn(4);
  // Hide Triforce IP redirections
  m_socket_table->hideColumn(6);

  auto* ios = system.GetIOS();
  if (!ios)
    return;

  auto socket_manager = ios->GetSocketManager();
  if (!socket_manager)
    return;

  for (s32 wii_fd = 0; wii_fd < IOS::HLE::WII_SOCKET_FD_MAX; wii_fd++)
  {
    m_socket_table->insertRow(wii_fd);
    const s32 host_fd = socket_manager->GetHostSocket(wii_fd);
    m_socket_table->setItem(wii_fd, 0, new QTableWidgetItem(QString::number(wii_fd)));
    m_socket_table->setItem(wii_fd, 1, GetSocketDomain(host_fd));
    m_socket_table->setItem(wii_fd, 2, GetSocketType(host_fd));
    m_socket_table->setItem(wii_fd, 3, GetSocketState(host_fd));
    m_socket_table->setItem(wii_fd, 4, GetSocketBlocking(*socket_manager, wii_fd));
    m_socket_table->setItem(wii_fd, 5, GetSocketName(host_fd));
  }

  for (s32 ssl_id = 0; ssl_id < IOS::HLE::NET_SSL_MAXINSTANCES; ssl_id++)
  {
    m_ssl_table->insertRow(ssl_id);
    s32 host_fd = -1;
    if (IOS::HLE::IsSSLIDValid(ssl_id))
    {
      const auto& ssl = IOS::HLE::NetSSLDevice::_SSL[ssl_id];
      host_fd = ssl.hostfd;
      m_ssl_table->setItem(ssl_id, 5, new QTableWidgetItem(QString::fromStdString(ssl.hostname)));
    }
    m_ssl_table->setItem(ssl_id, 0, new QTableWidgetItem(QString::number(ssl_id)));
    m_ssl_table->setItem(ssl_id, 1, GetSocketDomain(host_fd));
    m_ssl_table->setItem(ssl_id, 2, GetSocketType(host_fd));
    m_ssl_table->setItem(ssl_id, 3, GetSocketState(host_fd));
    m_ssl_table->setItem(ssl_id, 4, GetSocketName(host_fd));
  }
}

void NetworkWidget::UpdateTriforceSocketTable()
{
  // No easy way to get socket blocking state on Windows
  m_socket_table->hideColumn(4);
  // Show active IP redirections
  m_socket_table->showColumn(6);
  const auto ip_redirections = AMMediaboard::GetIPRedirections();
  for (s32 triforce_fd = 0; triforce_fd != AMMediaboard::SOCKET_FD_MAX; ++triforce_fd)
  {
    m_socket_table->insertRow(triforce_fd);
    const s32 host_fd = AMMediaboard::DebuggerGetSocket(triforce_fd);
    m_socket_table->setItem(triforce_fd, 0, new QTableWidgetItem(QString::number(triforce_fd)));
    m_socket_table->setItem(triforce_fd, 1, GetSocketDomain(host_fd));
    m_socket_table->setItem(triforce_fd, 2, GetSocketType(host_fd));
    m_socket_table->setItem(triforce_fd, 3, GetSocketState(host_fd));
    m_socket_table->setItem(triforce_fd, 4, new QTableWidgetItem(QTableWidget::tr("Unknown")));
    m_socket_table->setItem(triforce_fd, 5, GetSocketName(host_fd));
    m_socket_table->setItem(triforce_fd, 6, GetSocketRedirections(host_fd, ip_redirections));
  }
}

void NetworkWidget::Update()
{
  if (!isVisible())
    return;

  auto& system = Core::System::GetInstance();
  if (Core::GetState(system) != Core::State::Paused)
  {
    m_socket_table->setDisabled(true);
    m_ssl_table->setDisabled(true);
    return;
  }

  // needed because there's a race condition on the IOS instance otherwise
  const Core::CPUThreadGuard guard(system);
  m_socket_table->setDisabled(false);
  m_socket_table->setRowCount(0);
  m_ssl_table->setRowCount(0);
  if (system.IsTriforce())
  {
    UpdateTriforceSocketTable();
  }
  else if (system.IsWii())
  {
    m_ssl_table->setDisabled(false);
    UpdateWiiSocketTable(system);
  }
  m_socket_table->resizeColumnsToContents();
  m_socket_table->resizeRowsToContents();
  m_ssl_table->resizeColumnsToContents();

  const bool is_pcap = Config::Get(Config::MAIN_NETWORK_DUMP_AS_PCAP);
  const bool is_ssl_read = Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ);
  const bool is_ssl_write = Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE);

  m_dump_ssl_read_checkbox->setChecked(is_ssl_read);
  m_dump_ssl_write_checkbox->setChecked(is_ssl_write);
  m_dump_root_ca_checkbox->setChecked(Config::Get(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA));
  m_dump_peer_cert_checkbox->setChecked(Config::Get(Config::MAIN_NETWORK_SSL_DUMP_PEER_CERT));
  m_verify_certificates_checkbox->setChecked(
      Config::Get(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES));

  const int combo_index = int([is_pcap, is_ssl_read, is_ssl_write]() -> FormatComboId {
    if (is_pcap)
      return FormatComboId::PCAP;

    if (is_ssl_read && is_ssl_write)
      return FormatComboId::BinarySSL;

    if (is_ssl_read)
      return FormatComboId::BinarySSLRead;

    if (is_ssl_write)
      return FormatComboId::BinarySSLWrite;

    return FormatComboId::None;
  }());
  m_dump_format_combo->setCurrentIndex(combo_index);
}

void NetworkWidget::OnDumpFormatComboChanged(int index)
{
  const auto combo_id = static_cast<FormatComboId>(index);

  switch (combo_id)
  {
  case FormatComboId::PCAP:
    break;
  case FormatComboId::BinarySSL:
    m_dump_ssl_read_checkbox->setChecked(true);
    m_dump_ssl_write_checkbox->setChecked(true);
    m_dump_bba_checkbox->setChecked(false);
    break;
  case FormatComboId::BinarySSLRead:
    m_dump_ssl_read_checkbox->setChecked(true);
    m_dump_ssl_write_checkbox->setChecked(false);
    m_dump_bba_checkbox->setChecked(false);
    break;
  case FormatComboId::BinarySSLWrite:
    m_dump_ssl_read_checkbox->setChecked(false);
    m_dump_ssl_write_checkbox->setChecked(true);
    m_dump_bba_checkbox->setChecked(false);
    break;
  default:
    m_dump_ssl_read_checkbox->setChecked(false);
    m_dump_ssl_write_checkbox->setChecked(false);
    m_dump_bba_checkbox->setChecked(false);
    break;
  }
  // Enable raw or decrypted SSL choices for PCAP
  const bool is_pcap = combo_id == FormatComboId::PCAP;
  m_dump_ssl_read_checkbox->setEnabled(is_pcap);
  m_dump_ssl_write_checkbox->setEnabled(is_pcap);
  m_dump_bba_checkbox->setEnabled(is_pcap);
  Config::SetBaseOrCurrent(Config::MAIN_NETWORK_DUMP_AS_PCAP, is_pcap);
}
