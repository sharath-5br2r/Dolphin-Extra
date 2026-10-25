// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ApplicationTheme.h"

#include <QColor>

#include <gtest/gtest.h>

namespace
{
using ApplicationTheme::Type;

TEST(ApplicationTheme, NewProfilesDefaultToDark)
{
  EXPECT_EQ(ApplicationTheme::DEFAULT_TYPE, Type::Dark);
}

TEST(ApplicationTheme, LegacyStyleValuesMapToPaletteThemes)
{
  EXPECT_EQ(ApplicationTheme::Canonicalize(Type::LegacyLight), Type::Light);
  EXPECT_EQ(ApplicationTheme::Canonicalize(Type::LegacyDark), Type::Dark);
  EXPECT_EQ(ApplicationTheme::Canonicalize(static_cast<Type>(99)), Type::System);
}

TEST(ApplicationTheme, SystemAndUserStylesPreserveThePlatformStyle)
{
  for (const Type type : {Type::System, Type::User})
  {
    const ApplicationTheme::Definition definition = ApplicationTheme::GetDefinition(type);
    EXPECT_EQ(definition.type, type);
    EXPECT_FALSE(definition.use_fusion);
    EXPECT_EQ(definition.color_scheme, Qt::ColorScheme::Unknown);
    EXPECT_FALSE(definition.palette.has_value());
  }
}

TEST(ApplicationTheme, FixedThemesUseFusionAndDeclareTheirColorScheme)
{
  const ApplicationTheme::Definition light = ApplicationTheme::GetDefinition(Type::Light);
  EXPECT_TRUE(light.use_fusion);
  EXPECT_EQ(light.color_scheme, Qt::ColorScheme::Light);
  ASSERT_TRUE(light.palette.has_value());
  EXPECT_FALSE(ApplicationTheme::IsDark(*light.palette));
  EXPECT_EQ(light.palette->color(QPalette::Window), QColor(239, 239, 239));
  EXPECT_EQ(light.palette->color(QPalette::WindowText), QColor(0, 0, 0));

  for (const Type type : {Type::DarkGray, Type::Dark})
  {
    const ApplicationTheme::Definition definition = ApplicationTheme::GetDefinition(type);
    EXPECT_TRUE(definition.use_fusion);
    EXPECT_EQ(definition.color_scheme, Qt::ColorScheme::Dark);
    ASSERT_TRUE(definition.palette.has_value());
    EXPECT_TRUE(ApplicationTheme::IsDark(*definition.palette));
    EXPECT_GT(definition.palette->color(QPalette::Text).value(),
              definition.palette->color(QPalette::Base).value());
  }
}

TEST(ApplicationTheme, WindowsSystemDarkFallbackUsesTheDarkGrayPalette)
{
  const ApplicationTheme::Definition dark_gray = ApplicationTheme::GetDefinition(Type::DarkGray);
  const ApplicationTheme::Definition fallback = ApplicationTheme::GetDefinition(Type::System, true);

  EXPECT_EQ(fallback.type, Type::System);
  EXPECT_TRUE(fallback.use_fusion);
  EXPECT_EQ(fallback.color_scheme, Qt::ColorScheme::Unknown);
  ASSERT_TRUE(fallback.palette.has_value());
  ASSERT_TRUE(dark_gray.palette.has_value());
  EXPECT_EQ(*fallback.palette, *dark_gray.palette);
}
}  // namespace
