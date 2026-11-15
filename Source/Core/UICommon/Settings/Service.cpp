// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/Settings/Service.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Common/Common.h"

namespace UICommon::Settings
{
namespace
{
std::optional<double> Number(const Value& value)
{
  return std::visit(
      [](const auto& typed) -> std::optional<double> {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
          return static_cast<double>(typed);
        return std::nullopt;
      },
      value);
}

bool Valid(const Descriptor& descriptor, const Value& value, bool default_reset)
{
  if (value.index() != descriptor.default_value.index())
    return false;
  if (const auto number = Number(value); number && !std::isfinite(*number))
    return false;
  if (const auto* text = std::get_if<std::string>(&value);
      text && (text->size() > 4096 || text->find('\0') != std::string::npos))
    return false;
  // Defaults can name an unavailable backend/preset. Reset must still restore that default.
  if (!default_reset && descriptor.control == Control::Choice &&
      std::ranges::none_of(descriptor.choices,
                           [&](const Choice& choice) { return choice.value == value; }))
    return false;
  if (!default_reset && !descriptor.ticks.empty() &&
      std::ranges::find(descriptor.ticks, value) == descriptor.ticks.end())
    return false;
  if (descriptor.range)
  {
    const auto number = Number(value);
    const auto& range = *descriptor.range;
    if (!number || *number < range.minimum - 0.000001 || *number > range.maximum + 0.000001)
      return false;
    if (range.step > 0)
    {
      const double steps = (*number - range.minimum) / range.step;
      // Float values and float-derived steps lose precision before promotion to double.
      // Account for that accumulated error without relaxing validation for integer settings.
      const double tolerance = std::holds_alternative<float>(value) ?
                                   std::max(0.0001, 2 * std::numeric_limits<float>::epsilon() *
                                                        std::max(1.0, std::abs(steps))) :
                                   0.0001;
      if (std::abs(steps - std::round(steps)) > tolerance)
        return false;
    }
  }
  return true;
}

std::optional<Value> FromDisplay(const Descriptor& descriptor, double display)
{
  if (!std::isfinite(display) || !descriptor.range || descriptor.range->scale == 0)
    return std::nullopt;
  const double stored = (display - descriptor.range->offset) / descriptor.range->scale;
  return std::visit(
      [stored](const auto& value) -> std::optional<Value> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, float>)
        {
          if (!std::isfinite(stored) || std::abs(stored) > std::numeric_limits<float>::max())
            return std::nullopt;
          return Value{static_cast<float>(stored)};
        }
        else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, u32> ||
                           std::is_same_v<T, u64> || std::is_same_v<T, s64>)
        {
          if (!std::isfinite(stored) || stored < std::numeric_limits<T>::lowest() ||
              stored > std::numeric_limits<T>::max() ||
              std::abs(stored - std::round(stored)) > 0.000001)
            return std::nullopt;
          return Value{static_cast<T>(std::round(stored))};
        }
        return std::nullopt;
      },
      descriptor.default_value);
}
}  // namespace

Service::Service(const Catalog& catalog) : m_catalog(catalog), m_owner(std::this_thread::get_id())
{
}
bool Service::IsOwner() const
{
  return m_owner == std::this_thread::get_id();
}

bool Service::SetContext(Context context)
{
  if (!IsOwner() ||
      (context.target != EditTarget::Game && context.target != EditTarget::Base &&
       context.target != EditTarget::Effective) ||
      (context.target == EditTarget::Game) != bool(context.layer) ||
      (context.layer && context.layer->GetLayer() != Config::LayerType::LocalGame) ||
      (context.fallback && context.fallback->GetLayer() != Config::LayerType::GlobalGame) ||
      (context.target != EditTarget::Game && context.fallback))
    return false;
  m_context = std::move(context);
  ++m_revision;
  return true;
}

bool Service::SetEnvironment(Environment environment)
{
  if (!IsOwner())
    return false;
  if (m_environment != environment)
  {
    m_environment = std::move(environment);
    ++m_revision;
  }
  return true;
}

void Service::SynchronizeVersion()
{
  const u64 version = Config::GetConfigVersion();
  if (version != m_config_version)
  {
    m_config_version = version;
    ++m_revision;
  }
}

std::string Service::Unavailable(const Entry& entry) const
{
  if (m_context.target == EditTarget::Game && !entry.descriptor.per_game)
    return _trans("This setting is only available globally.");
  if (m_environment.transitioning)
    return _trans("Wait for the game transition to finish.");
  if (m_environment.netplay && !entry.descriptor.netplay_safe)
    return _trans("Settings changes are unavailable during NetPlay.");
  if (m_environment.movie && !entry.descriptor.netplay_safe)
    return _trans("This setting cannot be changed during movie recording or playback.");
  if (m_environment.hardcore && !entry.descriptor.hardcore_safe)
    return _trans("This setting cannot be changed in Hardcore Mode.");
  if (entry.descriptor.policy == ApplyPolicy::IdleOnly && m_environment.emulating)
    return _trans("Stop emulation before changing this setting.");
  return entry.unavailable ? entry.unavailable(m_environment, m_context) : std::string{};
}

std::shared_ptr<const Snapshot> Service::ReadSnapshot()
{
  if (!IsOwner())
    return nullptr;
  SynchronizeVersion();
  if (m_snapshot && m_snapshot->revision == m_revision)
    return m_snapshot;
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->revision = m_revision;
  snapshot->target = m_context.target;
  for (const auto& entry : m_catalog.Entries())
  {
    State state;
    state.descriptor = entry.descriptor;
    if (entry.options)
      state.descriptor.choices = entry.options(m_environment);
    state.value = entry.read(m_context);
    state.inherited_value = entry.inherited(m_context);
    if (state.descriptor.control == Control::Number)
      state.value_available = Valid(state.descriptor, state.value, false);
    const auto& location = state.descriptor.location;
    if (m_context.target == EditTarget::Game && m_context.layer->Exists(location))
      state.source_layer = Config::LayerType::LocalGame;
    else if (m_context.target == EditTarget::Game && m_context.fallback &&
             m_context.fallback->Exists(location))
      state.source_layer = m_context.fallback->GetLayer();
    else if (m_context.target == EditTarget::Effective && Config::GetAsString(location))
      state.source_layer = Config::GetActiveLayerForConfig(location);
    else if (Config::GetLayer(Config::LayerType::Base)->Exists(location))
      state.source_layer = Config::LayerType::Base;
    if (state.descriptor.control == Control::Choice)
      state.value_available =
          std::ranges::any_of(state.descriptor.choices,
                              [&](const Choice& choice) { return choice.value == state.value; });
    if (!state.descriptor.ticks.empty())
      state.value_available =
          std::ranges::find(state.descriptor.ticks, state.value) != state.descriptor.ticks.end();
    state.overridden = m_context.target != EditTarget::Base &&
                       Logic::IsLocal(state.descriptor.location, m_context.layer.get());
    state.display_value = state.value;
    if (state.descriptor.reverse)
      state.display_value = !std::get<bool>(state.value);
    if (state.descriptor.range)
      if (const auto number = Number(state.value))
        state.display_value = static_cast<float>(*number * state.descriptor.range->scale +
                                                 state.descriptor.range->offset);
    state.disabled_reason = Unavailable(entry);
    state.enabled = state.disabled_reason.empty();
    snapshot->settings.push_back(std::move(state));
  }
  m_snapshot = snapshot;
  return snapshot;
}

Result Service::Apply(const Request& request)
{
  if (!IsOwner())
    return {Status::WrongThread};
  SynchronizeVersion();
  if (request.revision != m_revision)
    return {Status::Stale};
  const auto* entry = m_catalog.Find(request.id);
  if (!entry)
    return {Status::UnknownSetting};
  const auto policy = entry->descriptor.policy;
  if (m_context.target == EditTarget::Game && !entry->descriptor.per_game)
    return {Status::UnsupportedContext, policy};
  if (!Unavailable(*entry).empty())
    return {Status::Unavailable, policy};
  if (request.operation == Operation::ClearOverride)
  {
    if (m_context.target != EditTarget::Game)
      return {Status::UnsupportedContext, policy};
    Logic::Clear(entry->descriptor.location, m_context.layer.get());
  }
  else
  {
    auto descriptor = entry->descriptor;
    if (entry->options)
      descriptor.choices = entry->options(m_environment);
    const bool reset = request.operation == Operation::ResetDefault;
    Value value = reset ? descriptor.default_value : request.value;
    if (request.operation == Operation::SetDisplay)
    {
      if (descriptor.control == Control::Toggle && std::holds_alternative<bool>(request.value))
        value = std::get<bool>(request.value) != descriptor.reverse;
      else if (const auto converted = FromDisplay(descriptor, request.display_value))
        value = *converted;
      else
        return {Status::InvalidValue, policy};
    }
    else if (request.operation != Operation::Set && !reset)
      return {Status::InvalidValue, policy};
    if (!Valid(descriptor, value, reset))
      return {Status::InvalidValue, policy};
    entry->write(m_context, value);
  }
  ++m_revision;
  m_config_version = Config::GetConfigVersion();
  return {Status::Success, policy};
}

bool Service::Save()
{
  if (!IsOwner())
    return false;
  const auto layer = m_context.target == EditTarget::Game ?
                         m_context.layer :
                         Config::GetLayer(Config::LayerType::Base);
  if (!layer)
    return false;
  layer->Save();
  return true;
}
}  // namespace UICommon::Settings
