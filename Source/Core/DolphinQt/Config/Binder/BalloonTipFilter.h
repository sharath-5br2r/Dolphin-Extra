// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <QObject>
#include <QPoint>
#include <QString>

class QEvent;
class QTimerEvent;
class QWidget;

namespace ConfigWidget
{
// Implements balloon-tooltip behaviour as an event filter, so it can attach to stock widgets built
// by uic. Attached as a child of the widget it watches, it lives exactly as long as that widget.
//
// Deliberately independent of ConfigBinding: unbound controls can also publish settings help.
class BalloonTipFilter final : public QObject
{
  Q_OBJECT

public:
  explicit BalloonTipFilter(QWidget* widget);
  ~BalloonTipFilter() override;

  void SetText(QString title, QString description);
  const QString& GetTitle() const { return m_title; }
  const QString& GetDescription() const { return m_description; }

  bool HasPendingTooltipForTesting() const { return m_timer_id.has_value(); }
  // Takes the timer's path without the delay, so tests need no sleep.
  void ShowTooltipNowForTesting();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void timerEvent(QTimerEvent* event) override;

private:
  QWidget* GetWidget() const;
  void ShowTooltip();
  void KillPendingTimer();
  void CancelPendingTooltip();

  std::optional<int> m_timer_id;
  QString m_title;
  QString m_description;
};

// Test seam for the balloon itself, so a test can assert what was shown without a real BalloonTip
// on screen. These are the arguments BalloonTipFilter passes to BalloonTip::ShowBalloon; an empty
// handler means "show the real balloon", which keeps ShowBalloon's own default arguments in play.
using BalloonPresenter = std::function<void(const QString& title, const QString& description,
                                            const QPoint& global_position, QWidget* parent)>;
void SetBalloonPresenterForTesting(BalloonPresenter presenter);
}  // namespace ConfigWidget
