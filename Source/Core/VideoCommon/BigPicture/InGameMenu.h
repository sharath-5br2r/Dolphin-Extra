// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>

#include "VideoCommon/BigPicture/Navigation.h"

namespace VideoCommon::BigPicture
{
// A frame-local view of immutable host data. Actions are values returned to the host queue.
struct InGameMenu
{
  MenuPage page;
  std::map<std::string, ::BigPicture::NavigationRequest> requests;
  std::optional<::BigPicture::NavigationRequest> Activate(const MenuResult& result) const;
};

InGameMenu BuildInGameMenu(const ::BigPicture::Snapshot& snapshot,
                           const ::BigPicture::NavigationText& text,
                           ::BigPicture::NavigationOptions options);
}  // namespace VideoCommon::BigPicture
