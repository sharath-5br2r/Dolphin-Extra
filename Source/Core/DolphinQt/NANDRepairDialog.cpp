// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NANDRepairDialog.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>

#include <fmt/format.h>

#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"
#include "DiscIO/WiiSaveBanner.h"
#include "DolphinQt/Resources.h"

#include "ui_NANDRepairDialog.h"

NANDRepairDialog::NANDRepairDialog(const WiiUtils::NANDCheckResult& result, QWidget* parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::NANDRepairDialog>())
{
  m_ui->setupUi(this);
  setWindowIcon(Resources::GetAppIcon());

  if (!result.titles_to_remove.empty())
  {
    std::string title_listings;
    Core::TitleDatabase title_db;
    const DiscIO::Language language = SConfig::GetInstance().GetCurrentLanguage(true);
    for (const u64 title_id : result.titles_to_remove)
    {
      title_listings += fmt::format("{:016x}", title_id);

      const std::string database_name = title_db.GetChannelName(title_id, language);
      if (!database_name.empty())
      {
        title_listings += " - " + database_name;
      }
      else
      {
        DiscIO::WiiSaveBanner banner(title_id);
        if (banner.IsValid())
        {
          title_listings += " - " + banner.GetName();
          const std::string description = banner.GetDescription();
          if (!StripWhitespace(description).empty())
            title_listings += " - " + description;
        }
      }

      title_listings += "\n";
    }

    m_ui->titleBox->setPlainText(QString::fromStdString(title_listings));
  }
  else
  {
    m_ui->warningLabel->hide();
    m_ui->titleBox->hide();
    m_ui->maybeFixLabel->hide();
  }

  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
  m_ui->iconLabel->setPixmap(icon.pixmap(100));

  connect(m_ui->buttonBox->button(QDialogButtonBox::Yes), &QPushButton::clicked, this,
          &QDialog::accept);
  connect(m_ui->buttonBox->button(QDialogButtonBox::No), &QPushButton::clicked, this,
          &QDialog::reject);
}

NANDRepairDialog::~NANDRepairDialog() = default;
