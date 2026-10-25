// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QEventLoop>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <gtest/gtest.h>

#include "DolphinQt/Config/Binder/BalloonTip.h"
#include "DolphinQt/Config/Binder/BalloonTipFilter.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

// Two of the filter's branches are deliberately untested, because both consult the real mouse
// cursor and the offscreen platform gives a test no way to put it over a widget: the Enter branch's
// `!BalloonTip::IsWidgetBalloonTipActive` clause (the cursor re-entering the widget after hovering
// the balloon) and the whole Leave condition (the balloon covering the part of the widget the
// cursor is over, which must keep the balloon open). Faking a cursor would test the fake.

namespace
{
class BalloonTipFilterTest : public ::testing::Test
{
protected:
  struct ShownBalloon
  {
    QString title;
    QString description;
    QPoint global_position;
    QWidget* parent = nullptr;
  };

  // Every test runs with the presenter installed, so no test ever puts a real BalloonTip on screen.
  void SetUp() override
  {
    ConfigWidget::SetBalloonPresenterForTesting(
        [this](const QString& title, const QString& description, const QPoint& global_position,
               QWidget* parent) {
          m_shown = ShownBalloon{title, description, global_position, parent};
        });
  }

  void TearDown() override
  {
    ConfigWidget::SetBalloonPresenterForTesting({});
    BalloonTip::HideBalloon();
  }

  static void SendEnter(QWidget* widget)
  {
    QEnterEvent event{QPointF{1, 1}, QPointF{1, 1}, QPointF{1, 1}};
    QCoreApplication::sendEvent(widget, &event);
  }

  static void SendLeave(QWidget* widget)
  {
    QEvent event{QEvent::Leave};
    QCoreApplication::sendEvent(widget, &event);
  }

  std::optional<ShownBalloon> m_shown;
};
}  // namespace

TEST_F(BalloonTipFilterTest, AnchorIsTheCentreForAComboBox)
{
  QComboBox box;
  box.resize(200, 30);

  EXPECT_EQ(ConfigWidget::ToolTipAnchor(&box), QPoint(100, 15));
}

TEST_F(BalloonTipFilterTest, AnchorIsOverTheIndicatorForACheckBox)
{
  QCheckBox box{QStringLiteral("A rather long check box label")};
  box.resize(300, 24);

  const QPoint anchor = ConfigWidget::ToolTipAnchor(&box);

  // Style-independent assertion: the arrow points at the box, not at the middle of the label.
  EXPECT_GT(anchor.x(), 0);
  EXPECT_LT(anchor.x(), 40) << "anchored on the indicator, not the centre of a 300px widget";
  EXPECT_EQ(anchor.y(), 12);
}

TEST_F(BalloonTipFilterTest, AnchorIsOverTheIndicatorForARadioButton)
{
  QRadioButton button{QStringLiteral("A rather long radio button label")};
  button.resize(300, 24);

  const QPoint anchor = ConfigWidget::ToolTipAnchor(&button);

  // Style-independent assertion: the arrow points at the button, not at the middle of the label.
  EXPECT_GT(anchor.x(), 0);
  EXPECT_LT(anchor.x(), 40) << "anchored on the indicator, not the centre of a 300px widget";
  EXPECT_EQ(anchor.y(), 12);
}

TEST_F(BalloonTipFilterTest, IndicatorWidthSeesTheCheckedState)
{
  QCheckBox box{QStringLiteral("Label")};
  box.resize(200, 30);
  // Set on the widget rather than the application, so the rest of the suite stays unaffected.
  box.setStyleSheet(QStringLiteral("QCheckBox::indicator { width: 13px; height: 13px; }"
                                   "QCheckBox::indicator:checked { width: 40px; }"));
  box.setChecked(true);

  // 40 / 2. An explicit width makes QStyleSheetStyle authoritative, so this is style-independent.
  // Without State_On the option matches only the base rule and the anchor lands on 13 / 2 == 6.
  EXPECT_EQ(ConfigWidget::ToolTipAnchor(&box).x(), 20);
}

TEST_F(BalloonTipFilterTest, AnchorFollowsASliderHandle)
{
  QSlider slider{Qt::Horizontal};
  slider.resize(200, 20);
  slider.setRange(0, 100);

  slider.setValue(0);
  const QPoint low = ConfigWidget::ToolTipAnchor(&slider);
  slider.setValue(100);
  const QPoint high = ConfigWidget::ToolTipAnchor(&slider);

  EXPECT_LT(low.x(), high.x()) << "the anchor tracks the handle, not the widget centre";
}

TEST_F(BalloonTipFilterTest, AnchorFollowsAVerticalSliderHandle)
{
  QSlider slider{Qt::Vertical};
  slider.resize(20, 200);
  slider.setRange(0, 100);

  slider.setValue(0);
  const QPoint low = ConfigWidget::ToolTipAnchor(&slider);
  slider.setValue(100);
  const QPoint high = ConfigWidget::ToolTipAnchor(&slider);

  // Vertical sliders run bottom-to-top by default, so minimum should be lower (greater y).
  EXPECT_GT(low.y(), high.y()) << "the anchor tracks the handle vertically";
}

TEST_F(BalloonTipFilterTest, AnchorIsWidgetLocalSoAParentlessWidgetIsSafe)
{
  QComboBox orphan;  // no parent: the old ToolTipWidget dereferenced parentWidget() here
  orphan.resize(100, 20);

  EXPECT_EQ(ConfigWidget::ToolTipAnchor(&orphan), QPoint(50, 10));
}

TEST_F(BalloonTipFilterTest, SetDescriptionStoresTheText)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));

  EXPECT_EQ(ConfigWidget::ToolTipDescription(&box), QStringLiteral("Body"));
}

TEST_F(BalloonTipFilterTest, SetDescriptionTwiceUpdatesRatherThanStacksFilters)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("First"));
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Second"));

  EXPECT_EQ(ConfigWidget::ToolTipDescription(&box), QStringLiteral("Second"));
  EXPECT_EQ(box.findChildren<ConfigWidget::BalloonTipFilter*>().size(), 1);
}

TEST_F(BalloonTipFilterTest, AnEmptyTitleFallsBackToTheWidgetsOwnText)
{
  // Checkboxes and radio buttons use their labels as the default help title.
  QCheckBox box{QStringLiteral("Enable Progressive Scan")};
  ConfigWidget::SetDescription(&box, QString{}, QStringLiteral("Body"));

  EXPECT_EQ(ConfigWidget::ToolTipTitle(&box), QStringLiteral("Enable Progressive Scan"));
}

TEST_F(BalloonTipFilterTest, ANonEmptyTitleIsKeptRatherThanTakenFromTheLabel)
{
  QCheckBox box{QStringLiteral("Enable Progressive Scan")};
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));

  EXPECT_EQ(ConfigWidget::ToolTipTitle(&box), QStringLiteral("Title"));
}

TEST_F(BalloonTipFilterTest, AnEmptyTitleIsNotTakenFromASpinBoxsValue)
{
  QSpinBox spin_box;
  spin_box.setValue(50);
  ConfigWidget::SetDescription(&spin_box, QString{}, QStringLiteral("Body"));

  // QAbstractSpinBox::text is the current value, not a label, so the fallback must skip it.
  EXPECT_EQ(ConfigWidget::ToolTipTitle(&spin_box), QString{});
  EXPECT_EQ(ConfigWidget::ToolTipDescription(&spin_box), QStringLiteral("Body"));
}

TEST_F(BalloonTipFilterTest, HoveringSchedulesTheTooltipAndLeavingCancelsIt)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box.findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());

  SendEnter(&box);
  EXPECT_TRUE(filter->HasPendingTooltipForTesting());

  SendLeave(&box);
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());
}

TEST_F(BalloonTipFilterTest, HidingTheWidgetCancelsAPendingTooltip)
{
  QWidget parent;
  auto* const box = new QComboBox{&parent};
  ConfigWidget::SetDescription(box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box->findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);

  SendEnter(box);
  ASSERT_TRUE(filter->HasPendingTooltipForTesting());

  box->hide();
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());
}

TEST_F(BalloonTipFilterTest, ShowingTheTooltipActivatesABalloonForThatWidget)
{
  QComboBox box;
  box.resize(200, 30);
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box.findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);

  // Skips the 300ms delay and takes the same path the timer does, so this stays deterministic.
  filter->ShowTooltipNowForTesting();

  ASSERT_TRUE(m_shown.has_value());
  EXPECT_EQ(m_shown->title, QStringLiteral("Title"));
  EXPECT_EQ(m_shown->description, QStringLiteral("Body"));
  EXPECT_EQ(m_shown->global_position, box.mapToGlobal(QPoint(100, 15)));
  EXPECT_EQ(m_shown->parent, &box);
}

TEST_F(BalloonTipFilterTest, ShowingTheTooltipNowDisarmsThePendingTimer)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box.findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);

  SendEnter(&box);
  ASSERT_TRUE(filter->HasPendingTooltipForTesting());

  // Otherwise the armed timer fires afterwards and shows the same balloon a second time.
  filter->ShowTooltipNowForTesting();
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());
}

TEST_F(BalloonTipFilterTest, TheTimerShowsTheTooltipWhenItExpires)
{
  QComboBox box;
  box.resize(200, 30);
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box.findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);

  SendEnter(&box);
  ASSERT_TRUE(filter->HasPendingTooltipForTesting());

  // The only test that drives the real 300 ms timer rather than ShowTooltipNowForTesting. qt-tests
  // does not link Qt6::Test, so there is no QTest::qWait; a nested loop is the equivalent.
  QEventLoop loop;
  QTimer::singleShot(400, &loop, &QEventLoop::quit);
  loop.exec();

  EXPECT_FALSE(filter->HasPendingTooltipForTesting());
  ASSERT_TRUE(m_shown.has_value());
  EXPECT_EQ(m_shown->description, QStringLiteral("Body"));
  EXPECT_EQ(m_shown->parent, &box);
}
