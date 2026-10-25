// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class QColor;
class QPalette;

namespace ToolTipStyle
{
void GetToolTipStyle(QColor& window_color, QColor& text_color, QColor& emphasis_text_color,
                     QColor& border_color, const QPalette& palette,
                     const QPalette& high_contrast_palette);
}  // namespace ToolTipStyle
