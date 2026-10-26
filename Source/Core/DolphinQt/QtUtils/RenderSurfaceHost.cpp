// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/RenderSurfaceHost.h"

#include <utility>

#include <QEvent>
#include <QWidget>
#include <QWindow>

#ifdef _WIN32
#include <windows.h>
#endif

namespace QtUtils
{
namespace
{
class NativeRenderSurface final : public QWidget
{
public:
  explicit NativeRenderSurface(QWidget* host) : QWidget(host)
  {
    setObjectName(QStringLiteral("native-render-surface"));
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setFocusProxy(host);
  }
  QPaintEngine* paintEngine() const override { return nullptr; }

#ifdef _WIN32
  bool nativeEvent(const QByteArray& event_type, void* message, qintptr* result) override
  {
    const auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_ERASEBKGND)
    {
      // WA_PaintOnScreen bypasses Qt's backing store, including the host's black
      // palette. Define the native background before a graphics owner can present.
      RECT rect;
      GetClientRect(msg->hwnd, &rect);
      FillRect(reinterpret_cast<HDC>(msg->wParam), &rect,
               static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
      *result = 1;
      return true;
    }
    return QWidget::nativeEvent(event_type, message, result);
  }
#endif
};
}  // namespace

RenderSurfaceHost::RenderSurfaceHost(QWidget* host, std::function<void(void*)> handle_changed)
    : QObject(host), m_host(host), m_handle_changed(std::move(handle_changed))
{
  m_host->installEventFilter(this);
}

RenderSurfaceHost::~RenderSurfaceHost()
{
  m_host->removeEventFilter(this);
  if (m_surface)
  {
    m_surface->removeEventFilter(this);
    delete m_surface;
  }
}

void RenderSurfaceHost::Prepare(bool recreate)
{
  if (!m_surface || recreate)
    Recreate();
}

void RenderSurfaceHost::Recreate()
{
  // Allocate before deleting the old child: keep the host continuously visible and prevent
  // native-handle reuse from hiding a required controller/backend surface notification.
  auto* previous = m_surface.data();
  auto* surface = new NativeRenderSurface(m_host);
  surface->setGeometry(m_host->rect());
  m_surface = surface;
  surface->installEventFilter(this);
  surface->show();
#ifdef _WIN32
  // Clear only a newly created surface. Reusing a compatible surface must retain
  // its last presentation until the next owner replaces it.
  RedrawWindow(reinterpret_cast<HWND>(surface->winId()), nullptr, nullptr,
               RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
#endif
  if (previous)
  {
    previous->removeEventFilter(this);
    delete previous;
  }
  m_handle_changed(reinterpret_cast<void*>(surface->winId()));
}

QWindow* RenderSurfaceHost::GetWindow() const
{
  return m_surface ? m_surface->windowHandle() : nullptr;
}

bool RenderSurfaceHost::eventFilter(QObject* object, QEvent* event)
{
  if (object == m_host && event->type() == QEvent::Resize && m_surface)
    m_surface->setGeometry(m_host->rect());
  if (object == m_surface && event->type() == QEvent::WinIdChange && m_surface)
    m_handle_changed(reinterpret_cast<void*>(m_surface->winId()));
  return false;
}
}  // namespace QtUtils
