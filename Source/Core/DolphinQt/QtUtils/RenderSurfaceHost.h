// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>

#include <QObject>
#include <QPointer>

class QWidget;
class QWindow;

namespace QtUtils
{
// The presentation widget/window outlives graphics owners. Only this native child is replaced
// after an owner has drained and its surface requirements have changed.
class RenderSurfaceHost final : public QObject
{
public:
  RenderSurfaceHost(QWidget* host, std::function<void(void*)> handle_changed);
  ~RenderSurfaceHost() override;
  void Prepare(bool recreate);
  void Recreate();
  QWindow* GetWindow() const;

private:
  bool eventFilter(QObject* object, QEvent* event) override;
  QWidget* m_host;
  QPointer<QWidget> m_surface;
  std::function<void(void*)> m_handle_changed;
};
}  // namespace QtUtils
