// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/CloudSavesPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QString>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCMemcard/RcloneUtils.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"

// How long to wait for a network-touching rclone command (checking or moving a remote folder)
// before giving up, so an offline or unreachable remote doesn't leave the UI stuck with no
// feedback.
constexpr int RCLONE_NETWORK_TIMEOUT_MS = 15000;

CloudSavesPane::CloudSavesPane()
{
  auto* const main_layout = new QVBoxLayout{this};

  // Enable/disable toggle
  m_enable_checkbox = new QCheckBox(tr("Enable cloud save sync"));
  ConfigWidget::Bind(m_enable_checkbox, Config::MAIN_CLOUDSYNC_ENABLED);
  main_layout->addWidget(m_enable_checkbox);

  // Configuration File Group
  auto* const conf_group = new QGroupBox{tr("rclone Configuration File")};
  main_layout->addWidget(conf_group);
  auto* const conf_layout = new QVBoxLayout{conf_group};

  auto* const conf_desc = new QLabel{
      tr("Path to <b>rclone.conf</b>. Leave blank to use Dolphin's internal storage or standard location.")};
  conf_desc->setWordWrap(true);
  conf_layout->addWidget(conf_desc);

  auto* const conf_h_layout = new QHBoxLayout;
  m_config_file_edit = new QLineEdit;
  ConfigWidget::Bind(m_config_file_edit, Config::MAIN_CLOUDSYNC_CONFIG_FILE);
  conf_h_layout->addWidget(m_config_file_edit);

  m_browse_config_button = new QPushButton(tr("Browse..."));
  conf_h_layout->addWidget(m_browse_config_button);
  connect(m_browse_config_button, &QPushButton::clicked, this, &CloudSavesPane::BrowseConfigFile);
  conf_layout->addLayout(conf_h_layout);

  connect(m_config_file_edit, &QLineEdit::editingFinished, this, [this] {
    RefreshRemotesList();
    RunCheck();
  });

  // Remote config group
  auto* const remote_group = new QGroupBox{tr("rclone Remote")};
  main_layout->addWidget(remote_group);

  auto* const remote_layout = new QVBoxLayout{remote_group};

  auto* const folder_label = new QLabel{tr("<b>Remote Folder Name</b>")};
  remote_layout->addWidget(folder_label);

  auto* const folder_description =
      new QLabel{tr("Name of the folder created in your cloud storage where saves are kept.")};
  folder_description->setWordWrap(true);
  remote_layout->addWidget(folder_description);

  m_remote_folder_edit = new QLineEdit;
  ConfigWidget::Bind(m_remote_folder_edit, Config::MAIN_CLOUDSYNC_REMOTE_FOLDER);
  remote_layout->addWidget(m_remote_folder_edit);

  // ConfigWidget::Bind already updates Config, so the old value has to be tracked separately here
  // rather than read back from Config.
  m_previous_remote_folder = m_remote_folder_edit->text();
  connect(m_remote_folder_edit, &QLineEdit::editingFinished, this, [this] {
    const QString new_folder = m_remote_folder_edit->text();
    if (new_folder != m_previous_remote_folder && !new_folder.isEmpty())
    {
      const auto choice = QMessageBox::question(
          this, tr("Move Cloud Saves?"),
          tr("Move existing saves from the cloud folder \"%1\" to \"%2\"?<br><br>"
             "If you skip this, saves already synced under the old folder name won't be found "
             "until you rename it back.")
              .arg(m_previous_remote_folder, new_folder),
          QMessageBox::Yes | QMessageBox::No);
      if (choice == QMessageBox::Yes)
        MigrateRemoteFolder(m_previous_remote_folder, new_folder);
    }
    m_previous_remote_folder = new_folder;
  });

  auto* const provider_label = new QLabel{tr("<b>Cloud Remote Name</b>")};
  remote_layout->addWidget(provider_label);

  auto* const remote_description = new QLabel{
      tr("Select an existing remote or configure a new remote directly:")};
  remote_description->setWordWrap(true);
  remote_layout->addWidget(remote_description);

  auto* const remote_h_layout = new QHBoxLayout;
  m_remote_combo = new QComboBox;
  m_remote_combo->setEditable(true);
  m_remote_combo->setFixedWidth(260);
  remote_h_layout->addWidget(m_remote_combo);

  m_new_remote_button = new QPushButton(tr("New Remote Wizard..."));
  remote_h_layout->addWidget(m_new_remote_button);
  remote_h_layout->addStretch(1);
  connect(m_new_remote_button, &QPushButton::clicked, this, &CloudSavesPane::OpenRemoteWizard);

  remote_layout->addLayout(remote_h_layout);

  m_remote_edit = m_remote_combo->lineEdit();
  ConfigWidget::Bind(m_remote_edit, Config::MAIN_CLOUDSYNC_REMOTE);

  connect(m_remote_combo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
    Config::SetBaseOrCurrent(Config::MAIN_CLOUDSYNC_REMOTE, text.toStdString());
  });

  // Status group
  auto* const status_group = new QGroupBox{tr("Status")};
  main_layout->addWidget(status_group);

  auto* const status_layout = new QVBoxLayout{status_group};

  m_status_label = new QLabel{tr("Checking rclone setup...")};
  m_status_label->setWordWrap(true);
  m_status_label->setTextFormat(Qt::RichText);
  m_status_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
  status_layout->addWidget(m_status_label);

  auto* const check_h_layout = new QHBoxLayout;
  m_check_button = new QPushButton(tr("Test Connection"));
  connect(m_check_button, &QPushButton::clicked, this, &CloudSavesPane::RunCheck);
  check_h_layout->addWidget(m_check_button);
  check_h_layout->addStretch(1);
  status_layout->addLayout(check_h_layout);

  main_layout->addStretch(1);

  RefreshRemotesList();
  RunCheck();
}

void CloudSavesPane::BrowseConfigFile()
{
  const QString current_file = QString::fromStdString(Memcard::GetRcloneConfigPath());
  const QString file = DolphinFileDialog::getOpenFileName(
      this, tr("Select rclone.conf"), current_file, tr("rclone config (*.conf);;All Files (*)"));
  if (!file.isEmpty())
  {
    m_config_file_edit->setText(file);
    Config::SetBaseOrCurrent(Config::MAIN_CLOUDSYNC_CONFIG_FILE, file.toStdString());
    RefreshRemotesList();
    RunCheck();
  }
}

void CloudSavesPane::RefreshRemotesList()
{
  const std::string conf_path = Memcard::GetRcloneConfigPath();
  const QString current_remote = QString::fromStdString(Config::Get(Config::MAIN_CLOUDSYNC_REMOTE));

  m_remote_combo->blockSignals(true);
  m_remote_combo->clear();

  if (File::Exists(conf_path))
  {
    Common::IniFile ini;
    if (ini.Load(conf_path))
    {
      for (const auto& section : ini.GetSections())
      {
        const QString name = QString::fromStdString(section.GetName()).trimmed();
        if (!name.isEmpty() && m_remote_combo->findText(name) == -1)
          m_remote_combo->addItem(name);
      }
    }
  }

  if (m_remote_combo->findText(current_remote) == -1 && !current_remote.isEmpty())
    m_remote_combo->addItem(current_remote);

  m_remote_combo->setCurrentText(current_remote.isEmpty() ? QStringLiteral("Dropbox") : current_remote);
  m_remote_combo->blockSignals(false);
}

std::vector<CloudSavesPane::ProviderInfo> CloudSavesPane::FetchProviders()
{
  std::vector<ProviderInfo> list;
  const auto json_str = Memcard::RcloneRPC("config/providers", "{}");
  if (!json_str || json_str->empty())
    return list;

  picojson::value val;
  std::string err = picojson::parse(val, *json_str);
  if (!err.empty() || !val.is<picojson::object>())
    return list;

  const auto& root_obj = val.get<picojson::object>();
  auto it = root_obj.find("providers");
  if (it == root_obj.end() || !it->second.is<picojson::array>())
    return list;

  for (const auto& prov_val : it->second.get<picojson::array>())
  {
    if (!prov_val.is<picojson::object>())
      continue;
    const auto& p_obj = prov_val.get<picojson::object>();
    ProviderInfo pinfo;
    if (p_obj.count("Name") && p_obj.at("Name").is<std::string>())
      pinfo.name = QString::fromStdString(p_obj.at("Name").get<std::string>());
    if (p_obj.count("Description") && p_obj.at("Description").is<std::string>())
      pinfo.description = QString::fromStdString(p_obj.at("Description").get<std::string>());
    if (p_obj.count("Prefix") && p_obj.at("Prefix").is<std::string>() && pinfo.name.isEmpty())
      pinfo.name = QString::fromStdString(p_obj.at("Prefix").get<std::string>());

    if (p_obj.count("Options") && p_obj.at("Options").is<picojson::array>())
    {
      for (const auto& opt_val : p_obj.at("Options").get<picojson::array>())
      {
        if (!opt_val.is<picojson::object>())
          continue;
        const auto& o_obj = opt_val.get<picojson::object>();
        ProviderOption opt;
        if (o_obj.count("Name") && o_obj.at("Name").is<std::string>())
          opt.name = QString::fromStdString(o_obj.at("Name").get<std::string>());
        if (o_obj.count("Help") && o_obj.at("Help").is<std::string>())
          opt.help = QString::fromStdString(o_obj.at("Help").get<std::string>());
        if (o_obj.count("Type") && o_obj.at("Type").is<std::string>())
          opt.type = QString::fromStdString(o_obj.at("Type").get<std::string>());
        if (o_obj.count("DefaultStr") && o_obj.at("DefaultStr").is<std::string>())
          opt.default_val = QString::fromStdString(o_obj.at("DefaultStr").get<std::string>());
        if (o_obj.count("IsPassword") && o_obj.at("IsPassword").is<bool>())
          opt.is_password = o_obj.at("IsPassword").get<bool>();
        if (o_obj.count("Required") && o_obj.at("Required").is<bool>())
          opt.required = o_obj.at("Required").get<bool>();
        if (o_obj.count("Advanced") && o_obj.at("Advanced").is<bool>())
          opt.advanced = o_obj.at("Advanced").get<bool>();

        if (o_obj.count("Examples") && o_obj.at("Examples").is<picojson::array>())
        {
          for (const auto& ex_val : o_obj.at("Examples").get<picojson::array>())
          {
            if (!ex_val.is<picojson::object>())
              continue;
            const auto& e_obj = ex_val.get<picojson::object>();
            ProviderOption::Example ex;
            if (e_obj.count("Value") && e_obj.at("Value").is<std::string>())
              ex.value = QString::fromStdString(e_obj.at("Value").get<std::string>());
            if (e_obj.count("Help") && e_obj.at("Help").is<std::string>())
              ex.help = QString::fromStdString(e_obj.at("Help").get<std::string>());
            if (!ex.value.isEmpty())
              opt.examples.push_back(ex);
          }
        }
        pinfo.options.push_back(opt);
      }
    }
    list.push_back(pinfo);
  }
  return list;
}

void CloudSavesPane::OpenRemoteWizard()
{
  auto providers = FetchProviders();

  // If librclone couldn't be queried, fallback to standard providers
  if (providers.empty())
  {
    auto make_fallback = [](const QString& name, const QString& desc) {
      ProviderInfo p;
      p.name = name;
      p.description = desc;
      return p;
    };
    providers.push_back(make_fallback(QStringLiteral("drive"), tr("Google Drive")));
    providers.push_back(make_fallback(QStringLiteral("dropbox"), tr("Dropbox")));
    providers.push_back(make_fallback(QStringLiteral("onedrive"), tr("Microsoft OneDrive")));
    providers.push_back(make_fallback(QStringLiteral("webdav"), tr("WebDAV / Nextcloud / ownCloud")));
    providers.push_back(make_fallback(QStringLiteral("s3"), tr("Amazon S3 / S3-compatible")));
    providers.push_back(make_fallback(QStringLiteral("box"), tr("Box")));
    providers.push_back(make_fallback(QStringLiteral("sftp"), tr("SSH / SFTP")));
    providers.push_back(make_fallback(QStringLiteral("ftp"), tr("FTP")));
  }

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Cloud Remote Configuration Wizard"));
  dialog.resize(550, 480);

  auto* const root_layout = new QVBoxLayout(&dialog);
  auto* const stack = new QStackedWidget(&dialog);
  root_layout->addWidget(stack, 1);

  // Bottom buttons
  auto* const btn_layout = new QHBoxLayout;
  auto* const back_btn = new QPushButton(tr("< Back"), &dialog);
  auto* const next_btn = new QPushButton(tr("Next >"), &dialog);
  auto* const cancel_btn = new QPushButton(tr("Cancel"), &dialog);
  back_btn->setEnabled(false);
  btn_layout->addWidget(back_btn);
  btn_layout->addWidget(next_btn);
  btn_layout->addStretch(1);
  btn_layout->addWidget(cancel_btn);
  root_layout->addLayout(btn_layout);

  connect(cancel_btn, &QPushButton::clicked, &dialog, &QDialog::reject);

  // ================= Step 1: Remote Name & Provider =================
  auto* const page1 = new QWidget(&dialog);
  auto* const page1_layout = new QVBoxLayout(page1);

  auto* const title1 = new QLabel(tr("<h3>Step 1: Choose Remote Name and Storage Type</h3>"), page1);
  page1_layout->addWidget(title1);

  auto* const form1 = new QFormLayout;
  auto* const name_edit = new QLineEdit(page1);
  name_edit->setPlaceholderText(tr("e.g. MyDrive, Dropbox, Wasabi"));
  form1->addRow(tr("Remote Name:"), name_edit);

  auto* const prov_combo = new QComboBox(page1);
  for (int i = 0; i < static_cast<int>(providers.size()); ++i)
  {
    prov_combo->addItem(QStringLiteral("%1) %2 (%3)")
                            .arg(i + 1)
                            .arg(providers[i].description.isEmpty() ? providers[i].name : providers[i].description)
                            .arg(providers[i].name),
                        providers[i].name);
  }
  form1->addRow(tr("Storage Provider:"), prov_combo);
  page1_layout->addLayout(form1);

  auto* const desc_label = new QLabel(page1);
  desc_label->setWordWrap(true);
  desc_label->setText(tr("<small>Just like the rclone CLI, options for the selected provider will be loaded dynamically.</small>"));
  page1_layout->addWidget(desc_label);
  page1_layout->addStretch(1);

  stack->addWidget(page1);

  // ================= Step 2: Dynamic Options Page =================
  auto* const page2 = new QWidget(&dialog);
  auto* const page2_layout = new QVBoxLayout(page2);
  auto* const title2 = new QLabel(tr("<h3>Step 2: Provider Configuration Options</h3>"), page2);
  page2_layout->addWidget(title2);

  auto* const scroll_area = new QScrollArea(page2);
  scroll_area->setWidgetResizable(true);
  auto* const scroll_content = new QWidget(scroll_area);
  auto* const dynamic_form = new QFormLayout(scroll_content);
  scroll_area->setWidget(scroll_content);
  page2_layout->addWidget(scroll_area, 1);

  stack->addWidget(page2);

  // ================= Step 3: Confirmation Summary =================
  auto* const page3 = new QWidget(&dialog);
  auto* const page3_layout = new QVBoxLayout(page3);
  auto* const title3 = new QLabel(tr("<h3>Step 3: Review and Confirm</h3>"), page3);
  page3_layout->addWidget(title3);

  auto* const summary_text = new QTextEdit(page3);
  summary_text->setReadOnly(true);
  page3_layout->addWidget(summary_text, 1);

  auto* const confirm_note = new QLabel(tr("Click <b>Finish</b> to write this configuration to your rclone.conf file."), page3);
  page3_layout->addWidget(confirm_note);

  stack->addWidget(page3);

  struct OptionWidget
  {
    QString name;
    QString type;
    bool is_password;
    QLineEdit* line_edit = nullptr;
    QComboBox* combo_edit = nullptr;
    QCheckBox* check_edit = nullptr;
  };
  std::vector<OptionWidget> option_widgets;

  const auto build_options_ui = [&]() {
    QLayoutItem* item;
    while ((item = dynamic_form->takeAt(0)) != nullptr)
    {
      if (item->widget())
        delete item->widget();
      delete item;
    }
    option_widgets.clear();

    const QString selected_prov = prov_combo->currentData().toString();
    const ProviderInfo* cur_info = nullptr;
    for (const auto& p : providers)
    {
      if (p.name == selected_prov)
      {
        cur_info = &p;
        break;
      }
    }

    if (!cur_info || cur_info->options.empty())
    {
      // Fallback fields
      OptionWidget ow_user;
      ow_user.name = QStringLiteral("user");
      ow_user.is_password = false;
      ow_user.line_edit = new QLineEdit(scroll_content);
      dynamic_form->addRow(tr("User / Access Key:"), ow_user.line_edit);
      option_widgets.push_back(ow_user);

      OptionWidget ow_pass;
      ow_pass.name = QStringLiteral("pass");
      ow_pass.is_password = true;
      ow_pass.line_edit = new QLineEdit(scroll_content);
      ow_pass.line_edit->setEchoMode(QLineEdit::Password);
      dynamic_form->addRow(tr("Password / Secret Key / Token:"), ow_pass.line_edit);
      option_widgets.push_back(ow_pass);
      return;
    }

    for (const auto& opt : cur_info->options)
    {
      // Display primary options (skip heavy advanced internals)
      if (opt.advanced && !opt.required && opt.name != QStringLiteral("endpoint") &&
          opt.name != QStringLiteral("url") && opt.name != QStringLiteral("vendor") &&
          opt.name != QStringLiteral("env_auth"))
      {
        continue;
      }

      OptionWidget ow;
      ow.name = opt.name;
      ow.type = opt.type;
      ow.is_password = opt.is_password;

      QString label_str = opt.name;
      if (opt.required)
        label_str += QStringLiteral(" *");

      if (!opt.examples.empty())
      {
        ow.combo_edit = new QComboBox(scroll_content);
        ow.combo_edit->setEditable(true);
        if (!opt.default_val.isEmpty())
          ow.combo_edit->addItem(opt.default_val, opt.default_val);
        for (const auto& ex : opt.examples)
        {
          QString display = ex.help.isEmpty() ? ex.value : QStringLiteral("%1 (%2)").arg(ex.value, ex.help);
          ow.combo_edit->addItem(display, ex.value);
        }
        if (!opt.default_val.isEmpty())
          ow.combo_edit->setCurrentText(opt.default_val);

        if (!opt.help.isEmpty())
          ow.combo_edit->setToolTip(opt.help);
        dynamic_form->addRow(label_str, ow.combo_edit);
      }
      else if (opt.type == QStringLiteral("bool"))
      {
        ow.check_edit = new QCheckBox(opt.help.isEmpty() ? opt.name : opt.help, scroll_content);
        ow.check_edit->setChecked(opt.default_val == QStringLiteral("true"));
        dynamic_form->addRow(label_str, ow.check_edit);
      }
      else
      {
        ow.line_edit = new QLineEdit(scroll_content);
        if (opt.is_password)
          ow.line_edit->setEchoMode(QLineEdit::Password);
        if (!opt.default_val.isEmpty())
          ow.line_edit->setText(opt.default_val);
        if (!opt.help.isEmpty())
          ow.line_edit->setPlaceholderText(opt.help.split(QLatin1Char('\n')).first());
        dynamic_form->addRow(label_str, ow.line_edit);
      }
      option_widgets.push_back(ow);
    }
  };

  const auto update_summary = [&]() {
    const QString name = name_edit->text().trimmed();
    const QString type = prov_combo->currentData().toString();
    QString text = QStringLiteral("[%1]\ntype = %2\n").arg(name, type);

    for (const auto& ow : option_widgets)
    {
      QString val;
      if (ow.combo_edit)
      {
        val = ow.combo_edit->currentData().toString();
        if (val.isEmpty())
          val = ow.combo_edit->currentText().trimmed();
      }
      else if (ow.check_edit)
      {
        val = ow.check_edit->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
      }
      else if (ow.line_edit)
      {
        val = ow.line_edit->text().trimmed();
      }

      if (!val.isEmpty())
      {
        if (ow.is_password)
          text += QStringLiteral("%1 = ********\n").arg(ow.name);
        else
          text += QStringLiteral("%1 = %2\n").arg(ow.name, val);
      }
    }
    summary_text->setText(text);
  };

  connect(next_btn, &QPushButton::clicked, [&]() {
    const int idx = stack->currentIndex();
    if (idx == 0)
    {
      if (name_edit->text().trimmed().isEmpty())
      {
        QMessageBox::warning(&dialog, tr("Validation Error"), tr("Please provide a name for the remote."));
        return;
      }
      build_options_ui();
      stack->setCurrentIndex(1);
      back_btn->setEnabled(true);
    }
    else if (idx == 1)
    {
      update_summary();
      stack->setCurrentIndex(2);
      next_btn->setText(tr("Finish"));
    }
    else if (idx == 2)
    {
      dialog.accept();
    }
  });

  connect(back_btn, &QPushButton::clicked, [&]() {
    const int idx = stack->currentIndex();
    if (idx > 0)
    {
      stack->setCurrentIndex(idx - 1);
      next_btn->setText(tr("Next >"));
      if (idx - 1 == 0)
        back_btn->setEnabled(false);
    }
  });

  if (dialog.exec() == QDialog::Accepted)
  {
    const QString name = name_edit->text().trimmed();
    const QString type = prov_combo->currentData().toString();

    // Construct parameters map for librclone RcloneRPC config/create
    picojson::object params;
    for (const auto& ow : option_widgets)
    {
      QString val;
      if (ow.combo_edit)
      {
        val = ow.combo_edit->currentData().toString();
        if (val.isEmpty())
          val = ow.combo_edit->currentText().trimmed();
      }
      else if (ow.check_edit)
      {
        val = ow.check_edit->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
      }
      else if (ow.line_edit)
      {
        val = ow.line_edit->text().trimmed();
      }

      if (!val.isEmpty())
        params[ow.name.toStdString()] = picojson::value(val.toStdString());
    }

    picojson::object rpc_input;
    rpc_input["name"] = picojson::value(name.toStdString());
    rpc_input["type"] = picojson::value(type.toStdString());
    rpc_input["parameters"] = picojson::value(params);

    const std::string rpc_str = picojson::value(rpc_input).serialize();
    const auto rpc_res = Memcard::RcloneRPC("config/create", rpc_str);

    // Save directly to rclone.conf as well to ensure synchronization
    const std::string conf_path = Memcard::GetRcloneConfigPath();
    Common::IniFile ini;
    if (File::Exists(conf_path))
      ini.Load(conf_path);

    const std::string sec_name = name.toStdString();
    auto* section = ini.GetOrCreateSection(sec_name);
    section->Set("type", type.toStdString());
    for (const auto& [k, v] : params)
    {
      if (v.is<std::string>())
        section->Set(k, v.get<std::string>());
    }
    ini.Save(conf_path);

    Config::SetBaseOrCurrent(Config::MAIN_CLOUDSYNC_REMOTE, sec_name);
    RefreshRemotesList();
    RunCheck();

    QMessageBox::information(
        this, tr("Remote Configured"),
        tr("Remote \"%1\" saved to %2 successfully.")
            .arg(name, QString::fromStdString(conf_path)));
  }
}

void CloudSavesPane::RunCheck()
{
  const QString remote =
      m_remote_edit->text().isEmpty() ? QStringLiteral("Dropbox") : m_remote_edit->text();
  const QString remote_target = remote + QStringLiteral(":");

  m_status_label->setText(tr("Checking connection to <b>%1</b>...").arg(remote_target));
  m_check_button->setEnabled(false);

  // Run in background thread so network timeouts or librclone RPC don't hang the GUI thread
  std::thread([this, remote_target] {
    picojson::object about_in;
    about_in["fs"] = picojson::value(remote_target.toStdString());
    const std::string about_str = picojson::value(about_in).serialize();

    const auto res = Memcard::RcloneRPC("operations/about", about_str);
    const auto ver_res = res ? std::nullopt : Memcard::RcloneRPC("core/version", "{}");

    QMetaObject::invokeMethod(this, [this, res, ver_res, remote_target] {
      m_check_button->setEnabled(true);
      if (res)
      {
        m_status_label->setText(
            tr("<font color=\"green\"><b>Connected successfully!</b></font><br>"
               "Target: %1")
                .arg(remote_target));
      }
      else if (ver_res)
      {
        m_status_label->setText(
            tr("Remote <b>%1</b> could not be accessed or is not configured.<br><br>"
               "Click <b>New Remote Wizard...</b> to configure it.")
                .arg(remote_target));
      }
      else
      {
        m_status_label->setText(tr("<b>rclone</b> is not available in this build."));
      }
    });
  }).detach();
}

void CloudSavesPane::MigrateRemoteFolder(const QString& old_folder, const QString& new_folder)
{
  const QString remote =
      m_remote_edit->text().isEmpty() ? QStringLiteral("Dropbox") : m_remote_edit->text();

  const QString rclone_exe = QStringLiteral("rclone");
  const std::string conf_path = Memcard::GetRcloneConfigPath();

  m_status_label->setText(tr("Moving saves from \"%1\" to \"%2\"...").arg(old_folder, new_folder));

  auto* const move_process = new QProcess(this);
  auto* const move_timeout = new QTimer(this);
  move_timeout->setSingleShot(true);
  connect(move_timeout, &QTimer::timeout, this, [move_process] {
    if (move_process->state() != QProcess::NotRunning)
      move_process->kill();
  });

  connect(move_process, &QProcess::finished, this,
          [this, move_process, move_timeout, old_folder,
           new_folder](int exit_code, QProcess::ExitStatus exit_status) {
            move_timeout->stop();
            move_timeout->deleteLater();

            if (exit_status == QProcess::CrashExit)
            {
              m_status_label->setText(tr("Timed out moving saves from \"%1\" to \"%2\".")
                                          .arg(old_folder, new_folder));
            }
            else if (exit_code == 0)
            {
              m_status_label->setText(
                  tr("Moved saves from \"%1\" to \"%2\".").arg(old_folder, new_folder));
            }
            else
            {
              m_status_label->setText(
                  tr("Could not move saves from \"%1\" to \"%2\" (the old folder may not have "
                     "existed yet). New saves will be written to \"%2\".")
                      .arg(old_folder, new_folder));
            }
            move_process->deleteLater();
          });

  QStringList move_args;
  if (!conf_path.empty() && File::Exists(conf_path))
    move_args << QStringLiteral("--config") << QString::fromStdString(conf_path);

  move_args << QStringLiteral("moveto")
            << (remote + QStringLiteral(":") + old_folder)
            << (remote + QStringLiteral(":") + new_folder);

  move_timeout->start(RCLONE_NETWORK_TIMEOUT_MS);
  move_process->start(rclone_exe, move_args);
}
