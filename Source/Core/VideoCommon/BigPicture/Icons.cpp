// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BigPicture/Icons.h"

#include <algorithm>
#include <cmath>
#include <imgui_internal.h>

namespace VideoCommon::BigPicture
{
ImWchar GetMenuIconCodepoint(MenuIcon icon)
{
  switch (icon)
  {
  case MenuIcon::Locked:
    return 0xf023;
  case MenuIcon::Unlocked:
    return 0xf09c;
  case MenuIcon::Play:
    return 0xf04b;
  case MenuIcon::Stop:
    return 0xf04d;
  case MenuIcon::Power:
    return 0xf011;
  case MenuIcon::Trophy:
    return 0xf091;
  case MenuIcon::StateSave:
    return 0xf019;
  case MenuIcon::StateLoad:
    return 0xf2ea;
  case MenuIcon::Save:
    return 0xf0c7;
  case MenuIcon::Load:
    return 0xf07c;
  case MenuIcon::Settings:
    return 0x2699;
  case MenuIcon::Reset:
    return 0xf2ea;
  case MenuIcon::Camera:
    return 0xf030;
  case MenuIcon::Exit:
    return 0xf2f5;
  case MenuIcon::Display:
    return 0xf26c;
  case MenuIcon::Grid:
    return 0xf00a;
  case MenuIcon::List:
    return 0xf03a;
  case MenuIcon::Search:
    return 0xf002;
  case MenuIcon::Folder:
    return 0xf07c;
  case MenuIcon::FolderSolid:
    return 0xf07b;
  case MenuIcon::Back:
    return 0xf060;
  case MenuIcon::Controller:
    return 0x2360;
  case MenuIcon::Audio:
    return 0x2365;
  case MenuIcon::Disc:
    return 0xf51f;
  case MenuIcon::File:
    return 0xf15b;
  case MenuIcon::Filter:
    return 0xf0b0;
  case MenuIcon::Theme:
    return 0xf1fc;
  case MenuIcon::Check:
    return 0xf00c;
  case MenuIcon::Globe:
    return 0xf0ac;
  case MenuIcon::Image:
    return 0x2366;
  case MenuIcon::CPU:
    return 0x235e;
  case MenuIcon::Keyboard:
    return 0x2361;
  case MenuIcon::Warning:
    return 0xf071;
  case MenuIcon::Network:
    return 0xf6ff;
  case MenuIcon::Achievements:
    return 0xf091;
  case MenuIcon::Speed:
    return 0xf624;
  case MenuIcon::Memory:
    return 0x2364;
  case MenuIcon::Mouse:
    return 0xf8cc;
  case MenuIcon::Clock:
    return 0xf017;
  case MenuIcon::Window:
    return 0xf2d0;
  case MenuIcon::Cube:
    return 0xf1b2;
  case MenuIcon::Sliders:
    return 0xf1de;
  case MenuIcon::Wrench:
    return 0xf0ad;
  case MenuIcon::Remote:
    return 0xf3cd;
  case MenuIcon::Puzzle:
    return 0xf12e;
  default:
    return 0;
  }
}

void DrawMenuIcon(ImDrawList* draw, MenuIcon icon, ImVec2 position, float size, ImU32 color)
{
  if (const auto codepoint = GetMenuIconCodepoint(icon))
  {
    auto* font = ImGui::GetFont();
    float font_size = size;
    if (const auto* glyph = font->GetFontBaked(font_size)->FindGlyphNoFallback(codepoint))
    {
      const float width = glyph->X1 - glyph->X0;
      const float height = glyph->Y1 - glyph->Y0;
      font_size *= std::min({1.0f, size / std::max(1.0f, width), size / std::max(1.0f, height)});
      glyph = font->GetFontBaked(font_size)->FindGlyphNoFallback(codepoint);
      if (glyph)
      {
        char text[5]{};
        ImTextCharToUtf8(text, codepoint);
        draw->AddText(font, font_size,
                      ImVec2(position.x + (size - (glyph->X1 - glyph->X0)) * 0.5f - glyph->X0,
                             position.y + (size - (glyph->Y1 - glyph->Y0)) * 0.5f - glyph->Y0),
                      color, text);
        return;
      }
    }
  }
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
    draw->AddTriangle(p(5, 3), p(21, 12), p(5, 21), color, stroke);
    break;
  case MenuIcon::Stop:
    rect(4, 4, 20, 20);
    break;
  case MenuIcon::Power:
    draw->PathArcTo(p(12, 13), 9 * scale, -0.95f, 4.09f, 32);
    draw->PathStroke(color, 0, stroke);
    line(12, 1, 12, 12);
    break;
  case MenuIcon::Trophy:
    line(6, 3, 18, 3);
    line(6, 3, 7, 11);
    line(18, 3, 17, 11);
    line(7, 11, 12, 15);
    line(17, 11, 12, 15);
    line(12, 15, 12, 21);
    line(7, 21, 17, 21);
    line(6, 5, 2, 5);
    line(2, 5, 3, 11);
    line(3, 11, 7, 12);
    line(18, 5, 22, 5);
    line(22, 5, 21, 11);
    line(21, 11, 17, 12);
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
    rect(3, 3, 21, 21);
    line(12, 3, 12, 21);
    line(3, 12, 21, 12);
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
  case MenuIcon::FolderSolid:
    draw->AddRectFilled(p(1, 7), p(23, 22), color, 2 * scale);
    draw->AddRectFilled(p(1, 3), p(11, 12), color, 2 * scale);
    draw->AddRectFilled(p(9, 5), p(23, 12), color, 2 * scale);
    break;
  case MenuIcon::Globe:
    circle(12, 12, 10);
    draw->AddEllipse(p(12, 12), ImVec2(5 * scale, 10 * scale), color, 0, 32, stroke);
    line(2, 12, 22, 12);
    break;
  case MenuIcon::Image:
    rect(2, 3, 22, 21);
    draw->AddCircleFilled(p(16, 8), 2 * scale, color);
    line(3, 19, 9, 11);
    line(9, 11, 15, 19);
    line(15, 19, 19, 14);
    line(19, 14, 22, 18);
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
  case A::Achievements:
    return MenuIcon::Achievements;
  case A::Boot:
  case A::BootIPL:
  case A::BootSystemMenu:
  case A::Resume:
    return MenuIcon::Play;
  case A::Stop:
    return MenuIcon::Power;
  case A::SaveStates:
  case A::SaveState:
    return MenuIcon::StateSave;
  case A::LoadStates:
  case A::LoadState:
    return MenuIcon::StateLoad;
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
  case O::Folders:
  case O::RemoveFolder:
    return MenuIcon::Folder;
  case O::OpenFile:
  case O::EditPath:
    return MenuIcon::File;
  case O::OpenDisc:
  case O::ChooseBootDisc:
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
