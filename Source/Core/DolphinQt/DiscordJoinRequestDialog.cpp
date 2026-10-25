// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_DISCORD_PRESENCE

#include "DolphinQt/DiscordJoinRequestDialog.h"

#include <QLabel>
#include <QPixmap>
#include <QPushButton>

#include <discord_rpc.h>
#include <fmt/format.h>

#include "Common/HttpRequest.h"

#include "ui_DiscordJoinRequestDialog.h"

DiscordJoinRequestDialog::DiscordJoinRequestDialog(QWidget* parent, const std::string& id,
                                                   const std::string& discord_tag,
                                                   const std::string& avatar)
    : QDialog(parent), m_ui(std::make_unique<Ui::DiscordJoinRequestDialog>()), m_user_id(id),
      m_close_timestamp(std::time(nullptr) + s_max_lifetime_seconds)
{
  m_ui->setupUi(this);

  QPixmap avatar_pixmap;

  if (!avatar.empty())
  {
    const std::string avatar_endpoint =
        fmt::format("https://cdn.discordapp.com/avatars/{}/{}.png", id, avatar);

    Common::HttpRequest request;
    Common::HttpRequest::Response response = request.Get(avatar_endpoint);

    if (response.has_value())
      avatar_pixmap.loadFromData(response->data(), static_cast<uint>(response->size()), "png");
  }

  m_ui->requestLabel->setText(
      tr("%1\nwants to join your party.").arg(QString::fromStdString(discord_tag)));
  if (!avatar_pixmap.isNull())
  {
    m_ui->avatarLabel->setPixmap(avatar_pixmap);
    m_ui->avatarLabel->show();
  }

  ConnectWidgets();
}

DiscordJoinRequestDialog::~DiscordJoinRequestDialog() = default;

std::time_t DiscordJoinRequestDialog::GetCloseTimestamp() const
{
  return m_close_timestamp;
}

void DiscordJoinRequestDialog::ConnectWidgets()
{
  connect(m_ui->inviteButton, &QPushButton::clicked, [this] { Reply(DISCORD_REPLY_YES); });
  connect(m_ui->declineButton, &QPushButton::clicked, [this] { Reply(DISCORD_REPLY_NO); });
  connect(m_ui->ignoreButton, &QPushButton::clicked, [this] { Reply(DISCORD_REPLY_IGNORE); });
  connect(this, &QDialog::rejected, [this] { Reply(DISCORD_REPLY_IGNORE); });
}

void DiscordJoinRequestDialog::Reply(int reply)
{
  Discord_Respond(m_user_id.c_str(), reply);
  close();
}

#endif
