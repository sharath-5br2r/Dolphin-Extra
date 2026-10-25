// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <QObject>
#include <QPointer>

#include "Common/Config/ConfigInfo.h"
#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"

class QEvent;
class QWidget;

namespace Config
{
class Layer;
}

namespace ConfigWidget
{
// Attached as a child of the widget it binds, so it lives exactly as long as the widget and call
// sites do no ownership bookkeeping. Replaces ConfigControl<Derived>, whose config location was a
// constructor argument and therefore could not be produced by uic.
//
// This is the only class in the binder with Q_OBJECT: moc cannot process templates, so typed
// state lives in non-Q_OBJECT subclasses that add no signals or slots.
class ConfigBinding : public QObject
{
  Q_OBJECT

public:
  ConfigBinding(QWidget* widget, Config::Location location, Config::Layer* layer);
  ~ConfigBinding() override;

  const Config::Location& GetLocation() const { return m_location; }
  // The optional secondary location. A binding with no secondary returns nullopt. A caller sweeping
  // bindings generically must consult both GetLocation() and GetSecondaryLocation().
  const std::optional<Config::Location>& GetSecondaryLocation() const
  {
    return m_secondary_location;
  }
  Config::Layer* GetLayer() const { return m_layer; }
  bool HasLocalValue() const;

  // `follower`'s font is set alongside this binding's whenever the override state is re-applied.
  // Used by MirrorFont so a label goes bold beside the control it names.
  void AddFontMirror(QWidget* follower);

protected:
  QWidget* GetWidget() const;

  // True while LoadFromConfig() is running. Subclasses must not write config in that window:
  // refresh sets the widget, the widget emits its value-changed signal, and a naive handler would
  // save straight back, clobbering a per-game layer or recursing.
  bool IsUpdating() const { return m_updating; }

  // Re-reads config into the widget and re-applies the overridden-value font.
  void RefreshFromConfig();
  void ClearLocalValue();

  // A second location that also counts for the bold font and is also cleared on right-click.
  // Only ComplexBinding uses it; two is the most any control drives.
  void SetSecondaryLocation(Config::Location location);

  virtual void LoadFromConfig() = 0;

  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void ApplyOverrideFont();

  const Config::Location m_location;
  Config::Layer* m_layer;  // Caller must keep the layer alive at least as long as the widget.
  bool m_updating = false;
  std::optional<Config::Location> m_secondary_location;

  // QPointer so a follower outliving its control is safe rather than a dangling read.
  std::vector<QPointer<QWidget>> m_font_mirrors;
};

// Shared state for a binding whose widget holds one value of type T. A template, so it carries no
// Q_OBJECT and declares no signals or slots; concrete per-widget subclasses connect the widget's
// value-changed signal and implement LoadFromConfig().
template <typename Widget, typename T>
class ValueBinding : public ConfigBinding
{
public:
  ValueBinding(Widget* widget, const Config::Info<T>& setting, Config::Layer* layer)
      : ConfigBinding(widget, setting.GetLocation(), layer), m_setting(setting)
  {
  }

protected:
  Widget* GetTypedWidget() const { return static_cast<Widget*>(GetWidget()); }

  T Read() const { return Logic::ReadValue(m_setting, GetLayer()); }
  T ReadInherited() const { return Logic::ReadInheritedValue(m_setting, GetLayer()); }

  void Save(const T& value)
  {
    if (IsUpdating())
      return;
    Logic::WriteValue(m_setting, GetLocation(), GetLayer(), value);
    RefreshFromConfig();
  }

  void Clear()
  {
    if (IsUpdating())
      return;
    ClearLocalValue();
    RefreshFromConfig();
  }

private:
  // Config::Info<T> has a deleted assignment operator and no move constructor, so it can only be
  // initialised in the member-initialiser list above.
  const Config::Info<T> m_setting;
};
}  // namespace ConfigWidget
