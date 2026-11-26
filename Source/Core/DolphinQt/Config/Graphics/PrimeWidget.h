// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class GraphicsPane;
class QCheckBox;
class QLabel;
class QRadioButton;
class QSlider;
class QSpinBox;
class QCheckBox;
class QSlider;
class QSpinBox;
class QRadioButton;
class QPushButton;
class QColorDialog;

class PrimeWidget final : public QWidget {
  Q_OBJECT
public:
  explicit PrimeWidget(GraphicsPane* parent);

private:
  void ToggleShowCrosshair(bool mode);
  void ArmPositionModeChanged(bool mode);

  // General
  QCheckBox* m_disable_bloom;
  QCheckBox* m_reduce_bloom;
  QSlider* m_bloom_intensity;
  QSpinBox* m_bloom_intensity_val;
  QCheckBox* m_motions_lock;
  QCheckBox* m_toggle_culling;
  QCheckBox* m_toggle_secondaryFX;
  QCheckBox* m_autofogtoggle_mp3;

  //Crosshair
  QCheckBox* m_toggle_gc_show_crosshair;
  QPushButton* m_select_colour;
  QPushButton* m_reset_colour;
  QColorDialog* colorpicker;
  
  QSpinBox* y_counter;
  QSpinBox* x_counter;
  QSpinBox* z_counter;
  
  QSlider* m_y_axis;
  QSlider* m_x_axis;
  QSlider* m_z_axis;

  // FOV
  QCheckBox* m_fov_toggle;
  QSpinBox* m_fov_counter;
  QSlider* m_fov_axis;

  // Viewmodel
  QCheckBox* m_toggle_arm_position;
  QRadioButton* m_auto_arm_position;
  QRadioButton* m_manual_arm_position;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();
};
