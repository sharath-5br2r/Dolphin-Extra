// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/FIFO/FIFOPlayerWindow.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

#include "Core/Core.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/System.h"

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/FIFO/FIFOAnalyzer.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

#include "ui_FIFOPlayerWindow.h"

FIFOPlayerWindow::FIFOPlayerWindow(FifoPlayer& fifo_player, FifoRecorder& fifo_recorder,
                                   QWidget* parent)
    : QWidget(parent), m_fifo_player(fifo_player), m_fifo_recorder(fifo_recorder),
      m_ui(std::make_unique<Ui::FIFOPlayerWindow>())
{
  m_ui->setupUi(this);
  setWindowIcon(Resources::GetAppIcon());

  m_ui->infoLabel->setFixedHeight(QFontMetrics(font()).lineSpacing() * 3);

  m_load = m_ui->buttonBox->addButton(tr("Load..."), QDialogButtonBox::ActionRole);
  m_save = m_ui->buttonBox->addButton(tr("Save..."), QDialogButtonBox::ActionRole);
  m_record = m_ui->buttonBox->addButton(tr("Record"), QDialogButtonBox::ActionRole);
  m_stop = m_ui->buttonBox->addButton(tr("Stop"), QDialogButtonBox::ActionRole);

  m_analyzer = new FIFOAnalyzer(m_fifo_player);
  m_ui->tabWidget->addTab(m_analyzer, tr("Analyze"));

  LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  UpdateInfo();

  UpdateControls();

  m_fifo_player.SetFileLoadedCallback(
      [this] { QueueOnObject(this, &FIFOPlayerWindow::OnFIFOLoaded); });
  m_fifo_player.SetFrameWrittenCallback([this] {
    QueueOnObject(this, [this] {
      UpdateInfo();
      UpdateControls();
    });
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    // We don't want to trigger OnEmulationStarted when going from Paused to Running,
    // and nothing in UpdateControls treats Paused and Running differently
    if (state == Core::State::Paused)
      state = Core::State::Running;

    // Skip redundant updates
    if (state == m_emu_state)
      return;

    UpdateControls();

    if (state == Core::State::Running)
      OnEmulationStarted();
    else if (state == Core::State::Uninitialized)
      OnEmulationStopped();

    m_emu_state = state;
  });

  installEventFilter(this);
}

FIFOPlayerWindow::~FIFOPlayerWindow()
{
  Settings::GetQSettings().setValue(QStringLiteral("fifoplayerwindow/geometry"), saveGeometry());

  m_fifo_player.SetFileLoadedCallback({});
  m_fifo_player.SetFrameWrittenCallback({});
}

void FIFOPlayerWindow::LoadSettings()
{
  restoreGeometry(
      Settings::GetQSettings().value(QStringLiteral("fifoplayerwindow/geometry")).toByteArray());

  m_ui->earlyMemoryUpdatesCheckBox->setChecked(
      Config::Get(Config::MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES));
  m_ui->loopCheckBox->setChecked(Config::Get(Config::MAIN_FIFOPLAYER_LOOP_REPLAY));
}

void FIFOPlayerWindow::ConnectWidgets()
{
  connect(m_load, &QPushButton::clicked, this, &FIFOPlayerWindow::LoadRecording);
  connect(m_save, &QPushButton::clicked, this, &FIFOPlayerWindow::SaveRecording);
  connect(m_record, &QPushButton::clicked, this, &FIFOPlayerWindow::StartRecording);
  connect(m_stop, &QPushButton::clicked, this, &FIFOPlayerWindow::StopRecording);
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &FIFOPlayerWindow::hide);
  connect(m_ui->earlyMemoryUpdatesCheckBox, &QCheckBox::toggled, this,
          &FIFOPlayerWindow::OnConfigChanged);
  connect(m_ui->loopCheckBox, &QCheckBox::toggled, this, &FIFOPlayerWindow::OnConfigChanged);

  connect(m_ui->frameRangeFromSpinBox, &QSpinBox::valueChanged, this,
          &FIFOPlayerWindow::OnLimitsChanged);
  connect(m_ui->frameRangeToSpinBox, &QSpinBox::valueChanged, this,
          &FIFOPlayerWindow::OnLimitsChanged);

  connect(m_ui->objectRangeFromSpinBox, &QSpinBox::valueChanged, this,
          &FIFOPlayerWindow::OnLimitsChanged);
  connect(m_ui->objectRangeToSpinBox, &QSpinBox::valueChanged, this,
          &FIFOPlayerWindow::OnLimitsChanged);
}

void FIFOPlayerWindow::AddDescriptions()
{
  static const char TR_MEMORY_UPDATES_DESCRIPTION[] = QT_TR_NOOP(
      "If enabled, then all memory updates happen at once before the first frame.<br><br>"
      "Causes issues with many fifologs, but can be useful for testing.<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_LOOP_DESCRIPTION[] =
      QT_TR_NOOP("If unchecked, then playback of the fifolog stops after the final frame.<br><br>"
                 "This is generally only useful when a frame-dumping option is enabled.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");

  ConfigWidget::SetDescription(m_ui->earlyMemoryUpdatesCheckBox, {},
                               tr(TR_MEMORY_UPDATES_DESCRIPTION));
  ConfigWidget::SetDescription(m_ui->loopCheckBox, {}, tr(TR_LOOP_DESCRIPTION));
}

void FIFOPlayerWindow::LoadRecording()
{
  QString path = DolphinFileDialog::getOpenFileName(this, tr("Open FIFO Log"), QString(),
                                                    tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  emit LoadFIFORequested(path);
}

void FIFOPlayerWindow::SaveRecording()
{
  QString path = DolphinFileDialog::getSaveFileName(this, tr("Save FIFO Log"), QString(),
                                                    tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  FifoDataFile* file = m_fifo_recorder.GetRecordedFile();

  bool result = file->Save(path.toStdString());

  if (!result)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to save FIFO log."));
  }
}

void FIFOPlayerWindow::StartRecording()
{
  // Start recording
  m_fifo_recorder.StartRecording(m_ui->frameRecordCountSpinBox->value(),
                                 [this] { QueueOnObject(this, [this] { OnRecordingDone(); }); });

  UpdateControls();

  UpdateInfo();
}

void FIFOPlayerWindow::StopRecording()
{
  m_fifo_recorder.StopRecording();

  UpdateControls();
  UpdateInfo();
}

void FIFOPlayerWindow::OnEmulationStarted()
{
  if (m_fifo_player.GetFile())
    OnFIFOLoaded();
}

void FIFOPlayerWindow::OnEmulationStopped()
{
  // If we have previously been recording, stop now.
  if (m_fifo_recorder.IsRecording())
    StopRecording();

  // When emulation stops, switch away from the analyzer tab, as it no longer shows anything useful
  m_ui->tabWidget->setCurrentWidget(m_ui->playRecordTab);
  m_analyzer->Update();
}

void FIFOPlayerWindow::OnRecordingDone()
{
  UpdateInfo();
  UpdateControls();
}

void FIFOPlayerWindow::UpdateInfo()
{
  if (m_fifo_player.IsPlaying())
  {
    FifoDataFile* file = m_fifo_player.GetFile();
    m_ui->infoLabel->setText(tr("%1 frame(s)\n%2 object(s)\nCurrent Frame: %3")
                                 .arg(QString::number(file->GetFrameCount()),
                                      QString::number(m_fifo_player.GetCurrentFrameObjectCount()),
                                      QString::number(m_fifo_player.GetCurrentFrameNum())));
    return;
  }

  if (m_fifo_recorder.IsRecordingDone())
  {
    FifoDataFile* file = m_fifo_recorder.GetRecordedFile();
    size_t fifo_bytes = 0;
    size_t mem_bytes = 0;

    for (u32 i = 0; i < file->GetFrameCount(); ++i)
    {
      fifo_bytes += file->GetFrame(i).fifoData.size();
      for (const auto& mem_update : file->GetFrame(i).memoryUpdates)
        mem_bytes += mem_update.data.size();
    }

    m_ui->infoLabel->setText(tr("%1 FIFO bytes\n%2 memory bytes\n%3 frames")
                                 .arg(QString::number(fifo_bytes), QString::number(mem_bytes),
                                      QString::number(file->GetFrameCount())));
    return;
  }

  if (Core::IsRunning(Core::System::GetInstance()) && m_fifo_recorder.IsRecording())
  {
    m_ui->infoLabel->setText(tr("Recording..."));
    return;
  }

  m_ui->infoLabel->setText(tr("No file loaded / recorded."));
}

void FIFOPlayerWindow::OnFIFOLoaded()
{
  FifoDataFile* file = m_fifo_player.GetFile();

  auto object_count = m_fifo_player.GetMaxObjectCount();
  auto frame_count = file->GetFrameCount();

  m_ui->frameRangeToSpinBox->setMaximum(frame_count - 1);
  m_ui->objectRangeToSpinBox->setMaximum(object_count - 1);

  m_ui->frameRangeFromSpinBox->setValue(0);
  m_ui->objectRangeFromSpinBox->setValue(0);
  m_ui->frameRangeToSpinBox->setValue(frame_count - 1);
  m_ui->objectRangeToSpinBox->setValue(object_count - 1);

  UpdateInfo();
  UpdateLimits();
  UpdateControls();

  m_analyzer->Update();
}

void FIFOPlayerWindow::OnConfigChanged()
{
  Config::SetBase(Config::MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES,
                  m_ui->earlyMemoryUpdatesCheckBox->isChecked());
  Config::SetBase(Config::MAIN_FIFOPLAYER_LOOP_REPLAY, m_ui->loopCheckBox->isChecked());
}

void FIFOPlayerWindow::OnLimitsChanged()
{
  FifoPlayer& player = m_fifo_player;

  player.SetFrameRangeStart(m_ui->frameRangeFromSpinBox->value());
  player.SetFrameRangeEnd(m_ui->frameRangeToSpinBox->value());
  player.SetObjectRangeStart(m_ui->objectRangeFromSpinBox->value());
  player.SetObjectRangeEnd(m_ui->objectRangeToSpinBox->value());
  UpdateLimits();
}

void FIFOPlayerWindow::UpdateLimits()
{
  m_ui->frameRangeFromSpinBox->setMaximum(m_ui->frameRangeToSpinBox->value());
  m_ui->frameRangeToSpinBox->setMinimum(m_ui->frameRangeFromSpinBox->value());
  m_ui->objectRangeFromSpinBox->setMaximum(m_ui->objectRangeToSpinBox->value());
  m_ui->objectRangeToSpinBox->setMinimum(m_ui->objectRangeFromSpinBox->value());
}

void FIFOPlayerWindow::UpdateControls()
{
  Core::System& system = Core::System::GetInstance();
  const bool core_is_uninitialized = Core::IsUninitialized(system);
  const bool core_is_running = Core::IsRunning(system);
  const bool is_recording = m_fifo_recorder.IsRecording();
  const bool is_playing = m_fifo_player.IsPlaying();

  m_ui->frameRangeFromSpinBox->setEnabled(is_playing);
  m_ui->frameRangeFromLabel->setEnabled(is_playing);
  m_ui->frameRangeToSpinBox->setEnabled(is_playing);
  m_ui->frameRangeToLabel->setEnabled(is_playing);
  m_ui->objectRangeFromSpinBox->setEnabled(is_playing);
  m_ui->objectRangeFromLabel->setEnabled(is_playing);
  m_ui->objectRangeToSpinBox->setEnabled(is_playing);
  m_ui->objectRangeToLabel->setEnabled(is_playing);

  bool enable_frame_record_count = !is_playing && !is_recording;

  m_ui->frameRecordCountLabel->setEnabled(enable_frame_record_count);
  m_ui->frameRecordCountSpinBox->setEnabled(enable_frame_record_count);

  m_load->setEnabled(core_is_uninitialized);
  m_record->setEnabled(core_is_running && !is_playing);

  m_stop->setVisible(core_is_running && is_recording);
  m_record->setVisible(!m_stop->isVisible());

  m_save->setEnabled(m_fifo_recorder.IsRecordingDone());
}

bool FIFOPlayerWindow::eventFilter(QObject* object, QEvent* event)
{
  // Close when escape is pressed
  if (event->type() == QEvent::KeyPress)
  {
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Cancel))
      hide();
  }

  return false;
}
