// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include <QObject>

#include "UICommon/BigPicture/Controller.h"

// The host owns the controller on Qt's thread. Render/input owners may only read a published
// snapshot or queue a value command; neither operation touches a QWidget or live core object.
class BigPictureHost final : public QObject, private BigPicture::Host
{
public:
  using ReadStateCallback = std::function<BigPicture::HostState()>;
  using ExecuteCallback = std::function<BigPicture::Result(const BigPicture::Command&)>;
  using PauseCallback = std::function<void(bool)>;
  using PublishCallback = std::function<void(std::shared_ptr<const BigPicture::Snapshot>)>;

  BigPictureHost(QObject* parent, ReadStateCallback read_state, ExecuteCallback execute,
                 PauseCallback pause = {}, PublishCallback publish = {});
  ~BigPictureHost() override;
  void Submit(BigPicture::Command command, std::optional<std::uint64_t> input_epoch = std::nullopt);
  void RequestBack();
  void RequestAction(std::uint64_t generation, BigPicture::Action action,
                     std::optional<std::uint64_t> input_epoch = std::nullopt, int slot = 0,
                     std::uint64_t confirmation_id = 0, std::string path = {});
  void InvalidatePauseOwnership();
  std::shared_ptr<const BigPicture::Snapshot> GetSnapshot() const;
  void Refresh();
  void InvalidateSession();
  // Complete a native desktop handoff after any pending stop has settled.
  void FinishStopToDesktop();
  void PrepareForBoot();
  void InvalidatePresentation();

private:
  BigPicture::HostState ReadState() const override;
  BigPicture::Result Execute(const BigPicture::Command& command) override;
  void Publish();

  ReadStateCallback m_read_state;
  ExecuteCallback m_execute;
  PauseCallback m_pause;
  PublishCallback m_publish;
  BigPicture::Controller m_controller;
  mutable std::mutex m_snapshot_mutex;
  std::shared_ptr<const BigPicture::Snapshot> m_snapshot;
  std::atomic<std::uint64_t> m_next_id{1};
  bool m_menu_was_open = false;
  bool m_owns_pause = false;
  bool m_finish_stop_to_desktop = false;
  std::uint64_t m_pause_generation = 0;
  std::uint64_t m_pause_revision = 0;
  std::uint64_t m_pause_invalidation = 0;
};
