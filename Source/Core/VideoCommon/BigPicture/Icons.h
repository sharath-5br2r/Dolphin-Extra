// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <imgui.h>

#include "Common/BigPicture/Contracts.h"

namespace VideoCommon::BigPicture
{
enum class MenuIcon
{
  None,
  Play,
  Stop,
  Save,
  Load,
  Settings,
  Reset,
  Camera,
  Exit,
  Display,
  Grid,
  List,
  Search,
  Folder,
  Back,
  Controller,
  Audio,
  Disc,
  File,
  Filter,
  Theme,
  Check,
};

// Small, independently drawn symbols; no icon-font dependency or external artwork.
void DrawMenuIcon(ImDrawList* draw, MenuIcon icon, ImVec2 position, float size, ImU32 color);
MenuIcon GetActionIcon(::BigPicture::Action action);
MenuIcon GetLibraryIcon(::BigPicture::LibraryOperation operation);
}  // namespace VideoCommon::BigPicture
