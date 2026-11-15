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
  Power,
  Trophy,
  FolderSolid,
  Globe,
  Image,
  CPU,
  Keyboard,
  Warning,
  Network,
  Achievements,
  Speed,
  Memory,
  Mouse,
  Clock,
  Window,
  Cube,
  Sliders,
  Wrench,
  Remote,
  Puzzle,
  StateSave,
  StateLoad,
  Locked,
  Unlocked,
};

// The same Font Awesome / PromptFont families as PCSX2, with a primitive fallback.
ImWchar GetMenuIconCodepoint(MenuIcon icon);
void DrawMenuIcon(ImDrawList* draw, MenuIcon icon, ImVec2 position, float size, ImU32 color);
MenuIcon GetActionIcon(::BigPicture::Action action);
MenuIcon GetLibraryIcon(::BigPicture::LibraryOperation operation);
}  // namespace VideoCommon::BigPicture
