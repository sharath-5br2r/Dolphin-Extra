// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ApplicationTheme.h"

#include <QColor>

namespace
{
void SetColors(QPalette& palette, QPalette::ColorRole role, const QColor& color,
               const QColor& disabled_color)
{
  palette.setColor(QPalette::All, role, color);
  palette.setColor(QPalette::Disabled, role, disabled_color);
}

QPalette CreateLightPalette()
{
  QPalette palette;
  SetColors(palette, QPalette::Window, QColor(239, 239, 239), QColor(239, 239, 239));
  SetColors(palette, QPalette::WindowText, QColor(0, 0, 0), QColor(190, 190, 190));
  SetColors(palette, QPalette::Base, QColor(255, 255, 255), QColor(239, 239, 239));
  SetColors(palette, QPalette::AlternateBase, QColor(247, 247, 247), QColor(247, 247, 247));
  SetColors(palette, QPalette::ToolTipBase, QColor(255, 255, 220), QColor(255, 255, 220));
  SetColors(palette, QPalette::ToolTipText, QColor(0, 0, 0), QColor(0, 0, 0));
  SetColors(palette, QPalette::PlaceholderText, QColor(119, 119, 119), QColor(119, 119, 119));
  SetColors(palette, QPalette::Text, QColor(0, 0, 0), QColor(190, 190, 190));
  SetColors(palette, QPalette::Button, QColor(239, 239, 239), QColor(239, 239, 239));
  SetColors(palette, QPalette::ButtonText, QColor(0, 0, 0), QColor(190, 190, 190));
  SetColors(palette, QPalette::BrightText, QColor(255, 255, 255), QColor(255, 255, 255));
  SetColors(palette, QPalette::Light, QColor(255, 255, 255), QColor(255, 255, 255));
  SetColors(palette, QPalette::Midlight, QColor(202, 202, 202), QColor(202, 202, 202));
  SetColors(palette, QPalette::Dark, QColor(159, 159, 159), QColor(190, 190, 190));
  SetColors(palette, QPalette::Mid, QColor(184, 184, 184), QColor(184, 184, 184));
  SetColors(palette, QPalette::Shadow, QColor(118, 118, 118), QColor(177, 177, 177));
  SetColors(palette, QPalette::Highlight, QColor(48, 140, 198), QColor(145, 145, 145));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  SetColors(palette, QPalette::Accent, QColor(48, 140, 198), QColor(145, 145, 145).darker());
#endif
  SetColors(palette, QPalette::HighlightedText, QColor(255, 255, 255), QColor(255, 255, 255));
  SetColors(palette, QPalette::Link, QColor(0, 0, 255), QColor(0, 0, 255));
  SetColors(palette, QPalette::LinkVisited, QColor(255, 0, 255), QColor(255, 0, 255));
  return palette;
}

QPalette CreateDarkGrayPalette()
{
  QPalette palette;
  SetColors(palette, QPalette::Window, QColor(50, 50, 50), QColor(55, 55, 55));
  SetColors(palette, QPalette::WindowText, QColor(200, 200, 200), QColor(108, 108, 108));
  SetColors(palette, QPalette::Base, QColor(25, 25, 25), QColor(30, 30, 30));
  SetColors(palette, QPalette::AlternateBase, QColor(38, 38, 38), QColor(42, 42, 42));
  SetColors(palette, QPalette::ToolTipBase, QColor(45, 45, 45), QColor(45, 45, 45));
  SetColors(palette, QPalette::ToolTipText, QColor(200, 200, 200), QColor(200, 200, 200));
  SetColors(palette, QPalette::PlaceholderText, QColor(90, 90, 90), QColor(90, 90, 90));
  SetColors(palette, QPalette::Text, QColor(200, 200, 200), QColor(108, 108, 108));
  SetColors(palette, QPalette::Button, QColor(54, 54, 54), QColor(54, 54, 54));
  SetColors(palette, QPalette::ButtonText, QColor(200, 200, 200), QColor(108, 108, 108));
  SetColors(palette, QPalette::BrightText, QColor(75, 75, 75), QColor(255, 255, 255));
  SetColors(palette, QPalette::Light, QColor(26, 26, 26), QColor(26, 26, 26));
  SetColors(palette, QPalette::Midlight, QColor(40, 40, 40), QColor(40, 40, 40));
  SetColors(palette, QPalette::Dark, QColor(108, 108, 108), QColor(108, 108, 108));
  SetColors(palette, QPalette::Mid, QColor(71, 71, 71), QColor(71, 71, 71));
  SetColors(palette, QPalette::Shadow, QColor(25, 25, 25), QColor(37, 37, 37));
  SetColors(palette, QPalette::Highlight, QColor(45, 140, 225), QColor(45, 140, 225).darker());
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  SetColors(palette, QPalette::Accent, QColor(45, 140, 225), QColor(45, 140, 225).darker());
#endif
  SetColors(palette, QPalette::HighlightedText, QColor(255, 255, 255), QColor(40, 40, 40));
  SetColors(palette, QPalette::Link, QColor(40, 130, 220), QColor(40, 130, 220).darker());
  SetColors(palette, QPalette::LinkVisited, QColor(110, 70, 150), QColor(110, 70, 150).darker());
  return palette;
}

QPalette CreateDarkPalette()
{
  QPalette palette;
  SetColors(palette, QPalette::Window, QColor(22, 22, 22), QColor(30, 30, 30));
  SetColors(palette, QPalette::WindowText, QColor(180, 180, 180), QColor(90, 90, 90));
  SetColors(palette, QPalette::Base, QColor(35, 35, 35), QColor(30, 30, 30));
  SetColors(palette, QPalette::AlternateBase, QColor(40, 40, 40), QColor(35, 35, 35));
  SetColors(palette, QPalette::ToolTipBase, QColor(0, 0, 0), QColor(0, 0, 0));
  SetColors(palette, QPalette::ToolTipText, QColor(170, 170, 170), QColor(170, 170, 170));
  SetColors(palette, QPalette::PlaceholderText, QColor(100, 100, 100), QColor(100, 100, 100));
  SetColors(palette, QPalette::Text, QColor(200, 200, 200), QColor(90, 90, 90));
  SetColors(palette, QPalette::Button, QColor(30, 30, 30), QColor(20, 20, 20));
  SetColors(palette, QPalette::ButtonText, QColor(180, 180, 180), QColor(90, 90, 90));
  SetColors(palette, QPalette::BrightText, QColor(75, 75, 75), QColor(255, 255, 255));
  SetColors(palette, QPalette::Light, QColor(0, 0, 0), QColor(0, 0, 0));
  SetColors(palette, QPalette::Midlight, QColor(40, 40, 40), QColor(40, 40, 40));
  SetColors(palette, QPalette::Dark, QColor(90, 90, 90), QColor(90, 90, 90));
  SetColors(palette, QPalette::Mid, QColor(60, 60, 60), QColor(60, 60, 60));
  SetColors(palette, QPalette::Shadow, QColor(10, 10, 10), QColor(20, 20, 20));
  SetColors(palette, QPalette::Highlight, QColor(35, 130, 200), QColor(35, 130, 200).darker());
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  SetColors(palette, QPalette::Accent, QColor(35, 130, 200), QColor(35, 130, 200).darker());
#endif
  SetColors(palette, QPalette::HighlightedText, QColor(240, 240, 240), QColor(35, 35, 35));
  SetColors(palette, QPalette::Link, QColor(40, 130, 220), QColor(40, 130, 220).darker());
  SetColors(palette, QPalette::LinkVisited, QColor(110, 70, 150), QColor(110, 70, 150).darker());
  return palette;
}
}  // namespace

namespace ApplicationTheme
{
Definition GetDefinition(Type type, bool use_system_dark_fallback)
{
  type = Canonicalize(type);

  switch (type)
  {
  case Type::Light:
    return {type, true, Qt::ColorScheme::Light, CreateLightPalette()};
  case Type::DarkGray:
    return {type, true, Qt::ColorScheme::Dark, CreateDarkGrayPalette()};
  case Type::Dark:
    return {type, true, Qt::ColorScheme::Dark, CreateDarkPalette()};
  case Type::System:
    if (use_system_dark_fallback)
      return {type, true, Qt::ColorScheme::Unknown, CreateDarkGrayPalette()};
    return {type, false, Qt::ColorScheme::Unknown, std::nullopt};
  case Type::User:
    return {type, false, Qt::ColorScheme::Unknown, std::nullopt};
  default:
    return {Type::System, false, Qt::ColorScheme::Unknown, std::nullopt};
  }
}

bool IsDark(const QPalette& palette)
{
  return palette.color(QPalette::WindowText).value() > palette.color(QPalette::Window).value();
}
}  // namespace ApplicationTheme
