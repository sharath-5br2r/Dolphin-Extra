// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCompleter;

namespace Ui
{
class GameConfigEdit;
}

class GameConfigEdit : public QWidget
{
public:
  explicit GameConfigEdit(QWidget* parent, QString path, bool read_only);
  ~GameConfigEdit() override;

protected:
  void keyPressEvent(QKeyEvent* e) override;
  void focusInEvent(QFocusEvent* e) override;

private:
  void ConnectWidgets();

  void LoadFile();
  void SaveFile();

  void OnSelectionChanged();
  void OnAutoComplete(const QString& completion);
  void OpenExternalEditor();

  QString GetTextUnderCursor();

  void AddDescription(const QString& keyword, const QString& description);

  std::unique_ptr<Ui::GameConfigEdit> m_ui;
  QCompleter* m_completer;
  QStringList m_completions;

  const QString m_path;

  bool m_read_only;

  QMap<QString, QString> m_keyword_map;
};
