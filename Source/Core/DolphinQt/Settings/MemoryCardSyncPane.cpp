// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/MemoryCardSyncPane.h"

#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSysInfo>

#include "Core/Config/MemoryCardSyncSettings.h"
#include "Core/HW/GCMemcard/MemcardSync.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "ui_MemoryCardSyncPane.h"

MemoryCardSyncPane::MemoryCardSyncPane(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::MemoryCardSyncPane>())
{
  m_ui->setupUi(this);
  BindSettings();
  ConnectLayout();
  AddDescriptions();

  // emusync names the device after the computer when no name is set.
  m_ui->deviceNameLineEdit->setPlaceholderText(QSysInfo::machineHostName());
  m_status_timer.setInterval(2000);
  connect(&m_status_timer, &QTimer::timeout, this, &MemoryCardSyncPane::UpdateStatus);

  UpdateEnabledState();
  UpdateSecretPlaceholders();
  UpdateStatus();
}

MemoryCardSyncPane::~MemoryCardSyncPane() = default;

void MemoryCardSyncPane::BindSettings()
{
  ConfigWidget::Bind(m_ui->enabledCheckBox, Config::MAIN_MEMCARD_SYNC_ENABLED);
  ConfigWidget::Bind(m_ui->storageLineEdit, Config::MAIN_MEMCARD_SYNC_REMOTE_URI);
  ConfigWidget::Bind(m_ui->accessKeyIdLineEdit, Config::MAIN_MEMCARD_SYNC_ACCESS_KEY_ID);
  ConfigWidget::Bind(m_ui->deviceNameLineEdit, Config::MAIN_MEMCARD_SYNC_DEVICE_NAME);
}

void MemoryCardSyncPane::ConnectLayout()
{
  // Connected after the bindings above, so the setting is written before it is applied.
  connect(m_ui->enabledCheckBox, &QCheckBox::toggled, this, [this] { Apply(); });
  for (QLineEdit* edit :
       {m_ui->storageLineEdit, m_ui->accessKeyIdLineEdit, m_ui->deviceNameLineEdit})
  {
    connect(edit, &QLineEdit::editingFinished, this, [this, edit] {
      if (edit->isModified())
      {
        edit->setModified(false);
        Apply();
      }
    });
  }
  connect(m_ui->secretLineEdit, &QLineEdit::editingFinished, this,
          &MemoryCardSyncPane::OnSecretEdited);
  connect(m_ui->secretShowButton, &QPushButton::toggled, this, [this](bool shown) {
    OnShowToggled(m_ui->secretLineEdit, m_ui->secretShowButton, MemcardSync::SECRET_ACCESS_KEY,
                  shown);
  });
  connect(m_ui->passphraseLineEdit, &QLineEdit::editingFinished, this,
          &MemoryCardSyncPane::OnPassphraseEdited);
  connect(m_ui->passphraseShowButton, &QPushButton::toggled, this, [this](bool shown) {
    OnShowToggled(m_ui->passphraseLineEdit, m_ui->passphraseShowButton,
                  MemcardSync::ENCRYPTION_PASSPHRASE, shown);
  });
  // Clicking Show leaves the focus in its field: taking it would finish the edit, which saves what
  // was typed and empties the field, so there would be nothing left to show. Tab still reaches it.
  m_ui->secretShowButton->setFocusPolicy(Qt::TabFocus);
  m_ui->passphraseShowButton->setFocusPolicy(Qt::TabFocus);
  connect(m_ui->passphraseRemoveButton, &QPushButton::clicked, this,
          &MemoryCardSyncPane::OnPassphraseRemoveClicked);
  connect(m_ui->syncNowButton, &QPushButton::clicked, this, &MemoryCardSyncPane::OnSyncNowClicked);
}

void MemoryCardSyncPane::AddDescriptions()
{
  ConfigWidget::SetDescription(
      m_ui->enabledCheckBox, tr("Sync Memory Cards"),
      tr("Newer saves from your other devices are put on the cards before a game starts; your "
         "saves upload as Dolphin writes them, and when the game "
         "closes.<br><br><dolphin_emphasis>If "
         "unsure, leave this unchecked.</dolphin_emphasis>"));
  ConfigWidget::SetDescription(
      m_ui->storageLineEdit, tr("Storage"),
      tr("Bucket and folder, e.g. s3://my-saves/emusync/v1?region=eu-west-1. Use the same on "
         "every device. S3-compatible services take an endpoint: "
         "s3://bucket/folder?endpoint=https://...&amp;region=auto."));
  ConfigWidget::SetDescription(
      m_ui->accessKeyIdLineEdit, tr("Access Key ID"),
      tr("From your storage provider. A key that can only read and write this bucket is enough."));
  ConfigWidget::SetDescription(
      m_ui->secretLineEdit, tr("Secret Access Key"),
      tr("Kept in your system's credential store (Credential Manager, Keychain or Secret "
         "Service), never in Dolphin's settings file. Show displays the saved key; type or paste "
         "a new one to replace it."));
  ConfigWidget::SetDescription(
      m_ui->passphraseLineEdit, tr("Encryption Passphrase"),
      tr("Optional. Encrypts your saves on this computer before they upload, so the storage "
         "provider can't read them. New storage is created encrypted; for storage that is already "
         "encrypted, enter the passphrase used when it was set up. Use the same passphrase on "
         "every device. It can't be recovered, and encrypted storage can't be made unencrypted. "
         "Kept in your system's credential store."));
  ConfigWidget::SetDescription(
      m_ui->deviceNameLineEdit, tr("Device Name"),
      tr("How this computer appears in a save's history on your other devices. Empty: the "
         "computer's name."));
}

void MemoryCardSyncPane::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  UpdateStatus();
  m_status_timer.start();
}

void MemoryCardSyncPane::hideEvent(QHideEvent* event)
{
  QWidget::hideEvent(event);
  m_status_timer.stop();
}

// Whether the field holds the stored secret that Show put there, unchanged: nothing to save.
static bool ShowsStoredSecret(const QLineEdit* edit)
{
  const QVariant revealed = edit->property("revealed");
  return revealed.isValid() && revealed.toString() == edit->text();
}

// Empties a secret's field and masks it again, once its value is handed over or removed.
static void ResetSecretField(QLineEdit* edit, QPushButton* show_button)
{
  edit->clear();
  edit->setProperty("revealed", QVariant());
  show_button->setChecked(false);
}

void MemoryCardSyncPane::OnSecretEdited()
{
  const QString text = m_ui->secretLineEdit->text().trimmed();
  if (text.isEmpty() || ShowsStoredSecret(m_ui->secretLineEdit))
    return;
  ResetSecretField(m_ui->secretLineEdit, m_ui->secretShowButton);
  Apply({{MemcardSync::SECRET_ACCESS_KEY, text.toStdString()}});
}

void MemoryCardSyncPane::OnPassphraseEdited()
{
  // Not trimmed: emusync keeps spaces too, and the same passphrase has to unlock the storage on
  // every device.
  const QString text = m_ui->passphraseLineEdit->text();
  if (text.isEmpty() || ShowsStoredSecret(m_ui->passphraseLineEdit))
    return;
  ResetSecretField(m_ui->passphraseLineEdit, m_ui->passphraseShowButton);
  Apply({{MemcardSync::ENCRYPTION_PASSPHRASE, text.toStdString()}});
}

void MemoryCardSyncPane::OnShowToggled(QLineEdit* edit, QPushButton* show_button,
                                       const char* secret, bool shown)
{
  edit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
  if (!shown)
  {
    // Hide puts a stored secret that was shown away again, rather than only masking it.
    if (ShowsStoredSecret(edit))
      edit->clear();
    edit->setProperty("revealed", QVariant());
    return;
  }
  // Show unmasks what is being typed; with nothing typed, it loads the stored secret, e.g. to set
  // up another device with it (and losing the passphrase loses the saves). While settings apply,
  // e.g. a value just saved by tabbing to Show, the store may not have it yet: Apply() shows it
  // once they are applied.
  if (!edit->text().isEmpty() || m_pending_applies > 0 || !MemcardSync::HasSecret(secret))
    return;

  // Reading the credential store can block (an unlock prompt), so not on the UI thread.
  std::thread([pane = QPointer<MemoryCardSyncPane>(this), edit, show_button, secret] {
    std::string error;
    std::optional<std::string> value = MemcardSync::LoadSecret(secret, &error);
    QueueOnObject(qApp, [pane, edit, show_button, secret, value = std::move(value),
                         error = std::move(error)]() mutable {
      if (pane && show_button->isChecked() && edit->text().isEmpty())
      {
        if (value.has_value())
        {
          const QString text = QString::fromStdString(*value);
          edit->setText(text);
          edit->setProperty("revealed", text);
        }
        else if (!error.empty())
        {
          pane->m_ui->statusLabel->setText(QString::fromStdString(error));
        }
        else
        {
          pane->m_ui->statusLabel->setText(std::string_view(secret) ==
                                                   MemcardSync::SECRET_ACCESS_KEY ?
                                               tr("No secret access key is saved.") :
                                               tr("No passphrase is saved."));
        }
      }
      if (value.has_value())
        MemcardSync::WipeSecret(*value);
    });
  }).detach();
}

void MemoryCardSyncPane::OnPassphraseRemoveClicked()
{
  ResetSecretField(m_ui->passphraseLineEdit, m_ui->passphraseShowButton);
  Apply({{MemcardSync::ENCRYPTION_PASSPHRASE, std::string()}});
}

void MemoryCardSyncPane::OnSyncNowClicked()
{
  MemcardSync::SyncNow();
  m_ui->statusLabel->setText(tr("Syncing..."));
}

void MemoryCardSyncPane::Apply(std::vector<std::pair<const char*, std::string>> secrets)
{
  std::vector<MemcardSync::SecretUpdate> updates;
  for (auto& [name, value] : secrets)
    updates.push_back({name, std::move(value)});
  ++m_pending_applies;
  UpdateStatus();
  MemcardSync::ApplySettingsAsync(std::move(updates),
                                  [pane = QPointer<MemoryCardSyncPane>(this)](const std::string&) {
                                    QueueOnObject(qApp, [pane] {
                                      if (pane)
                                        pane->OnApplied();
                                    });
                                  });
}

void MemoryCardSyncPane::OnApplied()
{
  --m_pending_applies;
  UpdateSecretPlaceholders();
  UpdateStatus();
  // Only once every queued change is in: an earlier one finishing doesn't mean a secret saved
  // since is stored yet.
  if (m_pending_applies > 0)
    return;
  // A Show pressed while they applied.
  if (m_ui->secretShowButton->isChecked())
  {
    OnShowToggled(m_ui->secretLineEdit, m_ui->secretShowButton, MemcardSync::SECRET_ACCESS_KEY,
                  true);
  }
  if (m_ui->passphraseShowButton->isChecked())
  {
    OnShowToggled(m_ui->passphraseLineEdit, m_ui->passphraseShowButton,
                  MemcardSync::ENCRYPTION_PASSPHRASE, true);
  }
}

void MemoryCardSyncPane::UpdateEnabledState()
{
  if (MemcardSync::IsAvailable())
    return;
  for (QWidget* w : std::initializer_list<QWidget*>{
           m_ui->enabledCheckBox, m_ui->storageLineEdit, m_ui->accessKeyIdLineEdit,
           m_ui->secretLineEdit, m_ui->secretShowButton, m_ui->passphraseLineEdit,
           m_ui->passphraseShowButton, m_ui->passphraseRemoveButton, m_ui->deviceNameLineEdit,
           m_ui->syncNowButton})
  {
    w->setEnabled(false);
  }
}

void MemoryCardSyncPane::UpdateSecretPlaceholders()
{
  m_ui->secretLineEdit->setPlaceholderText(
      MemcardSync::HasSecret(MemcardSync::SECRET_ACCESS_KEY) ? tr("Saved") : tr("Not set"));
  const bool has_passphrase = MemcardSync::HasSecret(MemcardSync::ENCRYPTION_PASSPHRASE);
  m_ui->passphraseLineEdit->setPlaceholderText(has_passphrase ? tr("Saved") :
                                                                tr("No passphrase saved"));
  m_ui->passphraseRemoveButton->setEnabled(has_passphrase && MemcardSync::IsAvailable());
}

void MemoryCardSyncPane::UpdateStatus()
{
  QString text;
  if (!MemcardSync::IsAvailable())
    text = tr("This build of Dolphin has no memory card sync.");
  else if (m_pending_applies > 0)
    text = tr("Connecting to the storage...");
  else if (const std::string error = MemcardSync::GetLastStartError(); !error.empty())
    text = QString::fromStdString(error);
  else if (!m_ui->enabledCheckBox->isChecked())
    text = tr("Off.");
  else if (!MemcardSync::IsRunning())
    text = tr("Not syncing: fill in the storage, access key ID and secret access key.");
  else if (const std::string status = MemcardSync::GetStatus(); !status.empty())
    text = QString::fromStdString(status);
  else
    text = tr("Syncing. Nothing has changed yet.");
  m_ui->statusLabel->setText(text);
  m_ui->syncNowButton->setEnabled(MemcardSync::IsAvailable() && MemcardSync::IsRunning() &&
                                  m_pending_applies == 0);
}
