// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ctime>
#include <memory>

#include <QDialog>

namespace Ui
{
class DiscordJoinRequestDialog;
}

class DiscordJoinRequestDialog : public QDialog
{
  Q_OBJECT
public:
  explicit DiscordJoinRequestDialog(QWidget* parent, const std::string& id,
                                    const std::string& discord_tag, const std::string& avatar);
  ~DiscordJoinRequestDialog() override;
  std::time_t GetCloseTimestamp() const;

  static constexpr std::time_t s_max_lifetime_seconds = 30;

private:
  void ConnectWidgets();
  void Reply(int reply);

  std::unique_ptr<Ui::DiscordJoinRequestDialog> m_ui;

  const std::string m_user_id;
  const std::time_t m_close_timestamp;
};
