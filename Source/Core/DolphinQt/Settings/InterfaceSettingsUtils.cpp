// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/InterfaceSettingsUtils.h"

#include <QObject>

namespace InterfaceSettings
{
std::vector<StringChoice> GetLanguageChoices()
{
  return {
      {QObject::tr("<System Language>"), QString{}},
      {QStringLiteral(u"Bahasa Melayu"), QStringLiteral("ms")},               // Malay
      {QStringLiteral(u"Catal\u00E0"), QStringLiteral("ca")},                 // Catalan
      {QStringLiteral(u"\u010Ce\u0161tina"), QStringLiteral("cs")},           // Czech
      {QStringLiteral(u"Dansk"), QStringLiteral("da")},                       // Danish
      {QStringLiteral(u"Deutsch"), QStringLiteral("de")},                     // German
      {QStringLiteral(u"English"), QStringLiteral("en")},                     // English
      {QStringLiteral(u"Espa\u00F1ol"), QStringLiteral("es")},                // Spanish
      {QStringLiteral(u"Fran\u00E7ais"), QStringLiteral("fr")},               // French
      {QStringLiteral(u"Hrvatski"), QStringLiteral("hr")},                    // Croatian
      {QStringLiteral(u"Italiano"), QStringLiteral("it")},                    // Italian
      {QStringLiteral(u"Magyar"), QStringLiteral("hu")},                      // Hungarian
      {QStringLiteral(u"Nederlands"), QStringLiteral("nl")},                  // Dutch
      {QStringLiteral(u"Norsk bokm\u00E5l"), QStringLiteral("nb")},           // Norwegian
      {QStringLiteral(u"Polski"), QStringLiteral("pl")},                      // Polish
      {QStringLiteral(u"Portugu\u00EAs"), QStringLiteral("pt")},              // Portuguese
      {QStringLiteral(u"Portugu\u00EAs (Brasil)"), QStringLiteral("pt_BR")},  // Portuguese (Brazil)
      {QStringLiteral(u"Rom\u00E2n\u0103"), QStringLiteral("ro")},            // Romanian
      {QStringLiteral(u"Srpski"), QStringLiteral("sr")},                      // Serbian
      {QStringLiteral(u"Suomi"), QStringLiteral("fi")},                       // Finnish
      {QStringLiteral(u"Svenska"), QStringLiteral("sv")},                     // Swedish
      {QStringLiteral(u"T\u00FCrk\u00E7e"), QStringLiteral("tr")},            // Turkish
      {QStringLiteral(u"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC"),
       QStringLiteral("el")},  // Greek
      {QStringLiteral(u"\u0420\u0443\u0441\u0441\u043A\u0438\u0439"),
       QStringLiteral("ru")},  // Russian
      {QStringLiteral(u"\u0627\u0644\u0639\u0631\u0628\u064A\u0629"),
       QStringLiteral("ar")},                                                     // Arabic
      {QStringLiteral(u"\u0641\u0627\u0631\u0633\u06CC"), QStringLiteral("fa")},  // Farsi
      {QStringLiteral(u"\uD55C\uAD6D\uC5B4"), QStringLiteral("ko")},              // Korean
      {QStringLiteral(u"\u65E5\u672C\u8A9E"), QStringLiteral("ja")},              // Japanese
      {QStringLiteral(u"\u7B80\u4F53\u4E2D\u6587"), QStringLiteral("zh_CN")},  // Simplified Chinese
      {QStringLiteral(u"\u7E41\u9AD4\u4E2D\u6587"),
       QStringLiteral("zh_TW")},  // Traditional Chinese
  };
}
}  // namespace InterfaceSettings
