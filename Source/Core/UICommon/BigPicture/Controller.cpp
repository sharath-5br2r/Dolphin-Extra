// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/BigPicture/Controller.h"

#include <utility>

namespace BigPicture
{
UnavailableReason GetAvailability(Action action, const HostState& state)
{
  if (static_cast<std::size_t>(action) >= static_cast<std::size_t>(Action::Count))
    return UnavailableReason::InvalidArgument;
  const bool active = state.lifecycle == Lifecycle::Running || state.lifecycle == Lifecycle::Paused;
  const bool transitioning =
      state.lifecycle == Lifecycle::Booting || state.lifecycle == Lifecycle::Stopping;
  switch (action)
  {
  case Action::ChangeDisc:
    if (state.netplay)
      return UnavailableReason::NetPlay;
    if (!state.can_change_disc)
      return UnavailableReason::DiscUnavailable;
    return active        ? UnavailableReason::None :
           transitioning ? UnavailableReason::Busy :
                           UnavailableReason::NoGame;
  case Action::Exit:
    if (state.lifecycle == Lifecycle::Idle)
      return UnavailableReason::None;
    [[fallthrough]];
  case Action::Stop:
    if (state.stop_requires_desktop)
      return UnavailableReason::DesktopRequired;
    return active        ? UnavailableReason::None :
           transitioning ? UnavailableReason::Busy :
                           UnavailableReason::NoGame;
  case Action::BootIPL:
  case Action::BootSystemMenu:
  case Action::Boot:
    if (state.netplay)
      return UnavailableReason::NetPlay;
    return state.lifecycle == Lifecycle::Idle ? UnavailableReason::None :
           transitioning                      ? UnavailableReason::Busy :
                                                UnavailableReason::GameActive;
  case Action::Resume:
  case Action::Reset:
  case Action::LoadStates:
  case Action::LoadState:
    if (state.netplay)
      return UnavailableReason::NetPlay;
    if ((action == Action::LoadState || action == Action::LoadStates) && state.hardcore)
      return UnavailableReason::Hardcore;
    [[fallthrough]];
  case Action::SaveState:
  case Action::SaveStates:
  case Action::Screenshot:
  case Action::RefreshStates:
    return active        ? UnavailableReason::None :
           transitioning ? UnavailableReason::Busy :
                           UnavailableReason::NoGame;
  case Action::DesktopSettings:
  case Action::EditSetting:
  case Action::SettingsMenu:
  case Action::Library:
    return transitioning ? UnavailableReason::Busy : UnavailableReason::None;
  case Action::OpenGame:
    if (state.netplay)
      return UnavailableReason::NetPlay;
    return state.lifecycle == Lifecycle::Idle ? UnavailableReason::None :
           transitioning                      ? UnavailableReason::Busy :
                                                UnavailableReason::GameActive;
  case Action::Count:
    return UnavailableReason::InvalidArgument;
  default:
    return UnavailableReason::None;
  }
}

Controller::Controller(Host& host) : m_host(host)
{
  Refresh();
}

std::shared_ptr<const Snapshot> Controller::GetSnapshot() const
{
  return m_snapshot;
}

void Controller::Publish()
{
  for (std::size_t i = 0; i < m_state.availability.size(); ++i)
  {
    const auto action = static_cast<Action>(i);
    m_state.availability[i] = GetAvailability(action, m_state.host);
    if ((action == Action::Confirm && !m_state.confirmation) ||
        (m_state.mode == Mode::Desktop &&
         (action == Action::Settings || action == Action::SaveStates ||
          action == Action::LoadStates || action == Action::Reset || action == Action::Stop)))
      m_state.availability[i] = UnavailableReason::InvalidArgument;
    if (m_state.pending ||
        (m_state.error && action != Action::Back && action != Action::Cancel &&
         action != Action::ReturnToDesktop) ||
        (m_state.confirmation && action != Action::Confirm && action != Action::Cancel &&
         action != Action::Back))
      m_state.availability[i] = UnavailableReason::Busy;
  }
  m_snapshot = std::make_shared<const Snapshot>(m_state);
}

void Controller::SetRoot()
{
  m_state.screens.clear();
  m_state.confirmation.reset();
  m_state.error.reset();
  if (m_state.mode == Mode::Desktop)
    return;
  m_state.mode = m_state.host.lifecycle == Lifecycle::Idle ? Mode::Library : Mode::InGame;
  m_state.screens.push_back(m_state.mode == Mode::Library ? Screen::Home : Screen::Pause);
}

void Controller::Finish(Result result)
{
  m_state.pending.reset();
  m_state.result = std::move(result);
  if (m_state.result->status == ResultStatus::Failed && m_state.mode != Mode::Desktop)
  {
    m_state.error = m_state.result;
    m_state.screens.push_back(Screen::Error);
  }
}

void Controller::InvalidateSession()
{
  if (m_dispatching)
    return;  // An accepted Boot owns this transition; Refresh observes its outcome.
  if (m_state.pending)
  {
    const auto command = *m_state.pending;
    if ((command.action == Action::Stop || command.action == Action::Exit) &&
        m_host.ReadState().lifecycle == Lifecycle::Idle)
      Finish({command, ResultStatus::Succeeded});
    else
      Finish({command, ResultStatus::Stale});
  }
  ++m_state.generation;
  m_state.host = m_host.ReadState();
  SetRoot();
  Publish();
}

void Controller::InvalidatePresentation()
{
  ++m_state.generation;
  Publish();
}

void Controller::Refresh()
{
  if (m_dispatching)
    return;
  const HostState previous = m_state.host;
  m_state.host = m_host.ReadState();

  // External desktop transitions must invalidate commands too. Explicit session hooks cover
  // fast stop/boot cycles whose intermediate states are not observed here.
  if ((previous.lifecycle == Lifecycle::Idle) != (m_state.host.lifecycle == Lifecycle::Idle))
  {
    ++m_state.generation;
    SetRoot();
  }

  if (m_state.pending)
  {
    const Command command = *m_state.pending;
    if (IsBootAction(command.action))
    {
      if (m_state.host.lifecycle == Lifecycle::Running ||
          m_state.host.lifecycle == Lifecycle::Paused)
      {
        Finish({command, ResultStatus::Succeeded});
        ++m_state.generation;
        m_state.mode = Mode::Desktop;
        SetRoot();
      }
      else if (m_state.host.lifecycle == Lifecycle::Idle)
      {
        SetRoot();
        Finish({command, ResultStatus::Failed});
      }
      else if (m_state.host.lifecycle == Lifecycle::Stopping)
      {
        Finish({command, ResultStatus::Stale});
      }
    }
    else if ((command.action == Action::Stop || command.action == Action::Exit) &&
             m_state.host.lifecycle == Lifecycle::Idle)
    {
      Finish({command, ResultStatus::Succeeded});
    }
  }

  Publish();
}

void Controller::Dispatch(const Command& command)
{
  if (IsBootAction(command.action))
    ++m_state.generation;
  m_state.pending = command;
  m_state.confirmation.reset();
  if (!m_state.screens.empty() && m_state.screens.back() == Screen::Confirmation)
    m_state.screens.pop_back();
  Publish();
  m_dispatching = true;
  Result result = m_host.Execute(command);
  m_dispatching = false;
  // Boot/stop acknowledgments are not completion. State I/O currently has void APIs; keep
  // their honest Dispatched status until core completion reporting is implemented in slice 10.
  if (result.status == ResultStatus::Dispatched &&
      (IsBootAction(command.action) || command.action == Action::Stop ||
       command.action == Action::Exit))
  {
    result.status = ResultStatus::Pending;
    m_state.result = result;
  }
  else
  {
    Finish(std::move(result));
    if (m_state.result->status == ResultStatus::Succeeded)
    {
      const auto push = [&](Screen screen) {
        if (m_state.screens.empty() || m_state.screens.back() != screen)
          m_state.screens.push_back(screen);
      };
      if (command.action == Action::Library)
      {
        switch (command.library_operation)
        {
        case LibraryOperation::Options:
          push(Screen::LibraryOptions);
          break;
        case LibraryOperation::Search:
        case LibraryOperation::EditPath:
          push(Screen::TextInput);
          break;
        case LibraryOperation::AddFolder:
        case LibraryOperation::OpenFile:
        case LibraryOperation::OpenDisc:
          push(Screen::FilePicker);
          break;
        case LibraryOperation::ChooseFolder:
        case LibraryOperation::TextDone:
          if (m_state.screens.size() > 1)
            m_state.screens.pop_back();
          break;
        default:
          break;
        }
      }
      if (command.action == Action::Settings)
        push(Screen::Settings);
    }
    if (command.action == Action::ChangeDisc && !command.path.empty() &&
        (m_state.result->status == ResultStatus::Succeeded ||
         m_state.result->status == ResultStatus::Dispatched) &&
        !m_state.screens.empty() && m_state.screens.back() == Screen::FilePicker)
      m_state.screens.pop_back();
    if (command.action == Action::Resume && m_state.result->status == ResultStatus::Succeeded)
    {
      m_state.mode = Mode::Desktop;
      SetRoot();
    }
  }
  Refresh();
}

void Controller::Submit(const Command& command)
{
  Refresh();
  if (command.generation != m_state.generation || command.id == 0 || command.id <= m_last_id)
  {
    m_state.result = Result{command, ResultStatus::Stale};
    Publish();
    return;
  }
  m_last_id = command.id;
  if (m_dispatching || m_state.pending)
  {
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::Busy};
    Publish();
    return;
  }
  if (m_state.error && command.action != Action::Back && command.action != Action::Cancel &&
      command.action != Action::ReturnToDesktop)
  {
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::Busy};
    Publish();
    return;
  }
  const auto reason = GetAvailability(command.action, m_state.host);
  if (reason != UnavailableReason::None)
  {
    m_state.result = Result{command, ResultStatus::Unavailable, reason};
    Publish();
    return;
  }
  if ((command.action == Action::Boot && command.path.empty()) ||
      ((command.action == Action::SaveState || command.action == Action::LoadState) &&
       (command.slot < 1 || command.slot > 10)))
  {
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
    Publish();
    return;
  }

  if (command.action == Action::EditSetting &&
      (!command.setting || m_state.mode == Mode::Desktop || m_state.screens.empty() ||
       m_state.screens.back() != Screen::Settings))
  {
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
    Publish();
    return;
  }

  if (command.action == Action::SettingsMenu &&
      (!command.settings_menu || m_state.mode == Mode::Desktop || m_state.screens.empty() ||
       m_state.screens.back() != Screen::Settings))
  {
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
    Publish();
    return;
  }

  if (command.action == Action::Library && m_state.mode == Mode::Desktop)
  {
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
    Publish();
    return;
  }
  if (command.action == Action::Library)
  {
    const auto screen = m_state.screens.empty() ? Screen::Library : m_state.screens.back();
    const auto operation = command.library_operation;
    bool valid = operation > LibraryOperation::None && operation < LibraryOperation::Count;
    if (operation == LibraryOperation::TextKey || operation == LibraryOperation::TextErase ||
        operation == LibraryOperation::TextClear || operation == LibraryOperation::TextReplace ||
        operation == LibraryOperation::ControllerKeyboard ||
        operation == LibraryOperation::TextDone)
      valid = valid && screen == Screen::TextInput;
    else if (operation == LibraryOperation::EnterDirectory ||
             operation == LibraryOperation::ChooseFolder ||
             operation == LibraryOperation::EditPath || operation == LibraryOperation::Recursive ||
             operation == LibraryOperation::BrowseDiscs)
      valid = valid && screen == Screen::FilePicker;
    else if (operation == LibraryOperation::OpenDisc)
      valid = valid && screen == Screen::Pause &&
              GetAvailability(Action::ChangeDisc, m_state.host) == UnavailableReason::None;
    else if (operation == LibraryOperation::Cover && m_state.mode == Mode::InGame)
      valid = valid && screen == Screen::Pause && m_state.host.library &&
              command.path == m_state.host.game_path && !command.path.empty();
    else
      valid = valid && m_state.mode == Mode::Library &&
              (screen == Screen::Library || screen == Screen::LibraryOptions ||
               (screen == Screen::StartGame && operation == LibraryOperation::OpenFile));
    if (!valid)
    {
      m_state.result =
          Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
      Publish();
      return;
    }
  }
  if ((command.action == Action::Boot || command.action == Action::ChangeDisc) &&
      command.library_revision != 0 &&
      (!m_state.host.library || m_state.host.library->picker_loading ||
       command.library_revision != m_state.host.library->picker_revision))
  {
    m_state.result = Result{command, ResultStatus::Stale};
    Publish();
    return;
  }

  // A confirmation is modal and defaults to cancel: only Confirm can dispatch its stored
  // action; all other operations remain blocked until Back/Cancel dismisses it.
  if (m_state.confirmation)
  {
    auto confirmed = *m_state.confirmation;
    if (command.action == Action::Confirm)
    {
      if (command.confirmation_id != confirmed.id)
      {
        m_state.result = Result{command, ResultStatus::Stale};
        Publish();
        return;
      }
      const auto current_reason = GetAvailability(confirmed.action, m_state.host);
      if (current_reason == UnavailableReason::None)
      {
        confirmed.confirmation_id = confirmed.id;
        Dispatch(confirmed);
      }
      else
      {
        m_state.confirmation.reset();
        m_state.screens.pop_back();
        Finish({confirmed, ResultStatus::Unavailable, current_reason});
        Publish();
      }
    }
    else if (command.action == Action::Back || command.action == Action::Cancel)
    {
      m_state.confirmation.reset();
      m_state.screens.pop_back();
      Finish({confirmed, ResultStatus::Cancelled});
      Publish();
    }
    else
    {
      m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::Busy};
      Publish();
    }
    return;
  }

  switch (command.action)
  {
  case Action::OpenMenu:
    if (m_state.mode == Mode::Desktop)
    {
      m_state.mode = Mode::Library;
      SetRoot();
    }
    break;
  case Action::Back:
  case Action::Cancel:
    if (m_state.mode == Mode::Library && m_state.screens.size() == 1 &&
        m_state.screens.back() == Screen::Home)
    {
      m_state.screens.push_back(Screen::ExitMenu);
    }
    else if (m_state.screens.size() > 1)
    {
      if (m_state.screens.back() == Screen::Error)
        m_state.error.reset();
      m_state.screens.pop_back();
    }
    else
    {
      m_state.mode = Mode::Desktop;
      SetRoot();
    }
    break;
  case Action::ShowLibrary:
  case Action::StartGame:
  case Action::ExitMenu:
  case Action::About:
    if (m_state.mode != Mode::Library || m_state.screens.empty() ||
        (command.action != Action::ExitMenu && m_state.screens.back() != Screen::Home))
    {
      m_state.result =
          Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
      Publish();
      return;
    }
    if (m_state.screens.back() != Screen::ExitMenu)
      m_state.screens.push_back(command.action == Action::About       ? Screen::About :
                                command.action == Action::ShowLibrary ? Screen::Library :
                                command.action == Action::StartGame   ? Screen::StartGame :
                                                                        Screen::ExitMenu);
    break;
  case Action::ReturnToDesktop:
    m_state.mode = Mode::Desktop;
    SetRoot();
    break;
  case Action::Settings:
    if (m_state.host.settings_menu && m_state.mode != Mode::Desktop)
    {
      Dispatch(command);
      return;
    }
    [[fallthrough]];
  case Action::SaveStates:
  case Action::LoadStates:
    if (m_state.mode == Mode::Desktop)
    {
      m_state.result =
          Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
      Publish();
      return;
    }
    m_state.screens.push_back(command.action == Action::Settings   ? Screen::Settings :
                              command.action == Action::SaveStates ? Screen::SaveStates :
                                                                     Screen::LoadStates);
    break;
  case Action::Stop:
  case Action::Reset:
  case Action::Exit:
    if (m_state.mode == Mode::Desktop)
    {
      m_state.result =
          Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
      Publish();
      return;
    }
    m_state.confirmation = command;
    m_state.screens.push_back(Screen::Confirmation);
    break;
  case Action::SaveState:
    if (m_state.host.state_slots_ready && m_state.host.state_slots[command.slot - 1].occupied)
    {
      if (m_state.mode == Mode::Desktop)
      {
        m_state.result =
            Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
        Publish();
        return;
      }
      m_state.confirmation = command;
      m_state.screens.push_back(Screen::Confirmation);
      break;
    }
    Dispatch(command);
    return;
  case Action::Confirm:
    m_state.result = Result{command, ResultStatus::Unavailable, UnavailableReason::InvalidArgument};
    Publish();
    return;
  default:
    Dispatch(command);
    return;
  }
  m_state.result =
      Result{command, m_state.confirmation ? ResultStatus::Pending : ResultStatus::Succeeded};
  Publish();
}
}  // namespace BigPicture
