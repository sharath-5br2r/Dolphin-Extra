// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

namespace ConfigWidget
{
// Relays "config changed" to every ConfigBinding without the binder library having to depend on
// DolphinQt's Settings class. dolphin-emu connects Settings::ConfigChanged to Broadcast() once at
// startup, so the coalescing and GUI-thread marshalling in Settings.cpp still governs when this
// fires. qt-tests calls Broadcast() directly.
class ConfigChangeBroadcaster final : public QObject
{
  Q_OBJECT

public:
  static ConfigChangeBroadcaster& Instance();

  void Broadcast();

signals:
  void Changed();

private:
  ConfigChangeBroadcaster() = default;
};
}  // namespace ConfigWidget
