// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

namespace Ui
{
class GCMemcardCreateNewDialog;
}

class GCMemcardCreateNewDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GCMemcardCreateNewDialog(QWidget* parent = nullptr);
  ~GCMemcardCreateNewDialog() override;

  std::string GetMemoryCardPath() const;

private:
  bool CreateCard();

  std::unique_ptr<Ui::GCMemcardCreateNewDialog> m_ui;
  std::string m_card_path;
};
