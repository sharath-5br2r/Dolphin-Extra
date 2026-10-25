// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

// QSignalSpy lives in Qt6Test, which is absent from the bundled Windows Qt
// (Externals/Qt/Qt6.8.3/x64/lib/cmake has no Qt6Test). This is the small part of it we need.
// Counts emissions only: it records no signal arguments and no ordering.
class SignalRecorder
{
public:
  template <typename Sender, typename Signal>
  SignalRecorder(Sender* sender, Signal signal)
  {
    QObject::connect(sender, signal, &m_context, [this] { ++m_count; });
  }

  int Count() const { return m_count; }
  void Reset() { m_count = 0; }

private:
  QObject m_context;
  int m_count = 0;
};
