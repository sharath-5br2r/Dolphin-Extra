// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <QDialog>

#include "Common/Lazy.h"
#include "Core/NetPlayClient.h"
#include "DolphinQt/GameList/GameListModel.h"
#include "VideoCommon/OnScreenDisplay.h"

class BootSessionData;
class ChunkedProgressDialog;
class GameDigestDialog;
class PadMappingDialog;
class QActionGroup;

namespace Ui
{
class NetPlayDialog;
}

class NetPlayDialog : public QDialog, public NetPlay::NetPlayUI
{
  Q_OBJECT
public:
  using StartGameCallback = std::function<void(const std::string& path,
                                               std::unique_ptr<BootSessionData> boot_session_data)>;

  explicit NetPlayDialog(const GameListModel& game_list_model,
                         StartGameCallback start_game_callback, QWidget* parent = nullptr);
  ~NetPlayDialog() override;

  void show(std::string nickname, bool use_traversal);
  void reject() override;

  // NetPlayUI methods
  void BootGame(const std::string& filename,
                std::unique_ptr<BootSessionData> boot_session_data) override;
  void StopGame() override;
  bool IsHosting() const override;

  void Update() override;
  void AppendChat(const std::string& msg) override;

  void OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                       const std::string& netplay_name) override;
  void OnMsgChangeGBARom(int pad, const NetPlay::GBAConfig& config) override;
  void OnMsgStartGame() override;
  void OnMsgStopGame() override;
  void OnMsgPowerButton() override;
  void OnPlayerConnect(const std::string& player) override;
  void OnPlayerDisconnect(const std::string& player) override;
  void OnPadBufferChanged(u32 buffer) override;
  void OnHostInputAuthorityChanged(bool enabled) override;
  void OnDesync(u32 frame, const std::string& player) override;
  void OnConnectionLost() override;
  void OnConnectionError(const std::string& message) override;
  void OnTraversalError(Common::TraversalClient::FailureReason error) override;
  void OnTraversalStateChanged(Common::TraversalClient::State state) override;
  void OnGameStartAborted() override;
  void OnGolferChanged(bool is_golfer, const std::string& golfer_name) override;
  void OnTtlDetermined(u8 ttl) override;

  void OnIndexAdded(bool success, const std::string error) override;
  void OnIndexRefreshFailed(const std::string error) override;

  bool IsRecording() override;
  std::shared_ptr<const UICommon::GameFile>
  FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
               NetPlay::SyncIdentifierComparison* found = nullptr) override;
  std::string FindGBARomPath(const std::array<u8, 20>& hash, std::string_view title,
                             int device_number) override;

  void LoadSettings();
  void SaveSettings();

  void ShowGameDigestDialog(const std::string& title) override;
  void SetGameDigestProgress(int pid, int progress) override;
  void SetGameDigestResult(int pid, const std::string& result) override;
  void AbortGameDigest() override;

  void ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                 std::span<const int> players) override;
  void HideChunkedProgressDialog() override;
  void SetChunkedProgress(int pid, u64 progress) override;

  void SetHostWiiSyncData(std::vector<u64> titles, std::string redirect_folder) override;

signals:
  void Stop();

private:
  void ConnectWidgets();
  void OnChat();
  void OnStart();
  void DisplayMessage(const QString& msg, const std::string& color,
                      int duration = OSD::Duration::NORMAL);
  void ResetExternalIP();
  void UpdateDiscordPresence();
  void UpdateGUI();
  void GameStatusChanged(bool running);
  void SetOptionsEnabled(bool enabled);

  void SendMessage(const std::string& message);

  QActionGroup* m_savedata_style_group;
  QActionGroup* m_network_mode_group;

  GameDigestDialog* m_game_digest_dialog;
  ChunkedProgressDialog* m_chunked_progress_dialog;
  PadMappingDialog* m_pad_mapping;
  NetPlay::SyncIdentifier m_current_game_identifier;
  std::string m_current_game_name;
  Common::Lazy<std::string> m_external_ip_address;
  std::string m_nickname;
  const GameListModel& m_game_list_model;
  StartGameCallback m_start_game_callback;
  std::unique_ptr<Ui::NetPlayDialog> m_ui;
  bool m_use_traversal = false;
  bool m_is_copy_button_retry = false;
  bool m_got_stop_request = true;
  int m_buffer_size = 0;
  int m_player_count = 0;
  int m_old_player_count = 0;
  bool m_host_input_authority = false;
};
