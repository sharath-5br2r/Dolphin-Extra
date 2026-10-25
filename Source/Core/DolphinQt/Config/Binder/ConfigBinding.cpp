// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigBinding.h"

#include <QFont>
#include <QMouseEvent>
#include <QWidget>
#include <utility>

#include "Common/Assert.h"
#include "Common/ScopeGuard.h"
#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"

namespace ConfigWidget
{
ConfigBinding::ConfigBinding(QWidget* widget, Config::Location location, Config::Layer* layer)
    : QObject(widget), m_location(std::move(location)), m_layer(layer)
{
  // The filter is the hook for override handling (right-click-to-clear).
  widget->installEventFilter(this);
  connect(&ConfigChangeBroadcaster::Instance(), &ConfigChangeBroadcaster::Changed, this,
          &ConfigBinding::RefreshFromConfig);
}

ConfigBinding::~ConfigBinding() = default;

bool ConfigBinding::HasLocalValue() const
{
  return m_layer != nullptr && m_layer->Exists(m_location);
}

QWidget* ConfigBinding::GetWidget() const
{
  return qobject_cast<QWidget*>(parent());
}

void ConfigBinding::AddFontMirror(QWidget* follower)
{
  m_font_mirrors.emplace_back(follower);
  ApplyOverrideFont();
}

void ConfigBinding::RefreshFromConfig()
{
  ApplyOverrideFont();

  const bool was_updating = m_updating;
  m_updating = true;
  Common::ScopeGuard guard{[this, was_updating] { m_updating = was_updating; }};
  LoadFromConfig();
}

void ConfigBinding::SetSecondaryLocation(Config::Location location)
{
  m_secondary_location = std::move(location);
  ApplyOverrideFont();
}

void ConfigBinding::ClearLocalValue()
{
  Logic::ClearLocal(m_location, m_layer);
  if (m_secondary_location.has_value())
    Logic::ClearLocal(*m_secondary_location, m_layer);
}

void ConfigBinding::ApplyOverrideFont()
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return;

  const bool local =
      Logic::IsLocal(m_location, m_layer) ||
      (m_secondary_location.has_value() && Logic::IsLocal(*m_secondary_location, m_layer));
  Config::Layer* const fallback = Logic::GetFallbackLayer(m_layer);
  const bool shipped_game_default =
      fallback != nullptr &&
      (fallback->Exists(m_location) ||
       (m_secondary_location.has_value() && fallback->Exists(*m_secondary_location)));
  widget->setProperty("dolphinInheritedSetting", m_layer != nullptr && !local);

  QFont font = widget->font();
  font.setBold(local);
  font.setItalic(shipped_game_default);
  widget->setFont(font);

  for (const QPointer<QWidget>& mirror : m_font_mirrors)
  {
    if (mirror.isNull())
      continue;
    QFont mirror_font = mirror->font();
    mirror_font.setBold(local);
    mirror_font.setItalic(shipped_game_default);
    mirror->setFont(mirror_font);
  }
}

bool ConfigBinding::eventFilter(QObject* watched, QEvent* event)
{
  // The filter is installed on exactly one object.
  DEBUG_ASSERT(watched == parent());

  // Was ConfigControl::mousePressEvent. Right-click clears a per-game override; with no layer
  // there is nothing to clear and the widget must still receive the click so context menus work.
  if (event->type() == QEvent::MouseButtonPress && m_layer != nullptr &&
      static_cast<QMouseEvent*>(event)->button() == Qt::RightButton)
  {
    QWidget* const widget = GetWidget();
    // QWidget::event() drops mouse events for a disabled widget, so ConfigControl's override never
    // ran for one. Object event filters run before that, so the check has to be explicit here.
    if (widget != nullptr && widget->isEnabled())
    {
      ClearLocalValue();
      // Logic::ClearLocal already calls Config::OnConfigChanged(), which reaches other bound
      // widgets through Settings. Refresh directly as well: qt-tests does not construct Settings.
      RefreshFromConfig();
      return true;
    }
  }

  return QObject::eventFilter(watched, event);
}
}  // namespace ConfigWidget
