// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

#include "Core/Core.h"

class QPushButton;
class FifoPlayer;
class FifoRecorder;
class FIFOAnalyzer;

namespace Ui
{
class FIFOPlayerWindow;
}

class FIFOPlayerWindow : public QWidget
{
  Q_OBJECT
public:
  explicit FIFOPlayerWindow(FifoPlayer& fifo_player, FifoRecorder& fifo_recorder,
                            QWidget* parent = nullptr);
  ~FIFOPlayerWindow() override;

signals:
  void LoadFIFORequested(const QString& path);

private:
  void LoadSettings();
  void ConnectWidgets();
  void AddDescriptions();

  void LoadRecording();
  void SaveRecording();
  void StartRecording();
  void StopRecording();

  void OnEmulationStarted();
  void OnEmulationStopped();
  void OnLimitsChanged();
  void OnRecordingDone();
  void OnFIFOLoaded();
  void OnConfigChanged();

  void UpdateControls();
  void UpdateInfo();
  void UpdateLimits();

  bool eventFilter(QObject* object, QEvent* event) final;

  FifoPlayer& m_fifo_player;
  FifoRecorder& m_fifo_recorder;
  std::unique_ptr<Ui::FIFOPlayerWindow> m_ui;
  QPushButton* m_load;
  QPushButton* m_save;
  QPushButton* m_record;
  QPushButton* m_stop;
  FIFOAnalyzer* m_analyzer;
  Core::State m_emu_state = Core::State::Uninitialized;
};
