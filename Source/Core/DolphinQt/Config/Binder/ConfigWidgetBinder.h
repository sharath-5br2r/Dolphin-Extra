// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QPoint>
#include <QString>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Config/ConfigInfo.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigBinding.h"
#include "DolphinQt/Config/Binder/ConfigSliderMapping.h"

class QLabel;
class QLineEdit;
class QRadioButton;
class QSlider;
class QSpinBox;
class QWidget;

namespace Config
{
class Layer;
}

// Binds stock Qt widgets to typed config settings after construction, so layouts can be authored
// in Qt Designer .ui files. A null `layer` binds global config; a non-null `layer` binds a
// per-game layer. Config::Info<T> already carries the key, type and default, so no key or default
// argument is needed.
namespace ConfigWidget
{
class ConfigBinding;

void Bind(QCheckBox* widget, const Config::Info<bool>& setting, Config::Layer* layer = nullptr,
          bool reverse = false);

// Binds the combo box's current index. For an index-to-value mapping use BindMapped.
void Bind(QComboBox* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
// The spin box's and slider's minimum and maximum come from the .ui file and are left alone.
void Bind(QSpinBox* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
void Bind(QSlider* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
// Checked exactly when the setting equals `value`; writes `value` when it becomes checked.
void Bind(QRadioButton* widget, const Config::Info<int>& setting, int value,
          Config::Layer* layer = nullptr);
// Saves on editingFinished, so a half-typed path is never written to the config file.
void Bind(QLineEdit* widget, const Config::Info<std::string>& setting,
          Config::Layer* layer = nullptr);

void Bind(QComboBox* widget, const Config::Info<u32>& setting, Config::Layer* layer = nullptr);

// Two forms, matching ConfigStringChoice: the option text is the stored data, or display text and
// stored data are given separately.
void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::string> options, Config::Layer* layer = nullptr);
void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::pair<QString, QString>> options,
                      Config::Layer* layer = nullptr);

// Slider position i means tick_values[i]. The .ui must author minimum 0, maximum one less than the
// tick count, pageStep 1 and a tick position; the binding asserts the range and never changes it.
// Disables the slider while the config value matches no tick. The binding owns the enabled
// property.
void BindMapped(QSlider* widget, const Config::Info<int>& setting, std::span<const int> tick_values,
                Config::Layer* layer = nullptr);
// Stored value is the slider position times `scale`.
void BindScaled(QSlider* widget, const Config::Info<u32>& setting, u32 scale,
                Config::Layer* layer = nullptr);

// The .ui file authors minimum = 0 and maximum = MaximumPosition() for the float range it will be
// bound to. BindFloat asserts they agree rather than imposing them: Designer owns geometry, and a
// binding asserts agreement with it. The handle exists because call sites read the mapped value
// back to drive a value label.
struct FloatSliderHandle
{
  QSlider* slider = nullptr;
  FloatSliderRange range{};

  float Value() const;
};

FloatSliderHandle BindFloat(QSlider* widget, const Config::Info<float>& setting, float minimum,
                            float maximum, float step, Config::Layer* layer = nullptr);

// `format` is a printf-style format taking one double, e.g. "%.0f%%". Sets the label immediately
// and on every subsequent slider move.
void MirrorFloatValue(QLabel* label, FloatSliderHandle handle, const QString& format);

// Displays the config value if it is non-empty, otherwise File::GetUserPath(dir_index): an empty
// config value means "use the current user path". Saving sets both.
void BindUserPath(QLineEdit* widget, unsigned int dir_index,
                  const Config::Info<std::string>& setting, Config::Layer* layer = nullptr);

// Replaces the modal warning shown when a user path field is emptied. Passing an empty function
// restores the default. Exists so qt-tests can assert the rejection without a modal exec() that
// would block forever.
using PathWarningHandler = std::function<void(QWidget* parent, const QString& message)>;
void SetPathWarningHandlerForTesting(PathWarningHandler handler);

// The binding attached to `widget`, or nullptr if it has none.
ConfigBinding* FindBinding(QWidget* widget);

// Copies a bound control's font so the label goes bold beside an overridden setting. `control`
// must already be bound.
void MirrorFont(QLabel* label, QWidget* control);

// Balloon tooltip text. For a QAbstractButton an empty `title` falls back to the button's own
// label, matching the historic checkbox and radio-button behavior. Any other widget keeps the
// empty title: for a spin box or a line edit, `text` is the current value.
void SetDescription(QWidget* widget, QString title, QString description);
QString ToolTipTitle(const QWidget* widget);
QString ToolTipDescription(const QWidget* widget);
// Returns the effective checked value for a layered tri-state checkbox. QCheckBox::isChecked()
// treats PartiallyChecked as true, while the inherited value may be false.
bool EffectiveChecked(const QCheckBox* widget);
bool IsInherited(const QWidget* widget);

template <typename Receiver, typename Slot>
void ConnectCheckStateChanged(QCheckBox* widget, Receiver* receiver, Slot&& slot)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  QObject::connect(widget, &QCheckBox::checkStateChanged, receiver, std::forward<Slot>(slot));
#else
  QObject::connect(widget, &QCheckBox::stateChanged, receiver, std::forward<Slot>(slot));
#endif
}

// The balloon's arrow tip, in the widget's own coordinates.
QPoint ToolTipAnchor(const QWidget* widget);

// Two settings driving one combo box: today's ConfigComplexChoice. Returned by BindComplex rather
// than bound in one call, because the option list is pairs of *values*, which cannot be authored in
// Designer, so callers add them afterwards.
class ComplexBinding final : public ConfigBinding
{
public:
  using InfoVariant = std::variant<Config::Info<u32>, Config::Info<int>, Config::Info<bool>>;
  using OptionVariant = std::variant<Config::DefaultState, u32, int, bool>;

  ComplexBinding(QComboBox* box, const InfoVariant& setting1, const InfoVariant& setting2,
                 Config::Layer* layer);

  void Add(const QString& name, OptionVariant option1, OptionVariant option2);
  // Index selected when no option matches the current values. -1, meaning "select nothing", until
  // set.
  void SetDefault(int index);
  void Reset();

  std::pair<Config::Location, Config::Location> GetLocations() const;

private:
  void LoadFromConfig() override;
  void OnIndexChanged(int index);

  const InfoVariant m_setting1;
  const InfoVariant m_setting2;
  std::vector<std::pair<OptionVariant, OptionVariant>> m_options;
  int m_default_index = -1;
};

// Returned binding is a QObject child of the widget and lives as long as the widget does.
ComplexBinding* BindComplex(QComboBox* widget, const ComplexBinding::InfoVariant& setting1,
                            const ComplexBinding::InfoVariant& setting2,
                            Config::Layer* layer = nullptr);

namespace detail
{
void PrepareLayeredCombo(QComboBox* box, Config::Layer* layer);
void SetInheritedComboText(QComboBox* box, int inherited_item_index);

// value <-> combo-index mapping, shared by both BindMapped overloads. No Q_OBJECT: it is a
// template.
template <typename T>
class MappedComboBinding final : public ValueBinding<QComboBox, T>
{
public:
  MappedComboBinding(QComboBox* box, const Config::Info<T>& setting, std::vector<T> values,
                     Config::Layer* layer)
      : ValueBinding<QComboBox, T>(box, setting, layer), m_values(std::move(values))
  {
    PrepareLayeredCombo(box, layer);
    QObject::connect(box, &QComboBox::currentIndexChanged, this,
                     &MappedComboBinding::OnIndexChanged);
    this->RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const box = this->GetTypedWidget();
    const bool layered = this->GetLayer() != nullptr;
    const T inherited = layered ? this->ReadInherited() : this->Read();
    const auto inherited_it = std::find(m_values.begin(), m_values.end(), inherited);
    const int inherited_index =
        inherited_it == m_values.end() ?
            -1 :
            static_cast<int>(std::distance(m_values.begin(), inherited_it)) + (layered ? 1 : 0);
    if (layered)
    {
      SetInheritedComboText(box, inherited_index);
      if (!this->HasLocalValue())
      {
        box->setCurrentIndex(0);
        return;
      }
    }

    const T value = this->Read();
    const auto it = std::find(m_values.begin(), m_values.end(), value);
    // -1 when nothing matches: better an empty combo than a wrong selection that the user then
    // saves by touching something else.
    const int index = it == m_values.end() ?
                          -1 :
                          static_cast<int>(std::distance(m_values.begin(), it)) + (layered ? 1 : 0);
    box->setCurrentIndex(index);
  }

  void OnIndexChanged(int index)
  {
    const bool layered = this->GetLayer() != nullptr;
    if (layered && index == 0)
    {
      this->Clear();
      return;
    }

    const int value_index = index - (layered ? 1 : 0);
    if (value_index < 0 || static_cast<size_t>(value_index) >= m_values.size())
      return;
    this->Save(m_values[static_cast<size_t>(value_index)]);
  }

  const std::vector<T> m_values;
};

// Defined in the .cpp so SettingKind plumbing stays there. Call after the combo is populated.
void RecordMappedCombo(const Config::Location& location, Config::Layer* layer,
                       const QComboBox* widget);
}  // namespace detail

// The items are authored in the .ui file; `values` pairs with them by index, so the display text
// stays in the uic translation path.
template <typename T>
void BindMapped(QComboBox* widget, const Config::Info<T>& setting, std::span<const T> values,
                Config::Layer* layer = nullptr)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  DEBUG_ASSERT(!values.empty());
  DEBUG_ASSERT(widget->count() == static_cast<int>(values.size()));
  new detail::MappedComboBinding<T>{widget, setting, std::vector<T>(values.begin(), values.end()),
                                    layer};
  detail::RecordMappedCombo(setting.GetLocation(), layer, widget);
}

// For option sets computed at runtime, which cannot be authored in Designer. Populates the combo.
template <typename T>
void BindMapped(QComboBox* widget, const Config::Info<T>& setting,
                std::span<const std::pair<QString, T>> options, Config::Layer* layer = nullptr)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  DEBUG_ASSERT(!options.empty());
  DEBUG_ASSERT(widget->count() == 0);
  std::vector<T> values;
  values.reserve(options.size());
  for (const auto& [text, value] : options)
  {
    widget->addItem(text);
    values.push_back(value);
  }
  new detail::MappedComboBinding<T>{widget, setting, std::move(values), layer};
  detail::RecordMappedCombo(setting.GetLocation(), layer, widget);
}
}  // namespace ConfigWidget
