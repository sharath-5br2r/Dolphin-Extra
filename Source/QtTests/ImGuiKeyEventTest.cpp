// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QKeyEvent>
#include <gtest/gtest.h>

#include "Common/BigPicture/PresentationState.h"
#include "DolphinQt/QtUtils/ImGuiKeyEvent.h"

TEST(ImGuiKeyEventTest, AutoRepeatPreservesTextWithoutKeyTransitions)
{
  BigPicture::PresentationState input;
  // Cover both ordinary text and multi-byte UTF-8 from native key events.
  const QString text = QString::fromUtf8("a\xc3\xa9\xf0\x9f\x90\xac");
  QtUtils::QueueImGuiKeyEvent(input, QKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, text),
                              0);
  for (int i = 0; i < 3; ++i)
  {
    QtUtils::QueueImGuiKeyEvent(
        input, QKeyEvent(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, text, true), 0);
    QtUtils::QueueImGuiKeyEvent(
        input, QKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, text, true), 0);
  }
  QtUtils::QueueImGuiKeyEvent(input, QKeyEvent(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, text),
                              0);

  const auto frame = input.TakeInput();
  ASSERT_EQ(frame.events.size(), 5u);
  EXPECT_EQ(frame.events.front().type, BigPicture::InputEvent::Type::Key);
  EXPECT_EQ(frame.events.front().key, static_cast<std::uint32_t>(Qt::Key_A));
  EXPECT_TRUE(frame.events.front().down);
  EXPECT_EQ(frame.events.front().text, text.toUtf8().toStdString());
  for (int i = 1; i <= 3; ++i)
  {
    EXPECT_EQ(frame.events[i].type, BigPicture::InputEvent::Type::Text);
    EXPECT_EQ(frame.events[i].text, text.toUtf8().toStdString());
  }
  EXPECT_EQ(frame.events.back().type, BigPicture::InputEvent::Type::Key);
  EXPECT_EQ(frame.events.back().key, static_cast<std::uint32_t>(Qt::Key_A));
  EXPECT_FALSE(frame.events.back().down);
  EXPECT_TRUE(frame.events.back().text.empty());
}

TEST(ImGuiKeyEventTest, NonTextAutoRepeatDoesNotEnqueueInput)
{
  BigPicture::PresentationState input;
  for (const auto key : {Qt::Key_Escape, Qt::Key_Shift, Qt::Key_Backspace})
  {
    QtUtils::QueueImGuiKeyEvent(input, QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier, {}, true),
                                0);
    QtUtils::QueueImGuiKeyEvent(input, QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier, {}, true),
                                0);
  }
  EXPECT_TRUE(input.TakeInput().events.empty());
}

TEST(ImGuiKeyEventTest, PhysicalTransitionsPreserveFullQtCodes)
{
  BigPicture::PresentationState input;
  for (const auto key : {Qt::Key_Shift, Qt::Key_Space, Qt::Key_Enter})
  {
    QtUtils::QueueImGuiKeyEvent(input, QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier), 0);
    QtUtils::QueueImGuiKeyEvent(input, QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier), 0);
    const auto frame = input.TakeInput();
    ASSERT_EQ(frame.events.size(), 2u);
    EXPECT_EQ(frame.events.front().type, BigPicture::InputEvent::Type::Key);
    EXPECT_EQ(frame.events.front().key, static_cast<std::uint32_t>(key));
    EXPECT_TRUE(frame.events.front().down);
    EXPECT_EQ(frame.events.back().type, BigPicture::InputEvent::Type::Key);
    EXPECT_EQ(frame.events.back().key, static_cast<std::uint32_t>(key));
    EXPECT_FALSE(frame.events.back().down);
  }
}
