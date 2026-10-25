// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cmath>

namespace ConfigWidget
{
// QSlider positions are integers, so a float setting bound to a slider needs a quantisation that
// round-trips. Position 0 is `minimum`; each position advances by `step`. MaximumPosition() returns
// the highest valid position (the slider's authored `maximum`), not the count of positions.
struct FloatSliderRange
{
  float minimum;
  float maximum;
  float step;

  constexpr int MaximumPosition() const
  {
    return static_cast<int>(std::lround((maximum - minimum) / step));
  }

  int PositionForValue(float value) const
  {
    const float clamped = std::clamp(value, minimum, maximum);
    const int position = static_cast<int>(std::lround((clamped - minimum) / step));
    return std::clamp(position, 0, MaximumPosition());
  }

  float ValueForPosition(int position) const
  {
    const int clamped = std::clamp(position, 0, MaximumPosition());
    return std::clamp(minimum + static_cast<float>(clamped) * step, minimum, maximum);
  }
};
}  // namespace ConfigWidget
