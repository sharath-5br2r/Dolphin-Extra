// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QEvent>
#include <QStackedWidget>
#include <QWidget>
#include <QWindow>
#include <gtest/gtest.h>

#include "DolphinQt/QtUtils/RenderSurfaceHost.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
class VisibilityRecorder final : public QObject
{
public:
  int hides = 0;
  int reparents = 0;

private:
  bool eventFilter(QObject*, QEvent* event) override
  {
    hides += event->type() == QEvent::Hide;
    reparents += event->type() == QEvent::ParentChange;
    return false;
  }
};

TEST(RenderSurfaceHostTest, ReplacingGraphicsSurfaceKeepsPresentationWindowAndNativeHandle)
{
  QWidget host;
  host.setAttribute(Qt::WA_NativeWindow);
  host.resize(800, 600);
  host.show();
  QCoreApplication::processEvents();
  const auto window = host.winId();
  VisibilityRecorder events;
  host.installEventFilter(&events);
  void* published = nullptr;
  QtUtils::RenderSurfaceHost surfaces(&host, [&](void* handle) { published = handle; });
  surfaces.Recreate();
  ASSERT_NE(surfaces.GetWindow(), nullptr);
  const auto previous = surfaces.GetWindow()->winId();
  EXPECT_EQ(published, reinterpret_cast<void*>(previous));
  surfaces.Recreate();
  QCoreApplication::processEvents();
  EXPECT_NE(surfaces.GetWindow()->winId(), previous);
  EXPECT_EQ(published, reinterpret_cast<void*>(surfaces.GetWindow()->winId()));
  EXPECT_EQ(host.winId(), window);
  EXPECT_TRUE(host.isVisible());
  EXPECT_EQ(events.hides, 0);
  EXPECT_EQ(events.reparents, 0);
}

TEST(RenderSurfaceHostTest, EmbeddedPresentationNeverSwitchesTheStackBackToDesktop)
{
  QStackedWidget stack;
  auto* desktop = new QWidget;
  auto* host = new QWidget;
  stack.addWidget(desktop);
  stack.addWidget(host);
  stack.setCurrentWidget(host);
  stack.resize(800, 600);
  stack.show();
  QCoreApplication::processEvents();
  VisibilityRecorder events;
  host->installEventFilter(&events);
  QtUtils::RenderSurfaceHost surfaces(host, [](void*) {});
  for (int i = 0; i < 3; ++i)
  {
    surfaces.Recreate();
    stack.resize(900 + i * 10, 700 + i * 10);
    QCoreApplication::processEvents();
    EXPECT_EQ(stack.currentWidget(), host);
    EXPECT_FALSE(desktop->isVisible());
    EXPECT_EQ(surfaces.GetWindow()->size(), host->size());
  }
  EXPECT_EQ(events.hides, 0);
  EXPECT_EQ(events.reparents, 0);
}

TEST(RenderSurfaceHostTest, CompatibleHandoffsReuseTheDrawingSurfaceWithoutNativeEvents)
{
  QWidget host;
  host.resize(800, 600);
  host.show();
  int handle_changes = 0;
  QtUtils::RenderSurfaceHost surfaces(&host, [&](void*) { ++handle_changes; });
  surfaces.Prepare(false);
  const auto window = surfaces.GetWindow()->winId();
  const int initial_changes = handle_changes;
  VisibilityRecorder events;
  host.installEventFilter(&events);
  for (int i = 0; i < 3; ++i)
  {
    surfaces.Prepare(false);
    QCoreApplication::processEvents();
    EXPECT_EQ(surfaces.GetWindow()->winId(), window);
  }
  EXPECT_EQ(handle_changes, initial_changes);
  EXPECT_EQ(events.hides, 0);
  EXPECT_EQ(events.reparents, 0);
  surfaces.Prepare(true);
  EXPECT_NE(surfaces.GetWindow()->winId(), window);
}

TEST(RenderSurfaceHostTest, FullscreenPresentationPersistsAcrossGraphicsOwnerChanges)
{
  QWidget host;
  host.setAttribute(Qt::WA_NativeWindow);
  host.showFullScreen();
  QCoreApplication::processEvents();
  const auto window = host.winId();
  VisibilityRecorder events;
  host.installEventFilter(&events);
  QtUtils::RenderSurfaceHost surfaces(&host, [](void*) {});
  for (int i = 0; i < 3; ++i)
  {
    surfaces.Recreate();
    QCoreApplication::processEvents();
    EXPECT_TRUE(host.isFullScreen());
    EXPECT_EQ(host.winId(), window);
  }
  EXPECT_EQ(events.hides, 0);
  EXPECT_EQ(events.reparents, 0);
}

#ifdef _WIN32
TEST(RenderSurfaceHostTest, FreshSurfaceErasesToBlackBeforeGraphicsInitialization)
{
  QWidget host;
  host.resize(64, 64);
  host.show();
  QtUtils::RenderSurfaceHost surfaces(&host, [](void*) {});
  surfaces.Recreate();
  QCoreApplication::processEvents();

  // Exercise the native erase message with a white bitmap instead of depending on the
  // test desktop's foreground window or a GPU capture of an uninitialized swap chain.
  const auto window = reinterpret_cast<HWND>(surfaces.GetWindow()->winId());
  const auto screen = GetDC(window);
  const auto target = CreateCompatibleDC(screen);
  const auto bitmap = CreateCompatibleBitmap(screen, 64, 64);
  const auto previous = SelectObject(target, bitmap);
  PatBlt(target, 0, 0, 64, 64, WHITENESS);
  SendMessage(window, WM_ERASEBKGND, reinterpret_cast<WPARAM>(target), 0);
  EXPECT_EQ(GetPixel(target, 32, 32), RGB(0, 0, 0));
  SelectObject(target, previous);
  DeleteObject(bitmap);
  DeleteDC(target);
  ReleaseDC(window, screen);
}
#endif
}  // namespace
