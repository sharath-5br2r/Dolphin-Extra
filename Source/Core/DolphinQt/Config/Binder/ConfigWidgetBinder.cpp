// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

#include <algorithm>
#include <vector>

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOption>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/FileUtil.h"
#include "DolphinQt/Config/Binder/BalloonTipFilter.h"
#include "DolphinQt/Config/Binder/ConfigBinding.h"
#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"
#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"
#include "DolphinQt/Config/SettingsHelp.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

namespace ConfigWidget
{
namespace
{
constexpr char INHERITED_CHECKED_PROPERTY[] = "dolphinInheritedChecked";
constexpr char INHERIT_COMBO_PROPERTY[] = "dolphinHasInheritedItem";

class CheckBoxBinding final : public ValueBinding<QCheckBox, bool>
{
public:
  CheckBoxBinding(QCheckBox* box, const Config::Info<bool>& setting, Config::Layer* layer,
                  bool reverse)
      : ValueBinding(box, setting, layer), m_reverse(reverse)
  {
    if (layer != nullptr)
      box->setTristate(true);
    ConnectCheckStateChanged(box, this, [this] { OnStateChanged(GetTypedWidget()->checkState()); });
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const box = GetTypedWidget();
    const bool checked = Read() ^ m_reverse;
    box->setProperty(INHERITED_CHECKED_PROPERTY, checked);
    if (GetLayer() != nullptr && !HasLocalValue())
      box->setCheckState(Qt::PartiallyChecked);
    else
      box->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
  }

  void OnStateChanged(Qt::CheckState state)
  {
    if (state == Qt::PartiallyChecked)
    {
      Clear();
      return;
    }
    Save((state == Qt::Checked) ^ m_reverse);
  }

  const bool m_reverse;
};

class ComboBoxBinding final : public ValueBinding<QComboBox, int>
{
public:
  ComboBoxBinding(QComboBox* box, const Config::Info<int>& setting, Config::Layer* layer)
      : ValueBinding(box, setting, layer)
  {
    detail::PrepareLayeredCombo(box, layer);
    connect(box, &QComboBox::currentIndexChanged, this, &ComboBoxBinding::OnIndexChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const box = GetTypedWidget();
    if (GetLayer() != nullptr)
    {
      detail::SetInheritedComboText(box, ReadInherited() + 1);
      box->setCurrentIndex(HasLocalValue() ? Read() + 1 : 0);
      return;
    }
    box->setCurrentIndex(Read());
  }

  void OnIndexChanged(int index)
  {
    if (GetLayer() != nullptr)
    {
      if (index == 0)
      {
        Clear();
        return;
      }
      --index;
    }
    Save(index);
  }
};

class SpinBoxBinding final : public ValueBinding<QSpinBox, int>
{
public:
  SpinBoxBinding(QSpinBox* spin, const Config::Info<int>& setting, Config::Layer* layer)
      : ValueBinding(spin, setting, layer), m_original_prefix(spin->prefix())
  {
    connect(spin, &QSpinBox::valueChanged, this, &SpinBoxBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const spin = GetTypedWidget();
    spin->setPrefix(GetLayer() != nullptr && !HasLocalValue() ?
                        QObject::tr("Default: ") + m_original_prefix :
                        m_original_prefix);
    spin->setValue(Read());
  }
  void OnValueChanged(int value)
  {
    if (IsUpdating())
      return;
    GetTypedWidget()->setPrefix(m_original_prefix);
    Save(value);
  }

  const QString m_original_prefix;
};

class SliderBinding final : public ValueBinding<QSlider, int>
{
public:
  SliderBinding(QSlider* slider, const Config::Info<int>& setting, Config::Layer* layer)
      : ValueBinding(slider, setting, layer)
  {
    connect(slider, &QSlider::valueChanged, this, &SliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setValue(Read()); }
  void OnValueChanged(int value) { Save(value); }
};

class RadioButtonBinding final : public ValueBinding<QRadioButton, int>
{
public:
  RadioButtonBinding(QRadioButton* button, const Config::Info<int>& setting, int value,
                     Config::Layer* layer)
      : ValueBinding(button, setting, layer), m_value(value)
  {
    connect(button, &QRadioButton::toggled, this, &RadioButtonBinding::OnToggled);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setChecked(Read() == m_value); }

  void OnToggled(bool checked)
  {
    // Autoexclusive grouping unchecks the previous button as part of checking the new one. Only
    // the button that won may write, or the two saves race.
    if (checked)
      Save(m_value);
  }

  const int m_value;
};

class LineEditBinding final : public ValueBinding<QLineEdit, std::string>
{
public:
  LineEditBinding(QLineEdit* edit, const Config::Info<std::string>& setting, Config::Layer* layer)
      : ValueBinding(edit, setting, layer)
  {
    connect(edit, &QLineEdit::editingFinished, this, &LineEditBinding::OnEditingFinished);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setText(QString::fromStdString(Read())); }

  void OnEditingFinished()
  {
    // Read-only fields are displays with programmatic write paths. They remain focusable, so Qt
    // still emits editingFinished when they lose focus or their window closes. Saving here would
    // turn a displayed inherited value into a per-game override.
    if (GetTypedWidget()->isReadOnly())
      return;
    Save(GetTypedWidget()->text().toStdString());
  }
};

class ComboBoxU32Binding final : public ValueBinding<QComboBox, u32>
{
public:
  ComboBoxU32Binding(QComboBox* box, const Config::Info<u32>& setting, Config::Layer* layer)
      : ValueBinding(box, setting, layer)
  {
    detail::PrepareLayeredCombo(box, layer);
    connect(box, &QComboBox::currentIndexChanged, this, &ComboBoxU32Binding::OnIndexChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const box = GetTypedWidget();
    if (GetLayer() != nullptr)
    {
      detail::SetInheritedComboText(box, static_cast<int>(ReadInherited()) + 1);
      box->setCurrentIndex(HasLocalValue() ? static_cast<int>(Read()) + 1 : 0);
      return;
    }
    box->setCurrentIndex(static_cast<int>(Read()));
  }
  void OnIndexChanged(int index)
  {
    if (GetLayer() != nullptr)
    {
      if (index == 0)
      {
        Clear();
        return;
      }
      --index;
    }
    if (index >= 0)
      Save(static_cast<u32>(index));
  }
};

class StringChoiceBinding final : public ValueBinding<QComboBox, std::string>
{
public:
  StringChoiceBinding(QComboBox* box, const Config::Info<std::string>& setting,
                      Config::Layer* layer)
      : ValueBinding(box, setting, layer)
  {
    detail::PrepareLayeredCombo(box, layer);
    connect(box, &QComboBox::currentIndexChanged, this, &StringChoiceBinding::OnIndexChanged);
    RefreshFromConfig();
  }

private:
  // findData returns -1 when nothing matches, which clears the selection - the same outcome as
  // before, and better than showing a value that is not the one in effect.
  void LoadFromConfig() override
  {
    auto* const box = GetTypedWidget();
    if (GetLayer() != nullptr)
    {
      detail::SetInheritedComboText(box, box->findData(QString::fromStdString(ReadInherited())));
      if (!HasLocalValue())
      {
        box->setCurrentIndex(0);
        return;
      }
    }
    box->setCurrentIndex(box->findData(QString::fromStdString(Read())));
  }

  void OnIndexChanged(int index)
  {
    if (GetLayer() != nullptr && index == 0)
    {
      Clear();
      return;
    }
    if (index >= 0)
      Save(GetTypedWidget()->itemData(index).toString().toStdString());
  }
};

class TickSliderBinding final : public ValueBinding<QSlider, int>
{
public:
  TickSliderBinding(QSlider* slider, const Config::Info<int>& setting, std::vector<int> ticks,
                    Config::Layer* layer)
      : ValueBinding(slider, setting, layer), m_ticks(std::move(ticks))
  {
    ASSERT(!m_ticks.empty());
    connect(slider, &QSlider::valueChanged, this, &TickSliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const slider = GetTypedWidget();
    const int value = Read();
    const auto it = std::find(m_ticks.begin(), m_ticks.end(), value);
    if (it == m_ticks.end())
    {
      // The config holds a value this slider cannot represent. Disable rather than snap, so the
      // user is not shown a value that is not the one in effect.
      slider->setEnabled(false);
      return;
    }
    slider->setEnabled(true);
    slider->setValue(static_cast<int>(std::distance(m_ticks.begin(), it)));
  }

  void OnValueChanged(int position)
  {
    if (position >= 0 && static_cast<size_t>(position) < m_ticks.size())
      Save(m_ticks[static_cast<size_t>(position)]);
  }

  const std::vector<int> m_ticks;
};

class ScaledSliderBinding final : public ValueBinding<QSlider, u32>
{
public:
  ScaledSliderBinding(QSlider* slider, const Config::Info<u32>& setting, u32 scale,
                      Config::Layer* layer)
      : ValueBinding(slider, setting, layer), m_scale(scale)
  {
    ASSERT(scale != 0);
    connect(slider, &QSlider::valueChanged, this, &ScaledSliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    // Qt clamps to the slider's range, so a stored value outside that range shows a clamped
    // position while the stored value remains in effect — inherited from ConfigSliderU32.
    GetTypedWidget()->setValue(static_cast<int>(Read() / m_scale));
  }
  void OnValueChanged(int position)
  {
    if (position < 0)
      return;
    Save(static_cast<u32>(position) * m_scale);
  }

  const u32 m_scale;
};

// GUI-thread-only, unguarded. Test fixtures reset to {} in TearDown to restore the default modal.
PathWarningHandler s_path_warning_handler;

void WarnAboutPath(QWidget* parent, const QString& message)
{
  if (s_path_warning_handler)
  {
    s_path_warning_handler(parent, message);
    return;
  }
  ModalMessageBox::warning(parent, QObject::tr("Empty Value"), message);
}

class FloatSliderBinding final : public ValueBinding<QSlider, float>
{
public:
  FloatSliderBinding(QSlider* slider, const Config::Info<float>& setting, FloatSliderRange range,
                     Config::Layer* layer)
      : ValueBinding(slider, setting, layer), m_range(range)
  {
    connect(slider, &QSlider::valueChanged, this, &FloatSliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setValue(m_range.PositionForValue(Read())); }
  void OnValueChanged(int position) { Save(m_range.ValueForPosition(position)); }

  const FloatSliderRange m_range;
};

class UserPathBinding final : public ValueBinding<QLineEdit, std::string>
{
public:
  UserPathBinding(QLineEdit* edit, unsigned int dir_index, const Config::Info<std::string>& setting,
                  Config::Layer* layer)
      : ValueBinding(edit, setting, layer), m_dir_index(dir_index)
  {
    connect(edit, &QLineEdit::editingFinished, this, &UserPathBinding::OnEditingFinished);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    // An empty config value means "use the current user path"; config only seeds UserPath at
    // startup, so the effective path is what the field must show.
    const std::string config_value = Read();
    const std::string effective =
        config_value.empty() ? File::GetUserPath(m_dir_index) : config_value;
    GetTypedWidget()->setText(QString::fromStdString(effective));
  }

  void OnEditingFinished()
  {
    if (IsUpdating())
      return;
    auto* const edit = GetTypedWidget();
    const QString trimmed = edit->text().trimmed();
    if (trimmed.isEmpty())
    {
      WarnAboutPath(edit, QObject::tr("This field cannot be left empty. Please enter a value."));
      RefreshFromConfig();
      return;
    }

    const std::string value = trimmed.toStdString();
    File::SetUserPath(m_dir_index, value);
    Save(value);
  }

  const unsigned int m_dir_index;
};

// Validates a printf-style format for use with exactly one double argument. QString::asprintf
// cannot check a caller-supplied format at compile time, and a mismatched conversion reads the
// argument as the wrong type: %d silently prints nothing useful, %s dereferences it and crashes.
bool IsValidFloatFormat(const QString& format)
{
  // %% is a literal percent rather than a conversion, so drop those before counting.
  QString conversions = format;
  conversions.remove(QStringLiteral("%%"));
  if (conversions.count(u'%') != 1)
    return false;

  // Whatever sits between the % and the conversion character is flags, width and precision.
  for (int i = conversions.lastIndexOf(u'%') + 1; i < conversions.length(); ++i)
  {
    const QChar c = conversions[i];
    if (c == u'f' || c == u'F' || c == u'e' || c == u'E' || c == u'g' || c == u'G')
      return true;
    if (!c.isDigit() && c != u'-' && c != u'+' && c != u' ' && c != u'#' && c != u'.')
      return false;
  }
  return false;
}

// initStyleOption is protected on QCheckBox, QRadioButton and QSlider, so a filter cannot call it.
// QStyleOption::initFrom is public, but it only covers rect, state's window-level bits, direction,
// palette and fontMetrics - not the per-widget fields below.
int IndicatorWidth(const QWidget* widget, QStyle::SubElement sub_element)
{
  constexpr int FALLBACK_WIDTH = 18;
  QStyle* const style = widget->style();
  if (style == nullptr)
    return FALLBACK_WIDTH;

  QStyleOptionButton opt;
  opt.initFrom(widget);
  opt.rect = widget->rect();
  // QCheckBox/QRadioButton::initStyleOption also set these, and QStyleSheetStyle resolves the
  // indicator's geometry through the :checked/:unchecked/:indeterminate/:pressed pseudo-classes
  // that come from them. Without them a user stylesheet that sizes the indicator per state anchors
  // the balloon in the wrong place.
  if (const auto* const button = qobject_cast<const QAbstractButton*>(widget))
  {
    if (button->isDown())
      opt.state |= QStyle::State_Sunken;
    const auto* const check_box = qobject_cast<const QCheckBox*>(widget);
    if (check_box != nullptr && check_box->checkState() == Qt::PartiallyChecked)
      opt.state |= QStyle::State_NoChange;
    else
      opt.state |= button->isChecked() ? QStyle::State_On : QStyle::State_Off;
    opt.text = button->text();
    opt.icon = button->icon();
    opt.iconSize = button->iconSize();
  }
  return style->subElementRect(sub_element, &opt, widget).width();
}

QRect SliderHandleRect(const QSlider* slider)
{
  constexpr QRect FALLBACK_RECT{0, 0, 15, 15};
  QStyle* const style = slider->style();
  if (style == nullptr)
    return FALLBACK_RECT;

  // The fields QSlider::initStyleOption sets, all of them publicly readable.
  QStyleOptionSlider opt;
  opt.initFrom(slider);
  opt.rect = slider->rect();
  opt.subControls = QStyle::SC_None;
  opt.activeSubControls = QStyle::SC_None;
  opt.orientation = slider->orientation();
  opt.minimum = slider->minimum();
  opt.maximum = slider->maximum();
  opt.tickPosition = slider->tickPosition();
  opt.tickInterval = slider->tickInterval();
  opt.upsideDown = slider->orientation() == Qt::Horizontal ?
                       slider->invertedAppearance() != (opt.direction == Qt::RightToLeft) :
                       !slider->invertedAppearance();
  opt.direction = Qt::LeftToRight;  // upsideDown carries the inversion instead
  opt.sliderPosition = slider->sliderPosition();
  opt.sliderValue = slider->value();
  opt.singleStep = slider->singleStep();
  opt.pageStep = slider->pageStep();
  if (slider->orientation() == Qt::Horizontal)
    opt.state |= QStyle::State_Horizontal;

  return style->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, slider);
}

void RecordSetting(const Config::Location& location, SettingKind kind, Config::Layer* layer,
                   const QWidget* widget)
{
  SettingEntry entry;
  entry.location = location;
  entry.kind = kind;
  entry.per_game = layer != nullptr;

  if (const auto* const box = qobject_cast<const QComboBox*>(widget))
  {
    const int first_choice = box->property(INHERIT_COMBO_PROPERTY).toBool() ? 1 : 0;
    for (int i = first_choice; i < box->count(); ++i)
      entry.choices.push_back(box->itemText(i));
  }
  else if (const auto* const spin = qobject_cast<const QSpinBox*>(widget))
  {
    entry.minimum = spin->minimum();
    entry.maximum = spin->maximum();
    entry.step = spin->singleStep();
  }
  else if (const auto* const slider = qobject_cast<const QSlider*>(widget))
  {
    entry.minimum = slider->minimum();
    entry.maximum = slider->maximum();
  }

  ConfigSettingRegistry::Instance().Record(std::move(entry));
}
}  // namespace

namespace detail
{
void PrepareLayeredCombo(QComboBox* box, Config::Layer* layer)
{
  if (layer == nullptr)
    return;
  box->insertItem(0, QString{});
  box->setProperty(INHERIT_COMBO_PROPERTY, true);
}

void SetInheritedComboText(QComboBox* box, int inherited_item_index)
{
  const QString inherited_value = inherited_item_index > 0 && inherited_item_index < box->count() ?
                                      box->itemText(inherited_item_index) :
                                      QString{};
  box->setItemText(0, QObject::tr("Use Global Setting [%1]").arg(inherited_value));
}

void RecordMappedCombo(const Config::Location& location, Config::Layer* layer,
                       const QComboBox* widget)
{
  RecordSetting(location, SettingKind::Choice, layer, widget);
}
}  // namespace detail

void Bind(QCheckBox* widget, const Config::Info<bool>& setting, Config::Layer* layer, bool reverse)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new CheckBoxBinding{widget, setting, layer, reverse};
  RecordSetting(setting.GetLocation(), SettingKind::Bool, layer, widget);
}

void Bind(QComboBox* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new ComboBoxBinding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Choice, layer, widget);
}

void Bind(QSpinBox* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new SpinBoxBinding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Int, layer, widget);
}

void Bind(QSlider* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new SliderBinding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Int, layer, widget);
}

void Bind(QRadioButton* widget, const Config::Info<int>& setting, int value, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new RadioButtonBinding{widget, setting, value, layer};
  // Several radio buttons share one location, each carrying a different `value`. Recording merges
  // by location, so a three-radio group collapses into one SettingKind::Int entry and the `value`
  // arguments are lost. The follow-up project's settings browser will special-case radio groups.
  RecordSetting(setting.GetLocation(), SettingKind::Int, layer, widget);
}

void Bind(QLineEdit* widget, const Config::Info<std::string>& setting, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new LineEditBinding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::String, layer, widget);
}

void Bind(QComboBox* widget, const Config::Info<u32>& setting, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new ComboBoxU32Binding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Choice, layer, widget);
}

void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::string> options, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  DEBUG_ASSERT(!options.empty());
  DEBUG_ASSERT(widget->count() == 0);
  for (const std::string& option : options)
  {
    const QString text = QString::fromStdString(option);
    widget->addItem(text, text);
  }
  new StringChoiceBinding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Choice, layer, widget);
}

void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::pair<QString, QString>> options, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  DEBUG_ASSERT(!options.empty());
  DEBUG_ASSERT(widget->count() == 0);
  for (const auto& [text, value] : options)
    widget->addItem(text, value);
  new StringChoiceBinding{widget, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Choice, layer, widget);
}

void BindMapped(QSlider* widget, const Config::Info<int>& setting, std::span<const int> tick_values,
                Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  DEBUG_ASSERT(!tick_values.empty());
  DEBUG_ASSERT(widget->minimum() == 0 &&
               widget->maximum() == static_cast<int>(tick_values.size()) - 1);
  new TickSliderBinding{widget, setting, std::vector<int>(tick_values.begin(), tick_values.end()),
                        layer};
  SettingEntry entry;
  entry.location = setting.GetLocation();
  entry.kind = SettingKind::Int;
  entry.per_game = layer != nullptr;
  entry.minimum = static_cast<double>(tick_values.front());
  entry.maximum = static_cast<double>(tick_values.back());
  // No step: irregular spacing is the whole point of a mapped slider.
  ConfigSettingRegistry::Instance().Record(std::move(entry));
}

void BindScaled(QSlider* widget, const Config::Info<u32>& setting, u32 scale, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new ScaledSliderBinding{widget, setting, scale, layer};
  SettingEntry entry;
  entry.location = setting.GetLocation();
  entry.kind = SettingKind::U32;
  entry.per_game = layer != nullptr;
  entry.minimum = static_cast<double>(widget->minimum()) * scale;
  entry.maximum = static_cast<double>(widget->maximum()) * scale;
  entry.step = static_cast<double>(scale);
  ConfigSettingRegistry::Instance().Record(std::move(entry));
}

float FloatSliderHandle::Value() const
{
  return range.ValueForPosition(slider->value());
}

FloatSliderHandle BindFloat(QSlider* widget, const Config::Info<float>& setting, float minimum,
                            float maximum, float step, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  const FloatSliderRange range{minimum, maximum, step};
  DEBUG_ASSERT(range.MaximumPosition() > 0);
  DEBUG_ASSERT(widget->minimum() == 0 && widget->maximum() == range.MaximumPosition());
  new FloatSliderBinding{widget, setting, range, layer};
  SettingEntry entry;
  entry.location = setting.GetLocation();
  entry.kind = SettingKind::Float;
  entry.per_game = layer != nullptr;
  entry.minimum = minimum;
  entry.maximum = maximum;
  entry.step = step;
  ConfigSettingRegistry::Instance().Record(std::move(entry));
  return FloatSliderHandle{widget, range};
}

void MirrorFloatValue(QLabel* label, FloatSliderHandle handle, const QString& format)
{
  DEBUG_ASSERT(handle.slider != nullptr);
  DEBUG_ASSERT(IsValidFloatFormat(format));
  const auto update = [label, handle, format] {
    label->setText(
        QString::asprintf(format.toUtf8().constData(), static_cast<double>(handle.Value())));
  };
  QObject::connect(handle.slider, &QSlider::valueChanged, label, update);
  update();
}

namespace
{
Config::Location LocationOf(const ComplexBinding::InfoVariant& info)
{
  return std::visit([](const auto& setting) { return setting.GetLocation(); }, info);
}
}  // namespace

ComplexBinding::ComplexBinding(QComboBox* box, const InfoVariant& setting1,
                               const InfoVariant& setting2, Config::Layer* layer)
    : ConfigBinding(box, LocationOf(setting1), layer), m_setting1(setting1), m_setting2(setting2)
{
  SetSecondaryLocation(LocationOf(setting2));
  detail::PrepareLayeredCombo(box, layer);
  connect(box, &QComboBox::currentIndexChanged, this, &ComplexBinding::OnIndexChanged);
  RefreshFromConfig();
}

void ComplexBinding::Add(const QString& name, OptionVariant option1, OptionVariant option2)
{
  auto* const box = static_cast<QComboBox*>(GetWidget());
  // The upper-bound guard in OnIndexChanged already rejects the first addItem's auto-selection
  // (index 0 arrives while m_options is empty). The blocker is kept so that protection does not
  // depend on addItem preceding emplace_back.
  const QSignalBlocker blocker{box};
  box->addItem(name);
  m_options.emplace_back(std::move(option1), std::move(option2));
  RefreshFromConfig();
}

void ComplexBinding::SetDefault(int index)
{
  m_default_index = index;
  RefreshFromConfig();
}

void ComplexBinding::Reset()
{
  auto* const box = static_cast<QComboBox*>(GetWidget());
  const QSignalBlocker blocker{box};
  box->clear();
  m_options.clear();
  detail::PrepareLayeredCombo(box, GetLayer());
}

std::pair<Config::Location, Config::Location> ComplexBinding::GetLocations() const
{
  return {LocationOf(m_setting1), LocationOf(m_setting2)};
}

void ComplexBinding::LoadFromConfig()
{
  const bool layered = GetLayer() != nullptr;
  const auto read = [this](const auto& setting) -> OptionVariant {
    if (GetLayer() != nullptr && GetLayer()->Exists(setting.GetLocation()))
      return static_cast<OptionVariant>(GetLayer()->Get(setting));
    if (GetLayer() != nullptr)
      return static_cast<OptionVariant>(Logic::ReadInheritedValue(setting, GetLayer()));
    return static_cast<OptionVariant>(Config::Get(setting));
  };
  const auto read_inherited = [this](const auto& setting) -> OptionVariant {
    return static_cast<OptionVariant>(Logic::ReadInheritedValue(setting, GetLayer()));
  };
  const auto default_of = [](const auto& setting) -> OptionVariant {
    return OptionVariant(setting.GetDefaultValue());
  };

  const auto matches = [&](const InfoVariant& info, const OptionVariant& option,
                           const auto& reader) {
    const OptionVariant wanted = std::holds_alternative<Config::DefaultState>(option) ?
                                     std::visit(default_of, info) :
                                     option;
    return std::visit(reader, info) == wanted;
  };

  const auto it = std::ranges::find_if(m_options, [&](const auto& option) {
    return matches(m_setting1, option.first, read) && matches(m_setting2, option.second, read);
  });

  auto* const box = static_cast<QComboBox*>(GetWidget());
  if (layered)
  {
    const auto inherited_it = std::ranges::find_if(m_options, [&](const auto& option) {
      return matches(m_setting1, option.first, read_inherited) &&
             matches(m_setting2, option.second, read_inherited);
    });
    const int inherited_index =
        inherited_it == m_options.end() ?
            -1 :
            static_cast<int>(std::distance(m_options.begin(), inherited_it)) + 1;
    detail::SetInheritedComboText(box, inherited_index);

    const auto [location1, location2] = GetLocations();
    if (!GetLayer()->Exists(location1) && !GetLayer()->Exists(location2))
    {
      const QSignalBlocker blocker{box};
      box->setCurrentIndex(0);
      return;
    }
  }

  const int index = it == m_options.end() ?
                        (m_default_index < 0 ? -1 : m_default_index + (layered ? 1 : 0)) :
                        static_cast<int>(std::distance(m_options.begin(), it)) + (layered ? 1 : 0);
  const QSignalBlocker blocker{box};
  box->setCurrentIndex(index);
}

void ComplexBinding::OnIndexChanged(int index)
{
  if (IsUpdating())
    return;

  if (GetLayer() != nullptr)
  {
    if (index == 0)
    {
      ClearLocalValue();
      RefreshFromConfig();
      return;
    }
    --index;
  }

  if (index < 0 || static_cast<size_t>(index) >= m_options.size())
    return;

  const auto set = [this](const auto& setting, const auto& value) {
    if (Config::Layer* const layer = GetLayer())
    {
      layer->Set(setting.GetLocation(), value);
      Config::OnConfigChanged();
      return;
    }
    Config::SetBaseOrCurrent(setting, value);
  };

  std::visit(set, m_setting1, m_options[static_cast<size_t>(index)].first);
  std::visit(set, m_setting2, m_options[static_cast<size_t>(index)].second);
  RefreshFromConfig();
}

ComplexBinding* BindComplex(QComboBox* widget, const ComplexBinding::InfoVariant& setting1,
                            const ComplexBinding::InfoVariant& setting2, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  DEBUG_ASSERT(widget->count() == 0);
  auto* const binding = new ComplexBinding{widget, setting1, setting2, layer};
  // Complex bindings are populated afterwards, so record kind = Complex with empty choices and
  // record nothing further. The follow-up project's settings browser will special-case complex
  // settings.
  const Config::Location location = LocationOf(setting1);
  RecordSetting(location, SettingKind::Complex, layer, widget);
  return binding;
}

void BindUserPath(QLineEdit* widget, unsigned int dir_index,
                  const Config::Info<std::string>& setting, Config::Layer* layer)
{
  DEBUG_ASSERT(FindBinding(widget) == nullptr);
  new UserPathBinding{widget, dir_index, setting, layer};
  RecordSetting(setting.GetLocation(), SettingKind::Path, layer, widget);
}

void SetPathWarningHandlerForTesting(PathWarningHandler handler)
{
  s_path_warning_handler = std::move(handler);
}

ConfigBinding* FindBinding(QWidget* widget)
{
  return widget->findChild<ConfigBinding*>(QString{}, Qt::FindDirectChildrenOnly);
}

void MirrorFont(QLabel* label, QWidget* control)
{
  ConfigBinding* const binding = FindBinding(control);
  DEBUG_ASSERT_MSG(COMMON, binding != nullptr, "MirrorFont called on an unbound control");
  if (binding == nullptr)
    return;
  binding->AddFontMirror(label);
}

QPoint ToolTipAnchor(const QWidget* widget)
{
  if (const auto* const slider = qobject_cast<const QSlider*>(widget))
    return SliderHandleRect(slider).center();
  if (qobject_cast<const QCheckBox*>(widget) != nullptr)
    return {IndicatorWidth(widget, QStyle::SE_CheckBoxIndicator) / 2, widget->height() / 2};
  if (qobject_cast<const QRadioButton*>(widget) != nullptr)
    return {IndicatorWidth(widget, QStyle::SE_RadioButtonIndicator) / 2, widget->height() / 2};

  return {widget->width() / 2, widget->height() / 2};
}

void SetDescription(QWidget* widget, QString title, QString description)
{
  if (title.isEmpty())
  {
    // Only buttons derive a title from their label. A generic property("text") read also matches
    // QAbstractSpinBox and QLineEdit, where "text" is the current *value* - a spin box would get a
    // balloon titled "50 ms".
    if (const auto* const button = qobject_cast<const QAbstractButton*>(widget))
      title = button->text();
  }

  SettingsHelp::Set(widget, title, description);

  auto* filter = widget->findChild<BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  if (filter == nullptr)
    filter = new BalloonTipFilter{widget};
  // Pass copies to the filter because the registry forward below still needs the strings.
  filter->SetText(title, description);

  // Unbound widgets get a tooltip and no registry entry.
  if (const ConfigBinding* const binding = FindBinding(widget))
  {
    ConfigSettingRegistry::Instance().SetText(binding->GetLocation(), std::move(title),
                                              std::move(description));
  }
}

QString ToolTipTitle(const QWidget* widget)
{
  const auto* const filter =
      widget->findChild<const BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  return filter == nullptr ? QString{} : filter->GetTitle();
}

QString ToolTipDescription(const QWidget* widget)
{
  const auto* const filter =
      widget->findChild<const BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  return filter == nullptr ? QString{} : filter->GetDescription();
}

bool EffectiveChecked(const QCheckBox* widget)
{
  if (widget->checkState() == Qt::PartiallyChecked)
    return widget->property(INHERITED_CHECKED_PROPERTY).toBool();
  return widget->isChecked();
}

bool IsInherited(const QWidget* widget)
{
  return widget->property("dolphinInheritedSetting").toBool();
}
}  // namespace ConfigWidget
