// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Updater.h"

#include <cstdlib>
#include <utility>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>

#include "Common/Version.h"

#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/Settings.h"

#include "ui_UpdateAvailableDialog.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

Updater::Updater(QWidget* parent, std::string update_track, std::string hash_override)
    : m_parent(parent), m_update_track(std::move(update_track)),
      m_hash_override(std::move(hash_override))
{
  connect(this, &QThread::finished, this, &QObject::deleteLater);
}

void Updater::run()
{
  AutoUpdateChecker::CheckForUpdate(m_update_track, m_hash_override,
                                    AutoUpdateChecker::CheckType::Automatic);
}

void Updater::CheckForUpdate()
{
  AutoUpdateChecker::CheckForUpdate(m_update_track, m_hash_override,
                                    AutoUpdateChecker::CheckType::Manual);
}

void Updater::OnUpdateAvailable(const NewVersionInformation& info)
{
  if (std::getenv("DOLPHIN_UPDATE_SERVER_URL"))
  {
    TriggerUpdate(info, AutoUpdateChecker::RestartMode::RESTART_AFTER_UPDATE);
    RunOnObject(m_parent, [this] {
      m_parent->close();
      return 0;
    });
    return;
  }

  bool later = false;

  std::optional<int> choice = RunOnObject(m_parent, [&] {
    QDialog* dialog = new QDialog(m_parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    Ui::UpdateAvailableDialog ui;
    ui.setupUi(dialog);
    ui.updateLabel->setText(ui.updateLabel->text()
                                .arg(QString::fromStdString(info.new_shortrev))
                                .arg(QString::fromStdString(Common::GetScmDescStr())));
    ui.changelogBrowser->setHtml(QString::fromStdString(info.changelog_html));

    connect(ui.updateLaterCheckBox, &QCheckBox::toggled, [&](bool checked) { later = checked; });

    auto* never_btn =
        ui.buttonBox->addButton(tr("Never Auto-Update"), QDialogButtonBox::DestructiveRole);
    ui.buttonBox->addButton(tr("Remind Me Later"), QDialogButtonBox::RejectRole);
    ui.buttonBox->addButton(tr("Install Update"), QDialogButtonBox::AcceptRole);

    connect(never_btn, &QPushButton::clicked, [dialog] {
      Settings::Instance().SetAutoUpdateTrack(QString{});
      dialog->reject();
    });

    connect(ui.buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(ui.buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    return dialog->exec();
  });

  if (choice && *choice == QDialog::Accepted)
  {
    TriggerUpdate(info, later ? AutoUpdateChecker::RestartMode::NO_RESTART_AFTER_UPDATE :
                                AutoUpdateChecker::RestartMode::RESTART_AFTER_UPDATE);

    if (!later)
    {
      RunOnObject(m_parent, [this] {
        m_parent->close();
        return 0;
      });
    }
  }
}
