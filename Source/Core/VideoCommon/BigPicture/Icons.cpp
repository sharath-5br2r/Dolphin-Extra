// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Icons.h"

#include <algorithm>
#include <cmath>

namespace VideoCommon::BigPicture
{
void DrawMenuIcon(ImDrawList* draw, MenuIcon icon, ImVec2 position, float size, ImU32 color)
{
  const float scale = size / 24;
  const float stroke = std::max(1.0f, 1.7f * scale);
  const auto p = [&](float x, float y) {
    return ImVec2(position.x + x * scale, position.y + y * scale);
  };
  const auto line = [&](float x, float y, float xx, float yy) {
    draw->AddLine(p(x, y), p(xx, yy), color, stroke);
  };
  const auto rect = [&](float x, float y, float xx, float yy) {
    draw->AddRect(p(x, y), p(xx, yy), color, 2 * scale, 0, stroke);
  };
  const auto circle = [&](float x, float y, float radius) {
    draw->AddCircle(p(x, y), radius * scale, color, 32, stroke);
  };
  switch (icon)
  {
  case MenuIcon::Play:
    draw->AddTriangleFilled(p(5, 3), p(21, 12), p(5, 21), color);
    break;
  case MenuIcon::Stop:
    draw->AddRectFilled(p(4, 4), p(20, 20), color, 2 * scale);
    break;
  case MenuIcon::Save:
    rect(3, 3, 21, 21);
    rect(7, 13, 17, 21);
    rect(7, 3, 16, 9);
    break;
  case MenuIcon::Load:
    line(12, 2, 12, 16);
    line(6, 10, 12, 16);
    line(12, 16, 18, 10);
    line(3, 17, 3, 21);
    line(3, 21, 21, 21);
    line(21, 21, 21, 17);
    break;
  case MenuIcon::Settings:
    circle(12, 12, 6);
    circle(12, 12, 2);
    for (int i = 0; i < 8; ++i)
    {
      const float angle = i * 0.78539816f;
      line(12 + std::cos(angle) * 7, 12 + std::sin(angle) * 7, 12 + std::cos(angle) * 10,
           12 + std::sin(angle) * 10);
    }
    break;
  case MenuIcon::Reset:
    circle(12, 12, 8);
    draw->AddTriangleFilled(p(2, 3), p(9, 4), p(3, 10), color);
    break;
  case MenuIcon::Camera:
    rect(2, 7, 22, 21);
    circle(12, 14, 4);
    line(7, 7, 9, 3);
    line(9, 3, 15, 3);
    line(15, 3, 17, 7);
    break;
  case MenuIcon::Exit:
    line(12, 3, 3, 3);
    line(3, 3, 3, 21);
    line(3, 21, 12, 21);
    line(9, 12, 22, 12);
    line(16, 6, 22, 12);
    line(16, 18, 22, 12);
    break;
  case MenuIcon::Display:
    rect(2, 3, 22, 17);
    line(12, 17, 12, 22);
    line(7, 22, 17, 22);
    break;
  case MenuIcon::Grid:
    for (int y = 0; y < 3; ++y)
      for (int x = 0; x < 3; ++x)
        draw->AddRectFilled(p(3 + x * 7, 3 + y * 7), p(7 + x * 7, 7 + y * 7), color, scale);
    break;
  case MenuIcon::List:
    for (int y = 5; y <= 19; y += 7)
    {
      draw->AddCircleFilled(p(3, y), scale, color);
      line(8, y, 22, y);
    }
    break;
  case MenuIcon::Search:
    circle(9, 9, 6);
    line(14, 14, 22, 22);
    break;
  case MenuIcon::Folder:
    line(2, 6, 9, 6);
    line(9, 6, 12, 9);
    line(12, 9, 22, 9);
    line(22, 9, 20, 21);
    line(20, 21, 2, 21);
    line(2, 21, 2, 6);
    break;
  case MenuIcon::Back:
    line(3, 12, 22, 12);
    line(3, 12, 10, 5);
    line(3, 12, 10, 19);
    break;
  case MenuIcon::Controller:
    draw->AddRect(p(2, 6), p(22, 19), color, 5 * scale, 0, stroke);
    line(5, 12, 11, 12);
    line(8, 9, 8, 15);
    draw->AddCircleFilled(p(16, 11), 1.3f * scale, color);
    draw->AddCircleFilled(p(19, 14), 1.3f * scale, color);
    break;
  case MenuIcon::Audio:
    rect(3, 9, 8, 15);
    line(8, 9, 14, 4);
    line(14, 4, 14, 20);
    line(14, 20, 8, 15);
    line(18, 7, 21, 10);
    line(21, 10, 21, 14);
    line(21, 14, 18, 17);
    break;
  case MenuIcon::Disc:
    circle(12, 12, 9);
    circle(12, 12, 2);
    break;
  case MenuIcon::File:
    rect(5, 2, 19, 22);
    line(8, 8, 16, 8);
    line(8, 12, 16, 12);
    line(8, 16, 16, 16);
    break;
  case MenuIcon::Filter:
    for (int y = 5; y <= 19; y += 7)
    {
      line(2, y, 22, y);
      circle(y == 12 ? 16 : 8, y, 2);
    }
    break;
  case MenuIcon::Theme:
    circle(12, 12, 9);
    draw->AddCircleFilled(p(8, 8), 1.7f * scale, color);
    draw->AddCircleFilled(p(15, 7), 1.7f * scale, color);
    draw->AddCircleFilled(p(17, 14), 1.7f * scale, color);
    break;
  case MenuIcon::Check:
    line(3, 12, 9, 18);
    line(9, 18, 21, 5);
    break;
  default:
    break;
  }
}

MenuIcon GetActionIcon(::BigPicture::Action action)
{
  using A = ::BigPicture::Action;
  switch (action)
  {
  case A::Boot:
  case A::BootIPL:
  case A::BootSystemMenu:
  case A::Resume:
    return MenuIcon::Play;
  case A::Stop:
    return MenuIcon::Stop;
  case A::SaveStates:
  case A::SaveState:
    return MenuIcon::Save;
  case A::LoadStates:
  case A::LoadState:
    return MenuIcon::Load;
  case A::Settings:
  case A::DesktopSettings:
    return MenuIcon::Settings;
  case A::Reset:
  case A::RefreshStates:
    return MenuIcon::Reset;
  case A::Screenshot:
    return MenuIcon::Camera;
  case A::Exit:
  case A::ExitMenu:
    return MenuIcon::Exit;
  case A::Fullscreen:
  case A::ReturnToDesktop:
    return MenuIcon::Display;
  case A::Back:
  case A::Cancel:
    return MenuIcon::Back;
  case A::ChangeDisc:
  case A::ShowLibrary:
    return MenuIcon::Disc;
  case A::StartGame:
    return MenuIcon::Controller;
  case A::OpenGame:
    return MenuIcon::Folder;
  default:
    return MenuIcon::None;
  }
}

MenuIcon GetLibraryIcon(::BigPicture::LibraryOperation operation)
{
  using O = ::BigPicture::LibraryOperation;
  switch (operation)
  {
  case O::Search:
  case O::ClearSearch:
    return MenuIcon::Search;
  case O::AddFolder:
  case O::EnterDirectory:
  case O::ChooseFolder:
    return MenuIcon::Folder;
  case O::OpenFile:
  case O::EditPath:
    return MenuIcon::File;
  case O::OpenDisc:
  case O::BrowseDiscs:
    return MenuIcon::Disc;
  case O::Options:
  case O::Sort:
  case O::Platform:
  case O::Region:
  case O::Tag:
    return MenuIcon::Filter;
  case O::View:
    return MenuIcon::Grid;
  case O::Refresh:
  case O::ClearFilters:
    return MenuIcon::Reset;
  default:
    return MenuIcon::None;
  }
}
}  // namespace VideoCommon::BigPicture
