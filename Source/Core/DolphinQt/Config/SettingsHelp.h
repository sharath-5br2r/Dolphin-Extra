// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QVariant>
#include <QWidget>

namespace SettingsHelp
{
inline constexpr char TITLE_PROPERTY[] = "_dolphin_settings_help_title";
inline constexpr char DESCRIPTION_PROPERTY[] = "_dolphin_settings_help_description";

inline void SetTitle(QWidget* widget, const QString& title)
{
  widget->setProperty(TITLE_PROPERTY, QVariant{title});
}

inline void SetDescription(QWidget* widget, const QString& description)
{
  widget->setProperty(DESCRIPTION_PROPERTY, QVariant{description});
}

inline void Set(QWidget* widget, const QString& title, const QString& description)
{
  SetTitle(widget, title);
  SetDescription(widget, description);
}

inline QString Title(const QObject* object)
{
  return object->property(TITLE_PROPERTY).toString();
}

inline QString Description(const QObject* object)
{
  return object->property(DESCRIPTION_PROPERTY).toString();
}
}  // namespace SettingsHelp
