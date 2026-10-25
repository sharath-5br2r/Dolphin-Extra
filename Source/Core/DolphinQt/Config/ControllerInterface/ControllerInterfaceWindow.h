// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
class DualShockUDPClientWidget;
#endif

namespace Ui
{
class ControllerInterfaceWindow;
}

class ControllerInterfaceWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit ControllerInterfaceWindow(QWidget* parent);
  ~ControllerInterfaceWindow() override;

private:
  std::unique_ptr<Ui::ControllerInterfaceWindow> m_ui;

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
  DualShockUDPClientWidget* m_dsuclient_widget;
#endif
};
