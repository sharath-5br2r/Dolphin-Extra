// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include <QDialog>

class QLabel;
class QWidget;

namespace Ui
{
class SetupWizardDialog;
}

class SetupWizardDialog final : public QDialog
{
  Q_OBJECT

public:
  explicit SetupWizardDialog(QWidget* parent = nullptr);
  ~SetupWizardDialog() override;

  bool ShouldOpenControllerSettings() const;

private:
  enum class Page
  {
    Appearance,
    GameFolders,
    Controllers,
    Privacy,
    Complete,
    Count,
  };

  void PopulateAppearance();
  void PopulateGameFolders();
  void ConnectWidgets();
  void AddGameFolder();
  void RemoveGameFolder();
  void ShowPreviousPage();
  void ShowNextPage();
  void UpdatePage();
  void FinishSetup();

  std::unique_ptr<Ui::SetupWizardDialog> m_ui;
  std::array<QLabel*, static_cast<std::size_t>(Page::Count)> m_page_labels;
};
