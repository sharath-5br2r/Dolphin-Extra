// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <string>

#include <QKeyEvent>

#include "Common/BigPicture/PresentationState.h"

namespace QtUtils
{
inline void QueueImGuiKeyEvent(BigPicture::PresentationState& input, const QKeyEvent& event,
                               std::uint64_t generation)
{
  const bool is_down = event.type() == QEvent::KeyPress;
  const auto text = is_down ? event.text().toUtf8().toStdString() : std::string{};
  if (event.isAutoRepeat())
  {
    // Qt can send both release and press events for auto-repeat. Preserve repeated text
    // without synthesizing key transitions that could retrigger navigation or menu actions.
    if (!text.empty())
      input.Enqueue({BigPicture::InputEvent::Type::Text, generation, 0, false, text});
    return;
  }

  // Preserve full Qt codes: masking their low bits aliases modifiers with ordinary keys.
  input.Enqueue({BigPicture::InputEvent::Type::Key, generation,
                 static_cast<std::uint32_t>(event.key()), is_down, text});
}
}  // namespace QtUtils
