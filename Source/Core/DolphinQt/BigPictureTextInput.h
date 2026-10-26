// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>

#include <QDialog>

class BigPictureTextInput final : public QDialog
{
  Q_OBJECT
public:
  enum
  {
    ControllerKeyboard = 2
  };
  struct InputMethods
  {
    bool physical_keyboard;
    std::function<void()> show_system_keyboard;
    std::function<bool()> system_keyboard_visible;
  };
  BigPictureTextInput(QWidget* parent, const QString& title, const QString& initial,
                      InputMethods methods);
  static InputMethods GetInputMethods();
  QString GetText() const;
};
