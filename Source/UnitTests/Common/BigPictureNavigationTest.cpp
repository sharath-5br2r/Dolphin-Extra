// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <memory>

#include <gtest/gtest.h>

#include "Common/BigPicture/Navigation.h"
#include "Common/BigPicture/PresentationState.h"

using namespace BigPicture;

TEST(BigPictureNavigationTest, DirectionHysteresisRetainsHoldWithoutRepeatedTransitions)
{
  NavigationFilter filter;
  NavValues values{};
  EXPECT_TRUE(filter.Update(values, true, true, 1).empty());
  values[1] = 0.54;
  EXPECT_TRUE(filter.Update(values, true, true, 1).empty());
  values[1] = 0.56;
  const auto press = filter.Update(values, true, true, 1);
  ASSERT_EQ(press.size(), 1u);
  EXPECT_EQ(press[0].button, NavButton::Up);
  EXPECT_TRUE(press[0].down);
  for (double value : {0.9, 0.56, 0.4, 0.31})
  {
    values[1] = value;
    EXPECT_TRUE(filter.Update(values, true, true, 1).empty());
  }
  values[1] = 0.3;
  const auto release = filter.Update(values, true, true, 1);
  ASSERT_EQ(release.size(), 1u);
  EXPECT_FALSE(release[0].down);
}

TEST(BigPictureNavigationTest, OpeningChordAndOtherHeldControlsRequireTheirOwnRelease)
{
  NavigationFilter filter;
  NavValues values{};
  filter.Update(values, true, true, 1);
  values[0] = values[5] = 1;
  ASSERT_EQ(filter.Update(values, true, true, 1).size(), 2u);
  const auto reset = filter.Update(values, true, true, 2);
  ASSERT_EQ(reset.size(), 2u);
  EXPECT_FALSE(reset[0].down);
  EXPECT_FALSE(reset[1].down);
  EXPECT_TRUE(filter.Update(values, true, true, 2).empty());
  values[0] = 0;
  filter.Update(values, true, true, 2);
  values[0] = 1;
  const auto repress = filter.Update(values, true, true, 2);
  ASSERT_EQ(repress.size(), 1u);
  EXPECT_EQ(repress[0].button, NavButton::Menu);
  EXPECT_TRUE(repress[0].down);
}

TEST(BigPictureNavigationTest, DisconnectReconnectAndFocusRegainCannotSynthesizeAPress)
{
  NavigationFilter filter;
  NavValues values{};
  filter.Update(values, true, true, 1);
  values[5] = 1;
  ASSERT_EQ(filter.Update(values, true, true, 1).size(), 1u);
  const auto disconnected = filter.Update(values, false, true, 1);
  ASSERT_EQ(disconnected.size(), 1u);
  EXPECT_FALSE(disconnected[0].down);
  EXPECT_TRUE(filter.Update(values, true, true, 1).empty());
  values[5] = 0;
  filter.Update(values, true, true, 1);
  values[5] = 1;
  ASSERT_EQ(filter.Update(values, true, true, 1).size(), 1u);
  const auto lost_focus = filter.Update(values, true, false, 1);
  ASSERT_EQ(lost_focus.size(), 1u);
  EXPECT_FALSE(lost_focus[0].down);
  EXPECT_TRUE(filter.Update(values, true, true, 1).empty());
}

TEST(BigPictureNavigationTest, InvalidValuesAndPreferencesRemainFiniteAndBounded)
{
  NavigationFilter filter;
  NavValues values{};
  filter.Update(values, true, true, 0);
  values[5] = std::numeric_limits<double>::quiet_NaN();
  values[6] = std::numeric_limits<double>::infinity();
  EXPECT_TRUE(filter.Update(values, true, true, 0).empty());
  const auto options = SanitizeNavigationOptions({true, 99, -1});
  EXPECT_EQ(options, (NavigationOptions{true, 1.5f, 0}));
  EXPECT_EQ(SanitizeNavigationOptions({false, std::numeric_limits<float>::infinity(),
                                       std::numeric_limits<float>::quiet_NaN()}),
            NavigationOptions{});
}

TEST(BigPictureNavigationTest, RenderRequestsAreBoundedAndCannotSurviveFocusOrSessionChanges)
{
  PresentationState state;
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->mode = Mode::InGame;
  snapshot->host.lifecycle = Lifecycle::Paused;
  state.Publish(snapshot);
  state.RequestNavigation({2, Action::Settings});
  EXPECT_TRUE(state.TakeNavigationRequests().empty());
  for (int i = 0; i < 40; ++i)
    state.RequestNavigation({1, Action::Settings});
  EXPECT_EQ(state.TakeNavigationRequests().size(), 32u);
  state.RequestNavigation({1, Action::Settings});
  state.SetFocused(false);
  EXPECT_TRUE(state.TakeNavigationRequests().empty());
  state.RequestNavigation({1, Action::Settings});
  EXPECT_TRUE(state.TakeNavigationRequests().empty());
  state.SetFocused(true);
  const auto epoch = state.GetFrameInputEpoch();
  state.ResetInput();
  state.RequestNavigation({1, Action::Settings}, epoch);
  EXPECT_TRUE(state.TakeNavigationRequests().empty());
  state.RequestNavigation({1, Action::Settings});
  snapshot = std::make_shared<Snapshot>(*snapshot);
  snapshot->generation = 2;
  state.Publish(snapshot);
  EXPECT_TRUE(state.TakeNavigationRequests().empty());
}

TEST(BigPictureNavigationTest, OpeningCaptureCoversQtDeliveryAndCannotBecomeStuck)
{
  PresentationState state;
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->host.lifecycle = Lifecycle::Running;
  state.Publish(snapshot);
  const auto epoch = state.BeginMenuRequest(1, state.GetFrameInputEpoch());
  ASSERT_TRUE(epoch);
  EXPECT_TRUE(state.IsModal());
  EXPECT_TRUE(state.GetCapture().modal);
  EXPECT_FALSE(state.BeginMenuRequest(1, *epoch));
  // A non-transitioning host refresh must not drop capture while delivery is pending.
  state.Publish(snapshot);
  EXPECT_TRUE(state.IsModal());
  state.FinishMenuRequest(*epoch);
  EXPECT_FALSE(state.IsModal());
  const auto stale_epoch = state.BeginMenuRequest(1, state.GetFrameInputEpoch());
  ASSERT_TRUE(stale_epoch);
  state.ResetInput();
  EXPECT_FALSE(state.IsModal());
  const auto next_epoch = state.BeginMenuRequest(1, state.GetFrameInputEpoch());
  ASSERT_TRUE(next_epoch);
  state.FinishMenuRequest(*stale_epoch);
  EXPECT_TRUE(state.IsModal());
  state.SetFocused(false);
  EXPECT_FALSE(state.IsModal());
  state.SetFocused(true);
  ASSERT_TRUE(state.BeginMenuRequest(1, state.GetFrameInputEpoch()));
  snapshot = std::make_shared<Snapshot>(*snapshot);
  snapshot->generation = 2;
  state.Publish(snapshot);
  EXPECT_FALSE(state.IsModal());
}
