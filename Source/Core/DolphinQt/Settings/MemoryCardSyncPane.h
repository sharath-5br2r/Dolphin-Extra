// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QTimer>
#include <QWidget>

class QLineEdit;
class QPushButton;

namespace Ui
{
class MemoryCardSyncPane;
}

// Settings for memory card sync with emusync (Core/HW/GCMemcard/MemcardSync.h): the storage, its
// keys (kept in the system's credential store), the optional encryption passphrase, and a status
// line. Each change applies at once, starting, restarting or stopping sync in the background.
class MemoryCardSyncPane final : public QWidget
{
  Q_OBJECT
public:
  explicit MemoryCardSyncPane(QWidget* parent = nullptr);
  ~MemoryCardSyncPane() override;

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

private:
  void BindSettings();
  void ConnectLayout();
  void AddDescriptions();

  void OnSecretEdited();
  void OnPassphraseEdited();
  // A Show button for the field of `secret` (MemcardSync::SECRET_ACCESS_KEY or
  // ENCRYPTION_PASSPHRASE).
  void OnShowToggled(QLineEdit* edit, QPushButton* show_button, const char* secret, bool shown);
  void OnPassphraseRemoveClicked();
  void OnSyncNowClicked();
  // Applies the settings in the background, storing `secrets` first.
  void Apply(std::vector<std::pair<const char*, std::string>> secrets = {});
  // An Apply() is done.
  void OnApplied();
  void UpdateEnabledState();
  void UpdateSecretPlaceholders();
  void UpdateStatus();

  std::unique_ptr<Ui::MemoryCardSyncPane> m_ui;
  QTimer m_status_timer;
  // Settings changes still being applied: each Apply() until its callback.
  int m_pending_applies = 0;
};
