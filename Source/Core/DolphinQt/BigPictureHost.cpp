// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureHost.h"

#include <utility>

#include <QThread>

#include "Common/BigPicture/PresentationState.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

BigPictureHost::BigPictureHost(QObject* parent, ReadStateCallback read_state,
                               ExecuteCallback execute, PauseCallback pause,
                               PublishCallback publish)
    : QObject(parent), m_read_state(std::move(read_state)), m_execute(std::move(execute)),
      m_pause(std::move(pause)), m_publish(std::move(publish)), m_controller(*this)
{
  Publish();
}

BigPictureHost::~BigPictureHost()
{
  if (m_publish)
    m_publish(nullptr);
}

BigPicture::HostState BigPictureHost::ReadState() const
{
  Q_ASSERT(QThread::currentThread() == thread());
  return m_read_state();
}

BigPicture::Result BigPictureHost::Execute(const BigPicture::Command& command)
{
  Q_ASSERT(QThread::currentThread() == thread());
  const auto reason = BigPicture::GetAvailability(command.action, ReadState());
  if (reason != BigPicture::UnavailableReason::None)
    return {command, BigPicture::ResultStatus::Unavailable, reason};
  return m_execute(command);
}

void BigPictureHost::Submit(BigPicture::Command command, std::optional<std::uint64_t> input_epoch)
{
  auto next = m_next_id.load();
  while (next <= command.id && !m_next_id.compare_exchange_weak(next, command.id + 1))
  {
  }
  // Always queue, including on the UI thread. A drawing/input callback never executes a host
  // action inline, and Qt discards the callback if the host is destroyed before delivery.
  QueueOnObject(this, [this, command = std::move(command), input_epoch] {
    if (input_epoch && !BigPicture::GetPresentationState().IsInputCurrent(*input_epoch))
    {
      if (command.action == BigPicture::Action::OpenMenu)
        BigPicture::GetPresentationState().FinishMenuRequest(input_epoch);
      return;
    }
    m_controller.Submit(command);
    Publish();
    if (command.action == BigPicture::Action::OpenMenu)
      BigPicture::GetPresentationState().FinishMenuRequest(input_epoch);
  });
}

void BigPictureHost::RequestBack()
{
  RequestAction(GetSnapshot()->generation, BigPicture::Action::Back);
}

void BigPictureHost::RequestAction(std::uint64_t generation, BigPicture::Action action,
                                   std::optional<std::uint64_t> input_epoch, int slot,
                                   std::uint64_t confirmation_id, std::string path,
                                   BigPicture::LibraryOperation library_operation,
                                   std::uint64_t library_revision)
{
  Submit({generation, m_next_id.fetch_add(1), action, std::move(path), slot, confirmation_id,
          library_operation, library_revision},
         input_epoch);
}

void BigPictureHost::RequestSetting(std::uint64_t generation, Common::Settings::Request request,
                                    std::optional<std::uint64_t> input_epoch)
{
  BigPicture::Command command{generation, m_next_id.fetch_add(1), BigPicture::Action::EditSetting};
  command.setting = std::move(request);
  Submit(std::move(command), input_epoch);
}

void BigPictureHost::InvalidatePauseOwnership()
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_owns_pause = false;
  ++m_pause_invalidation;
}

void BigPictureHost::RequestSettingsMenu(std::uint64_t generation,
                                         BigPicture::SettingsRequest request,
                                         std::optional<std::uint64_t> input_epoch)
{
  BigPicture::Command command{generation, m_next_id.fetch_add(1), BigPicture::Action::SettingsMenu};
  command.settings_menu = std::move(request);
  Submit(std::move(command), input_epoch);
}

std::shared_ptr<const BigPicture::Snapshot> BigPictureHost::GetSnapshot() const
{
  std::lock_guard lock(m_snapshot_mutex);
  return m_snapshot;
}

void BigPictureHost::Publish()
{
  auto snapshot = m_controller.GetSnapshot();
  if (m_finish_stop_to_desktop && snapshot->host.lifecycle == BigPicture::Lifecycle::Idle &&
      !snapshot->pending)
  {
    m_finish_stop_to_desktop = false;
    m_controller.Submit(
        {snapshot->generation, m_next_id.fetch_add(1), BigPicture::Action::ReturnToDesktop});
    snapshot = m_controller.GetSnapshot();
  }
  // Capture starts before requesting a pause, and stops before resuming. No snapshot mutex
  // is held while touching core state or waking a graphics owner.
  if (m_publish)
    m_publish(snapshot);
  const bool open = snapshot->mode == BigPicture::Mode::InGame &&
                    (snapshot->host.lifecycle == BigPicture::Lifecycle::Running ||
                     snapshot->host.lifecycle == BigPicture::Lifecycle::Paused);
  auto actual = ReadState();
  if (m_owns_pause &&
      (snapshot->generation != m_pause_generation || actual.pause_revision != m_pause_revision ||
       actual.lifecycle != BigPicture::Lifecycle::Paused))
    m_owns_pause = false;
  if (m_pause && open && !m_menu_was_open && actual.lifecycle == BigPicture::Lifecycle::Running &&
      !actual.netplay)
  {
    const auto invalidation = m_pause_invalidation;
    const auto expected_revision = actual.pause_revision + 1;
    m_pause(true);
    actual = ReadState();
    m_owns_pause = actual.lifecycle == BigPicture::Lifecycle::Paused &&
                   invalidation == m_pause_invalidation &&
                   actual.pause_revision == expected_revision;
    m_pause_generation = snapshot->generation;
    m_pause_revision = actual.pause_revision;
    m_controller.Refresh();
  }
  else if (m_pause && !open && m_menu_was_open && m_owns_pause)
  {
    m_owns_pause = false;
    m_pause(false);
    m_controller.Refresh();
  }
  m_menu_was_open = open;
  // Republish only if the pause transition refreshed the controller's snapshot.
  if (auto current = m_controller.GetSnapshot(); current != snapshot)
  {
    snapshot = std::move(current);
    if (m_publish)
      m_publish(snapshot);
  }
  std::lock_guard lock(m_snapshot_mutex);
  m_snapshot = std::move(snapshot);
}

void BigPictureHost::Refresh()
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_controller.Refresh();
  Publish();
}

void BigPictureHost::InvalidateSession()
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_controller.InvalidateSession();
  Publish();
}

void BigPictureHost::FinishStopToDesktop()
{
  Q_ASSERT(QThread::currentThread() == thread());
  // A synchronous stop callback can arrive inside Execute. Leave the mode change pending
  // until the outer dispatch publishes its completed state instead of submitting reentrantly.
  m_finish_stop_to_desktop = true;
  Refresh();
}

void BigPictureHost::PrepareForBoot()
{
  Q_ASSERT(QThread::currentThread() == thread());
  const auto snapshot = m_controller.GetSnapshot();
  if (!snapshot->pending || !BigPicture::IsBootAction(snapshot->pending->action))
  {
    InvalidateSession();
    m_controller.Submit({m_controller.GetSnapshot()->generation, m_next_id.fetch_add(1),
                         BigPicture::Action::ReturnToDesktop});
    Publish();
  }
}

void BigPictureHost::CompleteCommand(std::uint64_t command_id, BigPicture::ResultStatus status,
                                     std::string message)
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_controller.Complete(command_id,
                        {{}, status, BigPicture::UnavailableReason::None, std::move(message)});
  Publish();
}

void BigPictureHost::InvalidatePresentation()
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_controller.InvalidatePresentation();
  Publish();
}
