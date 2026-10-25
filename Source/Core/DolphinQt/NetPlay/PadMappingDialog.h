// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

#include "Core/NetPlayProto.h"

class QCheckBox;
class QComboBox;

namespace NetPlay
{
class Player;
}

namespace Ui
{
class PadMappingDialog;
}

class PadMappingDialog : public QDialog
{
  Q_OBJECT
public:
  explicit PadMappingDialog(QWidget* widget);
  ~PadMappingDialog() override;

  int exec() override;

  NetPlay::PadMappingArray GetGCPadArray();
  NetPlay::GBAConfigArray GetGBAArray();
  NetPlay::PadMappingArray GetWiimoteArray();

private:
  void ConnectWidgets();

  void OnMappingChanged();

  NetPlay::PadMappingArray m_pad_mapping;
  NetPlay::GBAConfigArray m_gba_config;
  NetPlay::PadMappingArray m_wii_mapping;

  std::array<QComboBox*, 4> m_gc_boxes;
  std::array<QCheckBox*, 4> m_gba_boxes;
  std::array<QComboBox*, 4> m_wii_boxes;
  std::vector<const NetPlay::Player*> m_players;
  std::unique_ptr<Ui::PadMappingDialog> m_ui;
};
