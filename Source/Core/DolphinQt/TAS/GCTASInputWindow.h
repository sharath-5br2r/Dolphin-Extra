// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/TAS/TASInputWindow.h"

class QHideEvent;
class QShowEvent;
class TASCheckBox;

class GCTASInputWindow : public TASInputWindow
{
  Q_OBJECT
public:
  explicit GCTASInputWindow(QWidget* parent, int controller_id);

  void hideEvent(QHideEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  int m_controller_id;

  InputOverrider m_overrider;

  TASCheckBox* m_a_button;
  TASCheckBox* m_b_button;
  TASCheckBox* m_x_button;
  TASCheckBox* m_y_button;
  TASCheckBox* m_z_button;
  TASCheckBox* m_l_button;
  TASCheckBox* m_r_button;
  TASCheckBox* m_start_button;
  TASCheckBox* m_left_button;
  TASCheckBox* m_up_button;
  TASCheckBox* m_down_button;
  TASCheckBox* m_right_button;
};
