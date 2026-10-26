// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureSettings.h"

#include <algorithm>
#include <set>

#include <QDir>
#include <QThread>

#include "Common/FileSearch.h"
#include "Common/IniFile.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "DolphinQt/Config/Binder/SharedSettings.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"

namespace
{
using namespace Common::Settings;
using O = BigPicture::SettingsOperation;
QString Q(const std::string& value)
{
  return QString::fromStdString(value);
}
std::string S(const QString& value)
{
  return value.toStdString();
}
Config::Location ProfileLocation(const BigPictureSettings::ControllerTarget& target)
{
  return {Config::System::GameSettingsOnly, "Controls",
          target.config->GetProfileKey() + "Profile" + std::to_string(target.index + 1)};
}
std::string Display(const Value& value)
{
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>)
          return v;
        else if constexpr (std::is_same_v<T, bool>)
          return S(v ? BigPictureSettings::tr("On") : BigPictureSettings::tr("Off"));
        else
          return S(QString::number(v));
      },
      value);
}
std::string ValueLabel(const State& state)
{
  const auto choice = std::ranges::find(state.descriptor.choices, state.value, &Choice::value);
  return choice != state.descriptor.choices.end() ? choice->label : Display(state.display_value);
}
bool ValidGameId(const std::string& id)
{
  return id.size() >= 4 && id.size() <= 8 && id != "00000000" &&
         std::ranges::all_of(
             id, [](unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
}
}  // namespace

BigPictureSettings::BigPictureSettings(QObject* parent, std::function<void()> changed,
                                       std::vector<ControllerTarget> targets,
                                       std::function<std::vector<std::string>()> devices)
    : QObject(parent), m_targets(std::move(targets)), m_input_devices(std::move(devices)),
      m_changed(std::move(changed)), m_worker("BigPicturePresets", [this](bool) {
        auto presets = VideoCommon::MultipassPostProcessing::GetPresetList();
        std::ranges::sort(presets);
        presets.erase(std::unique(presets.begin(), presets.end()), presets.end());
        QueueOnObject(this, [this, presets = std::move(presets)] {
          m_presets = presets;
          m_presets_loading = false;
          ++m_revision;
          if (m_changed)
            m_changed();
        });
      })
{
  m_service.SetContext({EditTarget::Base});
}
BigPictureSettings::~BigPictureSettings()
{
  m_worker.StopAndCancel();
}
void BigPictureSettings::Open()
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_service.SetContext({EditTarget::Base});
  if (!m_game.active)
    m_game_context = {};
  m_page = Page::Home;
  ++m_revision;
}
void BigPictureSettings::SetGame(Game game)
{
  Q_ASSERT(QThread::currentThread() == thread());
  if (m_game == game)
    return;
  m_service.SetContext({EditTarget::Base});
  m_game_context = {};
  m_game = std::move(game);
  m_page = Page::Home;
  ++m_revision;
}
void BigPictureSettings::SetEnvironment(UICommon::Settings::Environment environment)
{
  Q_ASSERT(QThread::currentThread() == thread());
  m_environment = environment;
  m_service.SetEnvironment(std::move(environment));
}
std::shared_ptr<const Snapshot> BigPictureSettings::ReadSnapshot()
{
  return ConfigWidget::TranslateSettingsSnapshot(*m_service.ReadSnapshot());
}
UICommon::Settings::Context BigPictureSettings::GameContext()
{
  if (m_game_context.layer)
    return m_game_context;
  m_game_context.target = EditTarget::Game;
  if (m_game.active)
  {
    m_game_context.layer = Config::GetLayer(Config::LayerType::LocalGame);
    m_game_context.fallback = Config::GetLayer(Config::LayerType::GlobalGame);
  }
  else if (ValidGameId(m_game.id))
  {
    m_game_context.layer = std::make_shared<Config::Layer>(
        ConfigLoaders::GenerateLocalGameConfigLoader(m_game.id, m_game.revision));
    m_game_context.fallback = std::make_shared<Config::Layer>(
        ConfigLoaders::GenerateGlobalGameConfigLoader(m_game.id, m_game.revision));
  }
  return m_game_context;
}
std::vector<BigPictureSettings::ControllerTarget> BigPictureSettings::Targets() const
{
  if (!m_targets.empty())
    return m_targets;
  std::vector<ControllerTarget> result;
  for (auto* config : {Pad::GetConfig(), Wiimote::GetConfig()})
    for (int index = 0; index < std::min(4, config->GetControllerCount()); ++index)
      result.push_back(
          {config->GetProfileKey() + std::to_string(index),
           S((config == Pad::GetConfig() ? tr("GameCube Controller %1") : tr("Wii Remote %1"))
                 .arg(index + 1)),
           config, index});
  return result;
}
std::vector<std::string> BigPictureSettings::Devices() const
{
  return m_input_devices ? m_input_devices() : g_controller_interface.GetAllDeviceStrings();
}
const BigPictureSettings::ControllerTarget*
BigPictureSettings::Target(const std::vector<ControllerTarget>& targets) const
{
  const auto it = std::ranges::find(targets, m_controller, &ControllerTarget::id);
  return it == targets.end() ? nullptr : &*it;
}
std::vector<BigPictureSettings::Profile>
BigPictureSettings::Profiles(const ControllerTarget& target)
{
  std::vector<Profile> result;
  const bool game = m_service.ReadSnapshot()->target == EditTarget::Game;
  for (const auto& [root, stock] : {std::pair{target.config->GetUserProfileDirectoryPath(), false},
                                    std::pair{target.config->GetSysProfileDirectoryPath(), true}})
  {
    if (game && stock)
      continue;  // Dolphin's per-game profile references use the user directory.
    for (const auto& path : Common::DoFileSearch(root, ".ini", true))
    {
      auto name = QDir(Q(root)).relativeFilePath(Q(path));
      name.chop(4);
      if (name.isEmpty() || (game && (name.contains(QLatin1Char(',')) || name.trimmed() != name)))
        continue;
      result.push_back({path, S(stock ? tr("%1 (Stock)").arg(name) : name), S(name)});
    }
  }
  std::ranges::sort(result, {}, &Profile::name);
  return result;
}
std::string BigPictureSettings::ControllerUnavailable() const
{
  if (m_environment.transitioning)
    return S(tr("Wait for the game transition to finish."));
  if (m_environment.netplay)
    return S(tr("Controller changes are unavailable during NetPlay."));
  if (m_environment.movie)
    return S(tr("Controller changes are unavailable during movie recording or playback."));
  if (m_environment.emulating)
    return S(tr("Stop emulation before changing controllers."));
  return {};
}
void BigPictureSettings::ScanPresets()
{
  if (m_presets_loading)
    return;
  m_presets_loading = true;
  m_worker.EmplaceItem(true);
}
void BigPictureSettings::Back()
{
  switch (m_page)
  {
  case Page::Presets:
    if (!m_preset_folder.empty())
      m_preset_folder = S(Q(m_preset_folder).section(QLatin1Char('/'), 0, -2));
    else
      m_page = Page::Edit;
    break;
  case Page::Edit:
    m_page = Page::Category;
    break;
  case Page::Devices:
  case Page::Profiles:
    m_page = Page::Controller;
    break;
  case Page::Controller:
    m_page = Page::Controllers;
    break;
  default:
    m_page = Page::Home;
    break;
  }
}

std::shared_ptr<const BigPicture::SettingsMenuSnapshot> BigPictureSettings::ReadMenu()
{
  Q_ASSERT(QThread::currentThread() == thread());
  const auto values = ReadSnapshot();
  const auto devices = Devices();
  if (values->revision != m_settings_revision || devices != m_devices)
  {
    ++m_revision;
    m_settings_revision = values->revision;
    m_devices = devices;
  }
  auto view = std::make_shared<BigPicture::SettingsMenuSnapshot>();
  view->revision = m_revision;
  const bool game = values->target == EditTarget::Game;
  view->id =
      "settings-" + std::to_string(static_cast<int>(m_page)) + "-" +
      (m_page == Page::Category || m_page == Page::Edit || m_page == Page::Presets ? m_category :
                                                                                     "") +
      "-" + (m_page == Page::Edit || m_page == Page::Presets ? m_edit : "") + "-" +
      (m_page == Page::Controller || m_page == Page::Devices || m_page == Page::Profiles ?
           m_controller :
           "") +
      "-" + (m_page == Page::Presets ? m_preset_folder : "") + (game ? m_game.id : "global");
  view->title = S(tr("Settings"));
  view->description =
      S(game ? tr("Game settings: %1").arg(Q(m_game.title)) : tr("Global settings"));
  if (m_environment.emulating && !game)
    view->description +=
        "\n" + S(tr("Game or session overrides may take priority over global values."));
  const auto nav = [&](std::string id, QString label, O op, std::string value = {},
                       std::string detail = {}, bool enabled = true, bool toolbar = false) {
    BigPicture::SettingsRow row{std::move(id), S(label), std::move(detail), enabled,
                                BigPicture::SettingsRequest{m_revision, op, std::move(value)}};
    row.active =
        (op == O::Global && !game) || (op == O::Game && game) ||
        (op == O::Category && row.navigation->value == m_category &&
         (m_page == Page::Category || m_page == Page::Edit || m_page == Page::Presets)) ||
        (op == O::Controllers && (m_page == Page::Controllers || m_page == Page::Controller ||
                                  m_page == Page::Devices || m_page == Page::Profiles));
    (toolbar ? view->toolbar : view->rows).push_back(std::move(row));
  };
  const auto edit = [&](std::string id, QString label, const State& state, Value value,
                        Operation operation = Operation::Set, std::string detail = {}) {
    BigPicture::SettingsRow row{std::move(id), S(label),
                                state.enabled ? std::move(detail) : state.disabled_reason,
                                state.enabled};
    row.edit = Request{values->revision, state.descriptor.id, operation, std::move(value)};
    row.active = operation == Operation::Set        ? row.edit->value == state.value :
                 operation == Operation::SetDisplay ? row.edit->value == state.display_value :
                                                      false;
    view->rows.push_back(std::move(row));
  };
  if (m_page != Page::Home)
    view->back = BigPicture::SettingsRequest{m_revision, O::Back};
  nav("global", tr("Global settings"), O::Global, {}, {}, true, true);
  nav("game", tr("Game settings"), O::Game, {},
      ValidGameId(m_game.id) ? m_game.title : S(tr("Select a game in the library first.")),
      ValidGameId(m_game.id), true);
  if (m_page != Page::Home)
  {
    for (const auto& [category, label] :
         {std::pair{"Interface", tr("Interface and menu")}, std::pair{"Audio", tr("Audio")},
          std::pair{"Display", tr("Display")}, std::pair{"PostProcessing", tr("Post-processing")}})
      nav(category, label, O::Category, category, {}, true, true);
    nav("controllers", tr("Controllers"), O::Controllers, {}, {}, true, true);
  }
  if (m_page == Page::Home)
  {
    if (ValidGameId(m_game.id))
      view->description += "\n" + S(tr("Selected game: %1").arg(Q(m_game.title)));
    else
      view->description += "\n" + S(tr("Select a game in the library to edit game settings."));
    for (const auto& [category, label] :
         {std::pair{"Interface", tr("Interface and menu")}, std::pair{"Audio", tr("Audio")},
          std::pair{"Display", tr("Display")}, std::pair{"PostProcessing", tr("Post-processing")}})
      nav(category, label, O::Category, category);
    nav("controllers", tr("Controllers"), O::Controllers);
  }
  else if (m_page == Page::Category)
  {
    view->title = S(m_category == "Interface"      ? tr("Interface and menu") :
                    m_category == "Audio"          ? tr("Audio") :
                    m_category == "PostProcessing" ? tr("Post-processing") :
                                                     tr("Display"));
    view->preferences = m_category == "Interface" && !game;
    for (const auto& state : values->settings)
    {
      const bool preset = state.descriptor.id == "display.post_processing_preset";
      if ((m_category == "PostProcessing" && !preset) ||
          (m_category != "PostProcessing" &&
           (preset ||
            UICommon::Settings::GetCatalog().Find(state.descriptor.id)->descriptor.category !=
                m_category)))
        continue;
      std::string detail = ValueLabel(state);
      if (detail.empty())
        detail = S(tr("Off"));
      if (!state.value_available)
        detail += " — " + S(tr("Stored value unavailable"));
      if (game)
        detail += " — " + S(state.overridden ? tr("Override") : tr("Inherited"));
      if (!state.enabled)
        detail += "\n" + state.disabled_reason;
      nav(state.descriptor.id, Q(state.descriptor.label), O::Edit, state.descriptor.id, detail);
    }
  }
  else if (m_page == Page::Edit || m_page == Page::Presets)
  {
    const auto it = std::ranges::find(values->settings, m_edit,
                                      [](const State& state) { return state.descriptor.id; });
    if (it == values->settings.end())
      return view;
    const auto& state = *it;
    view->title = state.descriptor.label;
    view->description += "\n" + state.descriptor.help + "\n" + ValueLabel(state);
    if (game)
    {
      auto inherited = state;
      inherited.value = state.inherited_value;
      inherited.display_value = state.inherited_value;
      view->description += "\n" + S((state.overridden ? tr("Override — inherited value: %1") :
                                                        tr("Inherited value: %1"))
                                        .arg(Q(ValueLabel(inherited))));
    }
    else if (Config::GetActiveLayerForConfig(state.descriptor.location) != Config::LayerType::Base)
    {
      view->description +=
          "\n" +
          S(tr("A game or session override currently takes priority over this global value."));
    }
    view->description += "\n" + S(state.descriptor.policy == ApplyPolicy::IdleOnly ?
                                      tr("Stop emulation to change this setting.") :
                                  state.descriptor.policy == ApplyPolicy::NextBoot ?
                                      tr("Applies the next time Dolphin starts.") :
                                  game && !m_game.active ? tr("Applies on the next game start.") :
                                                           tr("Applies immediately."));
    if (!state.enabled)
      view->description += "\n" + state.disabled_reason;
    if (m_page == Page::Presets)
    {
      view->description += "\n" + m_preset_folder;
      if (m_presets_loading)
        view->description += "\n" + S(tr("Loading presets…"));
      const std::string prefix = m_preset_folder.empty() ? "" : m_preset_folder + "/";
      std::set<std::string> folders;
      for (const auto& preset : m_presets)
      {
        if (!preset.starts_with(prefix))
          continue;
        const auto rest = preset.substr(prefix.size());
        const auto slash = rest.find('/');
        if (slash != std::string::npos)
        {
          const auto folder = rest.substr(0, slash);
          if (folders.insert(folder).second)
            nav("folder-" + folder, Q(folder), O::PresetFolder, prefix + folder);
        }
        else
          edit("preset-" + preset, Q(rest), state, preset);
      }
      nav("refresh", tr("Refresh presets"), O::Refresh);
    }
    else
    {
      if (state.descriptor.control == Control::Toggle)
      {
        edit("on", tr("On"), state, true, Operation::SetDisplay);
        edit("off", tr("Off"), state, false, Operation::SetDisplay);
        view->default_focus = std::get<bool>(state.display_value) ? "on" : "off";
      }
      else if (state.descriptor.control == Control::Choice)
      {
        for (size_t index = 0; index < state.descriptor.choices.size(); ++index)
        {
          const auto& choice = state.descriptor.choices[index];
          const std::string id = "choice-" + std::to_string(index);
          edit(id, Q(choice.label), state, choice.value);
          if (choice.value == state.value)
            view->default_focus = id;
        }
      }
      else if (state.descriptor.control == Control::Number && m_edit == "audio.volume")
      {
        for (int value = 0; value <= 100; ++value)
        {
          const std::string id = "volume-" + std::to_string(value);
          edit(id, tr("%1%").arg(value), state, value);
          if (Value(value) == state.value)
            view->default_focus = id;
        }
      }
      else if (m_edit == "display.post_processing_preset")
      {
        nav("presets", tr("Choose preset"), O::PresetFolder, {}, state.disabled_reason,
            state.enabled);
        edit("off", tr("Off"), state, std::string{});
      }
      edit("reset", tr("Reset to default"), state, false, Operation::ResetDefault);
      if (game)
      {
        edit("inherit", tr("Use inherited value"), state, false, Operation::ClearOverride);
        view->rows.back().enabled = state.enabled && state.overridden;
      }
    }
  }
  else
  {
    const auto targets = Targets();
    const auto* target = Target(targets);
    const auto unavailable = ControllerUnavailable();
    if (m_page == Page::Controllers)
    {
      view->title = S(tr("Controllers"));
      view->description += "\n" + S(tr("Select an existing emulated controller. Full mapping and "
                                       "controller types are available on the desktop."));
      for (const auto& candidate : targets)
        nav(candidate.id, Q(candidate.label), O::Controller, candidate.id);
    }
    else if (target)
    {
      view->title = target->label;
      auto* controller = target->config->GetController(target->index);
      std::string device;
      {
        const auto lock = controller->GetStateLock();
        device = controller->GetDefaultDevice().ToString();
      }
      if (!unavailable.empty())
        view->description += "\n" + unavailable;
      view->description +=
          "\n" +
          S(tr(
              "Profile selection replaces the controller mapping. Menu controls remain separate."));
      if (m_page == Page::Controller)
      {
        nav("devices", tr("Input device"), O::Devices, {},
            game ? S(tr("Per-game devices come from the selected profile.")) : device,
            !game && unavailable.empty());
        std::string profile_value;
        if (game)
        {
          const auto location = ProfileLocation(*target);
          profile_value = m_game_context.layer->Get<std::string>(location).value_or(
              m_game_context.fallback ?
                  m_game_context.fallback->Get<std::string>(location).value_or("") :
                  "");
          profile_value +=
              " — " + S(m_game_context.layer->Exists(location) ? tr("Override") : tr("Inherited"));
        }
        nav("profiles", game ? tr("Game profile") : tr("Load profile"), O::Profiles, {},
            profile_value, unavailable.empty());
        if (game)
          nav("clear-profile", tr("Use inherited profile"), O::ClearProfile, {}, {},
              unavailable.empty() && m_game_context.layer->Exists(ProfileLocation(*target)));
      }
      else if (m_page == Page::Devices)
      {
        view->description += "\n" + device;
        if (std::ranges::find(m_devices, device) == m_devices.end())
          view->description +=
              "\n" + S(tr("Stored device is disconnected; it has been preserved."));
        for (const auto& available : m_devices)
          nav("device-" + available, Q(available), O::Device, available, {}, unavailable.empty());
        nav("refresh", tr("Refresh devices"), O::Refresh);
      }
      else
      {
        const auto profiles = Profiles(*target);
        if (profiles.empty())
          view->description +=
              "\n" +
              S(tr("No profiles found. Create profiles using desktop controller configuration."));
        if (game)
          view->description += "\n" + S(tr("Game profiles are selected from your user profiles and "
                                           "apply at the next game start."));
        for (const auto& profile : profiles)
          nav("profile-" + profile.path, Q(profile.name), O::Profile, profile.path, {},
              unavailable.empty());
      }
    }
  }
  return view;
}

Result BigPictureSettings::Apply(const Request& request)
{
  Q_ASSERT(QThread::currentThread() == thread());
  auto result = m_service.Apply(request);
  if (result.status == Status::Success)
    m_service.Save();
  return result;
}
Status BigPictureSettings::Execute(const BigPicture::SettingsRequest& request)
{
  Q_ASSERT(QThread::currentThread() == thread());
  const auto view = ReadMenu();
  if (request.revision != view->revision)
    return Status::Stale;
  // Only published actions are executable. Old pages and forged IDs cannot select a new target.
  const auto matches = [&](const BigPicture::SettingsRow& row) {
    return row.enabled && row.navigation && row.navigation->operation == request.operation &&
           row.navigation->value == request.value;
  };
  if (!(view->back && request.operation == O::Back) && !std::ranges::any_of(view->rows, matches) &&
      !std::ranges::any_of(view->toolbar, matches))
    return Status::Unavailable;
  const auto targets = Targets();
  const auto* target = Target(targets);
  switch (request.operation)
  {
  case O::Back:
    Back();
    break;
  case O::Category:
    m_category = request.value;
    m_page = Page::Category;
    break;
  case O::Global:
    m_service.SetContext({EditTarget::Base});
    break;
  case O::Game:
    if (!ValidGameId(m_game.id) || !m_service.SetContext(GameContext()))
      return Status::UnsupportedContext;
    break;
  case O::Edit:
    m_edit = request.value;
    m_page = Page::Edit;
    break;
  case O::Controllers:
    m_page = Page::Controllers;
    break;
  case O::Controller:
    m_controller = request.value;
    m_page = Page::Controller;
    break;
  case O::Devices:
    m_page = Page::Devices;
    break;
  case O::Profiles:
    m_page = Page::Profiles;
    break;
  case O::PresetFolder:
    m_page = Page::Presets;
    m_preset_folder = request.value;
    if (m_presets.empty())
      ScanPresets();
    break;
  case O::Refresh:
    if (m_page == Page::Presets)
      ScanPresets();
    else
      g_controller_interface.RefreshDevices();
    break;
  case O::Device:
  case O::Profile:
  case O::ClearProfile:
  {
    if (!target || !ControllerUnavailable().empty())
      return Status::Unavailable;
    if (m_service.ReadSnapshot()->target == EditTarget::Game)
    {
      if (request.operation == O::Device)
        return Status::UnsupportedContext;
      const auto location = ProfileLocation(*target);
      if (request.operation == O::ClearProfile)
        m_game_context.layer->DeleteKey(location);
      else
      {
        const auto profiles = Profiles(*target);
        const auto profile = std::ranges::find(profiles, request.value, &Profile::path);
        if (profile == profiles.end())
          return Status::InvalidValue;
        Common::IniFile file;
        if (!file.Load(profile->path) || !file.Exists("Profile"))
          return Status::InvalidValue;
        m_game_context.layer->Set(location, profile->game_value);
      }
      m_game_context.layer->Save();
      Config::OnConfigChanged();
    }
    else
    {
      auto* controller = target->config->GetController(target->index);
      if (request.operation == O::Device)
      {
        const auto devices = Devices();
        if (std::ranges::find(devices, request.value) == devices.end())
          return Status::Unavailable;
        const auto lock = controller->GetStateLock();
        controller->SetDefaultDevice(request.value);
      }
      else
      {
        Common::IniFile file;
        if (!file.Load(request.value) || !file.Exists("Profile"))
          return Status::InvalidValue;
        controller->LoadConfig(file.GetOrCreateSection("Profile"));
      }
      controller->UpdateReferences(g_controller_interface);
      target->config->SaveConfig();
      target->config->GenerateControllerTextures();
    }
    m_page = Page::Controller;
    break;
  }
  default:
    return Status::InvalidValue;
  }
  ++m_revision;
  return Status::Success;
}
