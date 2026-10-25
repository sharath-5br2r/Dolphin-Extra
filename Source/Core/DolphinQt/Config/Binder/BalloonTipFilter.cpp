// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/BalloonTipFilter.h"

#include <utility>

#include <QEvent>
#include <QTimerEvent>
#include <QWidget>

#include "DolphinQt/Config/Binder/BalloonTip.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace ConfigWidget
{
namespace
{
constexpr int TOOLTIP_DELAY_MS = 300;

// GUI-thread-only, unguarded. Test fixtures reset to {} in TearDown to restore the real balloon.
BalloonPresenter s_balloon_presenter;
}  // namespace

BalloonTipFilter::BalloonTipFilter(QWidget* widget) : QObject(widget)
{
  widget->installEventFilter(this);
}

BalloonTipFilter::~BalloonTipFilter() = default;

void BalloonTipFilter::SetText(QString title, QString description)
{
  m_title = std::move(title);
  m_description = std::move(description);
}

QWidget* BalloonTipFilter::GetWidget() const
{
  return qobject_cast<QWidget*>(parent());
}

bool BalloonTipFilter::eventFilter(QObject* watched, QEvent* event)
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return QObject::eventFilter(watched, event);

  // The anchor is computed from the watched widget, so acting on some other object's hover would
  // show a balloon pointing at this one. The original was a virtual override and could not.
  if (watched != widget)
    return QObject::eventFilter(watched, event);

  switch (event->type())
  {
  case QEvent::Enter:
    // Don't restart if a timer is already running, or if the cursor is re-entering the widget after
    // having hovered the balloon itself.
    if (!m_timer_id && !BalloonTip::IsWidgetBalloonTipActive(*widget))
      m_timer_id = startTimer(TOOLTIP_DELAY_MS);
    break;

  case QEvent::Leave:
    // If the cursor is still inside the widget's bounding box but the balloon is covering that part
    // of it, keep the balloon open: it tracks the cursor and closes itself.
    if (!BalloonTip::IsCursorInsideWidgetBoundingBox(*widget) ||
        !BalloonTip::IsCursorOnBalloonTip())
      CancelPendingTooltip();
    break;

  case QEvent::Hide:
    CancelPendingTooltip();
    break;

  default:
    break;
  }

  // Never swallow: these are observations, and the widget still needs its own hover handling.
  return QObject::eventFilter(watched, event);
}

void BalloonTipFilter::timerEvent(QTimerEvent* event)
{
  if (!m_timer_id || event->timerId() != *m_timer_id)
  {
    QObject::timerEvent(event);
    return;
  }

  KillPendingTimer();
  ShowTooltip();
}

void BalloonTipFilter::ShowTooltip()
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return;

  // ToolTipWidget mapped through parentWidget(), which crashed for a parentless widget. The anchor
  // is widget-local, so mapping through the widget itself gives the same point and no crash.
  const QPoint global_position = widget->mapToGlobal(ToolTipAnchor(widget));
  if (s_balloon_presenter)
  {
    s_balloon_presenter(m_title, m_description, global_position, widget);
    return;
  }
  BalloonTip::ShowBalloon(m_title, m_description, global_position, widget);
}

void BalloonTipFilter::ShowTooltipNowForTesting()
{
  // Cancel any pending timer before showing, which takes the same path the timer does. Not
  // CancelPendingTooltip: that also hides the balloon this is about to show.
  KillPendingTimer();
  ShowTooltip();
}

void BalloonTipFilter::KillPendingTimer()
{
  if (m_timer_id)
  {
    killTimer(*m_timer_id);
    m_timer_id.reset();
  }
}

void BalloonTipFilter::CancelPendingTooltip()
{
  KillPendingTimer();
  BalloonTip::HideBalloon();
}

void SetBalloonPresenterForTesting(BalloonPresenter presenter)
{
  s_balloon_presenter = std::move(presenter);
}
}  // namespace ConfigWidget
