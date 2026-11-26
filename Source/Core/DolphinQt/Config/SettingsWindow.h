// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <QDialog>
#include <QPointer>

class QIcon;
class MainWindow;
class QEvent;

namespace Ui
{
class SettingsWindow;
}

// A settings window with a QListWidget to switch between panes of a QStackedWidget.
class StackedSettingsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit StackedSettingsWindow(QWidget* parent = nullptr);
  ~StackedSettingsWindow() override;

  void ActivatePane(int index);

protected:
  void AddPane(QWidget*, const QString& name);
  void AddPane(QWidget*, const QString& name, const QIcon& icon, QString help_text);

  // Adds a scrollable Pane.
  void AddWrappedPane(QWidget*, const QString& name);
  void AddWrappedPane(QWidget*, const QString& name, const QIcon& icon, QString help_text);

  // For derived classes to call after they create their settings panes.
  void OnDoneCreatingPanes();
  void SetPaneIcon(int index, const QIcon& icon);

  void changeEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

  QWidget* GetActivePane();

private:
  QWidget* FindHelpWidget(QObject* object) const;
  void InstallHelpEventFilters(QWidget* root);
  void OnCurrentRowChanged(int index);
  void ShowCategoryHelp();
  void ShowControlHelp(QWidget* widget);
  void UpdateNavigationListStyle();

  std::unique_ptr<Ui::SettingsWindow> m_ui;
  std::vector<QString> m_category_help_text;
  QPointer<QWidget> m_current_help_widget;
  bool m_has_help = false;
  bool m_handling_theme_change = false;
};

enum class SettingsWindowPaneIndex : int
{
  General = 0,
  Graphics,
  Controllers,
  CloudSaves,
  Interface,
  BigPicture,
  OnScreenDisplay,
  Audio,
  Paths,
  GameCube,
  Wii,
  Triforce,
  Advanced,
};

class SettingsWindow final : public StackedSettingsWindow
{
  Q_OBJECT
public:
  explicit SettingsWindow(MainWindow* parent);

  void SelectPane(SettingsWindowPaneIndex);

  void closeEvent(QCloseEvent* event) override;

signals:
  void ConfigureBigPictureInput(int controller);

private:
  void UpdateCategoryIcons();
};
