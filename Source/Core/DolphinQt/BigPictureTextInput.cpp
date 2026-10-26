// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureTextInput.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <QDialogButtonBox>
#include <QFile>
#include <QGuiApplication>
#include <QInputMethod>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <IOKit/hid/IOHIDManager.h>
#endif

namespace
{
bool HasPhysicalKeyboard()
{
#ifdef _WIN32
  // Unlike Dolphin's logical Keyboard Mouse device, raw input reflects attached hardware.
  if (GetSystemMetrics(SM_REMOTESESSION))
    return true;
  UINT count = 0;
  if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1))
    return false;
  std::vector<RAWINPUTDEVICELIST> devices(count);
  if (GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1))
    return false;
  devices.resize(count);
  return std::ranges::any_of(devices,
                             [](const auto& device) { return device.dwType == RIM_TYPEKEYBOARD; });
#elif defined(__APPLE__)
  IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
  if (!manager)
    return false;
  auto match = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
  int page = kHIDPage_GenericDesktop, usage = kHIDUsage_GD_Keyboard;
  auto page_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
  auto usage_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
  CFDictionarySetValue(match, CFSTR(kIOHIDDeviceUsagePageKey), page_value);
  CFDictionarySetValue(match, CFSTR(kIOHIDDeviceUsageKey), usage_value);
  IOHIDManagerSetDeviceMatching(manager, match);
  auto devices = IOHIDManagerCopyDevices(manager);
  const bool available = devices && CFSetGetCount(devices) != 0;
  if (devices)
    CFRelease(devices);
  CFRelease(page_value);
  CFRelease(usage_value);
  CFRelease(match);
  CFRelease(manager);
  return available;
#else
  QFile devices(QStringLiteral("/proc/bus/input/devices"));
  if (!devices.open(QIODevice::ReadOnly))
    return false;
  for (const auto& line : devices.readAll().split('\n'))
    if (line.startsWith("H: Handlers=") && line.split(' ').contains("kbd"))
      return true;
  return false;
#endif
}
}  // namespace

BigPictureTextInput::InputMethods BigPictureTextInput::GetInputMethods()
{
  const auto system_keyboard_visible = [] {
#ifdef _WIN32
    // The accessibility keyboard is independent of Qt's touch-keyboard visibility.
    if (const HWND keyboard = FindWindowW(L"OSKMainClass", nullptr);
        keyboard && IsWindowVisible(keyboard))
      return true;
    // An open IME alone is not an on-screen keyboard.
    return QGuiApplication::inputMethod()->isVisible() &&
           !QGuiApplication::inputMethod()->keyboardRectangle().isEmpty();
#else
    return QGuiApplication::inputMethod()->isVisible();
#endif
  };
  return {HasPhysicalKeyboard(),
          [system_keyboard_visible] {
            QGuiApplication::inputMethod()->show();
#ifdef _WIN32
            if (!system_keyboard_visible())
            {
              // Qt's Windows input context enables the touch panel but does not launch the
              // accessibility keyboard on a controller-only PC. Ask the system to open it.
              SHELLEXECUTEINFOW launch{};
              std::wstring keyboard(MAX_PATH, L'\0');
              const UINT length =
                  GetSystemDirectoryW(keyboard.data(), static_cast<UINT>(keyboard.size()));
              if (!length || length >= keyboard.size())
                return;
              keyboard.resize(length);
              keyboard += L"\\osk.exe";
              launch.cbSize = sizeof(launch);
              launch.fMask = SEE_MASK_FLAG_NO_UI;
              launch.lpVerb = L"open";
              launch.lpFile = keyboard.c_str();
              launch.nShow = SW_SHOWNOACTIVATE;
              ShellExecuteExW(&launch);
            }
#endif
          },
          system_keyboard_visible};
}

BigPictureTextInput::BigPictureTextInput(QWidget* parent, const QString& title,
                                         const QString& initial, InputMethods methods)
    : QDialog(parent)
{
  setWindowTitle(title);
  setWindowModality(Qt::WindowModal);
  setMinimumWidth(480);
  auto* layout = new QVBoxLayout(this);
  auto* edit = new QLineEdit(initial, this);
  edit->setObjectName(QStringLiteral("big-picture-text"));
  edit->setMaxLength(1024);
  edit->setClearButtonEnabled(true);
  layout->addWidget(edit);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  auto* clear = buttons->addButton(tr("Clear"), QDialogButtonBox::ResetRole);
  connect(clear, &QPushButton::clicked, edit, &QLineEdit::clear);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  edit->selectAll();
  QTimer::singleShot(0, this, [this, edit, methods = std::move(methods)] {
    edit->setFocus();
    if (methods.physical_keyboard)
      return;
    if (methods.show_system_keyboard)
      methods.show_system_keyboard();
    // Give the platform panel time to appear. An unavailable panel falls back to controller
    // keys; merely having an IME configured does not establish a usable system keyboard.
    QTimer::singleShot(1500, this, [this, visible = methods.system_keyboard_visible] {
      if (isVisible() && (!visible || !visible()))
        done(ControllerKeyboard);
    });
  });
}

QString BigPictureTextInput::GetText() const
{
  return findChild<QLineEdit*>()->text();
}
