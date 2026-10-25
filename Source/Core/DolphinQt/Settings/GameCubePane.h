// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include <QWidget>

#include "Common/EnumMap.h"
#include "Core/HW/EXI/EXI.h"
#include "DolphinQt/MainWindow.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QString;

namespace Ui
{
class GameCubePane;
}

class GameCubePane : public QWidget
{
  Q_OBJECT
public:
  explicit GameCubePane(MainWindow* main_window);
  ~GameCubePane() override;

  static std::string GetOpenGBARom(std::string_view title);

signals:
  void ShowTriforceWindow();

private:
  void ConfigureWidgets();
  void BindSettings();
  void PopulateDeviceChoices();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnEmulationStateChanged();

  void UpdateButton(ExpansionInterface::Slot slot);
  void OnConfigPressed(ExpansionInterface::Slot slot);

  void BrowseMemcard(ExpansionInterface::Slot slot);
  bool SetMemcard(ExpansionInterface::Slot slot, const QString& filename);
  void BrowseGCIFolder(ExpansionInterface::Slot slot);
  bool SetGCIFolder(ExpansionInterface::Slot slot, const QString& path);
  void BrowseAGPRom(ExpansionInterface::Slot slot);
  void SetAGPRom(ExpansionInterface::Slot slot, const QString& filename);

#ifdef HAS_LIBMGBA
  void BrowseGBABios();
  void BrowseGBARom(size_t index);
  void SaveRomPathChanged();
  void BrowseGBASaves();
#endif  // HAS_LIBMGBA

  std::unique_ptr<Ui::GameCubePane> m_ui;

  Common::EnumMap<QPushButton*, ExpansionInterface::Slot::SP1> m_slot_buttons;
  Common::EnumMap<QComboBox*, ExpansionInterface::Slot::SP1> m_slot_combos;

  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_memcard_path_labels;
  Common::EnumMap<QLineEdit*, ExpansionInterface::MAX_MEMCARD_SLOT> m_memcard_paths;

  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_agp_path_labels;
  Common::EnumMap<QLineEdit*, ExpansionInterface::MAX_MEMCARD_SLOT> m_agp_paths;

  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_path_labels;
  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_override_labels;
  Common::EnumMap<QLineEdit*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_paths;

  std::array<QPushButton*, 5> m_gba_browse_roms{};
  std::array<QLineEdit*, 5> m_gba_rom_edits{};
};
