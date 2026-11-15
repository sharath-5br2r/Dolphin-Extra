// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureSettings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>
#include <span>
#include <string_view>

#include <fmt/format.h>

#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QTimer>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "DolphinQt/Config/Binder/SharedSettings.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "InputCommon/BigPictureInput.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"
#include "InputCommon/InputConfig.h"
#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"

namespace
{
using namespace Common::Settings;
using O = BigPicture::SettingsOperation;
constexpr auto CAPTURE_RELEASE_WAIT = std::chrono::seconds(1);
constexpr auto CAPTURE_INITIAL_WAIT = std::chrono::seconds(3);
constexpr auto CAPTURE_MAXIMUM_WAIT = std::chrono::seconds(5);
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
// Device qualifiers are quoted in expressions; the menu shows them without the backticks.
std::string BindingLabel(std::string expression)
{
  std::erase(expression, '`');
  return expression.empty() ? S(BigPictureSettings::tr("Unbound")) : expression;
}
// Visits the groups shown on a mapping page: the controller's own and those of its selected
// extension, keyed "[ext:<extension>/]<group>" and labelled by section.
void ForEachMappingGroup(ControllerEmu::ControlGroupContainer& container, const std::string& prefix,
                         const QString& section,
                         const std::function<void(const std::string&, const QString&,
                                                  ControllerEmu::ControlGroup&)>& visit)
{
  using ControllerEmu::GroupType;
  for (const auto& group : container.groups)
  {
    // Motion-sensor passthrough remains desktop mapping.
    if (group->type == GroupType::IMUAccelerometer || group->type == GroupType::IMUGyroscope ||
        group->type == GroupType::IMUCursor || group->type == GroupType::IRPassthrough)
      continue;
    // Every group is visited, including the extension group's own settings (Attach MotionPlus);
    // its extension selector is serialized separately and has its own row.
    visit(prefix + group->name, section + BigPictureSettings::tr(group->ui_name.c_str()), *group);
    if (group->type == GroupType::Attachments)
    {
      auto& attachments = static_cast<ControllerEmu::Attachments&>(*group);
      const auto& list = attachments.GetAttachmentList();
      const auto selected = attachments.GetSelectedAttachment();
      if (selected < list.size())
      {
        auto& attachment = *list[selected];
        ForEachMappingGroup(attachment, prefix + "ext:" + attachment.GetName() + "/",
                            BigPictureSettings::tr(attachment.GetDisplayName().c_str()) +
                                QStringLiteral(": "),
                            visit);
      }
    }
  }
}
ControllerEmu::Attachments* FindAttachments(ControllerEmu::EmulatedController& controller)
{
  for (const auto& group : controller.groups)
  {
    if (group->type == ControllerEmu::GroupType::Attachments)
      return static_cast<ControllerEmu::Attachments*>(group.get());
  }
  return nullptr;
}
double GetNumber(const ControllerEmu::NumericSettingBase& setting)
{
  using ControllerEmu::NumericSetting;
  switch (setting.GetType())
  {
  case ControllerEmu::SettingType::Int:
    return static_cast<const NumericSetting<int>&>(setting).GetValue();
  case ControllerEmu::SettingType::Double:
    return static_cast<const NumericSetting<double>&>(setting).GetValue();
  default:
    return static_cast<const NumericSetting<bool>&>(setting).GetValue();
  }
}
struct SettingBounds
{
  double minimum;
  double maximum;
  double step;
};
SettingBounds GetBounds(const ControllerEmu::NumericSettingBase& setting)
{
  using ControllerEmu::NumericSetting;
  if (setting.GetType() == ControllerEmu::SettingType::Int)
  {
    const auto& typed = static_cast<const NumericSetting<int>&>(setting);
    return {double(typed.GetMinValue()), double(typed.GetMaxValue()), 1};
  }
  const auto& typed = static_cast<const NumericSetting<double>&>(setting);
  // Whole display units for the usual percentages; finer steps for small ranges.
  const double range = typed.GetMaxValue() - typed.GetMinValue();
  return {typed.GetMinValue(), typed.GetMaxValue(), range >= 20 ? 1.0 : range >= 2 ? 0.1 : 0.01};
}
void SetNumber(ControllerEmu::NumericSettingBase& setting, double value)
{
  using ControllerEmu::NumericSetting;
  const auto bounds = GetBounds(setting);
  value = std::clamp(value, bounds.minimum, bounds.maximum);
  if (setting.GetType() == ControllerEmu::SettingType::Int)
    static_cast<NumericSetting<int>&>(setting).SetValue(static_cast<int>(std::lround(value)));
  else
    static_cast<NumericSetting<double>&>(setting).SetValue(value);
}
std::string SettingLabel(const ControllerEmu::NumericSettingBase& setting)
{
  auto text = QString::number(GetNumber(setting));
  if (const char* suffix = setting.GetUISuffix(); suffix && *suffix)
    text += QLatin1Char(' ') + BigPictureSettings::tr(suffix);
  return S(text);
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
  if (choice != state.descriptor.choices.end())
    return choice->label;
  if (state.descriptor.range)
  {
    const auto& range = *state.descriptor.range;
    const double stored = std::visit(
        [](const auto& v) -> double {
          if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>)
            return static_cast<double>(v);
          else
            return 0;
        },
        state.value);
    return S(QString::number(stored * range.scale + range.offset)) + range.unit;
  }
  if (state.descriptor.reverse && std::holds_alternative<bool>(state.value))
    return Display(!std::get<bool>(state.value));
  return Display(state.value);
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
                                       std::function<std::vector<std::string>()> devices,
                                       const ciface::Core::DeviceContainer* detection_source)
    : QObject(parent), m_targets(std::move(targets)), m_input_devices(std::move(devices)),
      m_changed(std::move(changed)),
      m_detection_source(detection_source ? detection_source : &g_controller_interface),
      m_worker("BigPicturePresets",
               [this](bool) {
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
               }),
      m_browse_worker("BigPictureSettingsBrowse", [this](BrowseJob job) {
        if (job.revision != m_browse_latest)
          return;
        std::function<bool(const QFileInfo&)> include_file;
        if (!job.folders_only)
          include_file = [](const QFileInfo&) { return true; };
        auto listing = BigPictureFiles::List(job.path, include_file, m_browse_cancelled);
        QueueOnObject(this, [this, revision = job.revision, listing = std::move(listing)] {
          if (revision != m_browse_revision)
            return;
          m_browse_listing = listing;
          m_browse_loading = false;
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
  m_browse_cancelled = true;
  m_browse_worker.StopAndCancel();
  m_worker.StopAndCancel();
}
void BigPictureSettings::Browse(std::string directory)
{
  m_browse_directory = std::move(directory);
  m_browse_listing = {};
  m_browse_loading = true;
  m_browse_latest = ++m_browse_revision;
  m_browse_worker.Push({m_browse_revision, m_browse_directory, m_browse_folders});
}
std::string BigPictureSettings::BrowseStartDirectory(const State& state) const
{
  // Start where the current value lives; fall back to the locations list.
  const auto* value = std::get_if<std::string>(&state.value);
  if (!value || value->empty())
    return {};
  const QFileInfo info(Q(*value));
  if (state.descriptor.directory && info.isDir())
    return S(info.absoluteFilePath());
  const QFileInfo parent(info.absolutePath());
  return parent.isDir() ? S(parent.absoluteFilePath()) : std::string{};
}
bool BigPictureSettings::Open(bool game)
{
  Q_ASSERT(QThread::currentThread() == thread());
  CancelCapture();
  m_service.SetContext({EditTarget::Base});
  if (!m_game.active)
    m_game_context = {};
  if (game && (!ValidGameId(m_game.id) || !m_service.SetContext(GameContext())))
    return false;
  m_page = Page::Home;
  m_category = game ? "Display" : "Interface";
  ++m_revision;
  return true;
}
void BigPictureSettings::SetGame(Game game)
{
  Q_ASSERT(QThread::currentThread() == thread());
  if (m_game == game)
    return;
  CancelCapture();
  m_service.SetContext({EditTarget::Base});
  m_game_context = {};
  m_game = std::move(game);
  m_page = Page::Home;
  m_category = "Interface";
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
  auto source = m_service.ReadSnapshot();
  if (source != m_source_snapshot)
  {
    m_translated_snapshot = ConfigWidget::TranslateSettingsSnapshot(*source);
    m_source_snapshot = std::move(source);
  }
  return m_translated_snapshot;
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
  result.push_back({"menu", S(tr("Big Picture Menu Controls")), BigPictureInput::GetConfig(), 0});
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
std::vector<BigPictureSettings::MappingControl>
BigPictureSettings::MappingControls(ControllerEmu::EmulatedController& controller) const
{
  std::vector<MappingControl> result;
  ForEachMappingGroup(
      controller, {}, {},
      [&](const std::string& key, const QString& section, ControllerEmu::ControlGroup& group) {
        for (const auto& control : group.controls)
        {
          if (!control->control_ref->IsInput())
            continue;
          result.push_back({key + "/" + control->name, S(section),
                            control->translate == ControllerEmu::Translatability::Translate ?
                                S(tr(control->ui_name.c_str())) :
                                control->ui_name,
                            control.get()});
        }
      });
  return result;
}
std::vector<BigPictureSettings::MappingSetting>
BigPictureSettings::MappingSettings(ControllerEmu::EmulatedController& controller) const
{
  std::vector<MappingSetting> result;
  ForEachMappingGroup(
      controller, {}, {},
      [&](const std::string& key, const QString& section, ControllerEmu::ControlGroup& group) {
        for (const auto& setting : group.numeric_settings)
        {
          // Advanced settings, such as calibration, remain desktop tuning.
          if (setting->GetVisibility() != ControllerEmu::SettingVisibility::Normal)
            continue;
          result.push_back({"set:" + key + "/" + setting->GetININame(), S(section),
                            S(tr(setting->GetUIName())), setting.get()});
        }
      });
  return result;
}
std::string BigPictureSettings::SaveProfile(const ControllerTarget& target, const std::string& name)
{
  const auto trimmed = S(Q(name).trimmed());
  // A plain file name in the profile folder, as the desktop profile box saves.
  if (trimmed.empty() || trimmed == "." || trimmed == ".." ||
      trimmed.find_first_of("/\\:*?\"<>|") != std::string::npos ||
      std::ranges::any_of(trimmed, [](unsigned char c) { return c < 0x20; }))
    return S(tr("Enter a profile name without / \\ : * ? \" < > |."));
  const auto path = target.config->GetUserProfileDirectoryPath() + trimmed + ".ini";
  const bool replaced = File::Exists(path);
  File::CreateFullPath(path);
  Common::IniFile ini;
  {
    auto* controller = target.config->GetController(target.index);
    const auto lock = controller->GetStateLock();
    controller->SaveConfig(ini.GetOrCreateSection("Profile"));
  }
  if (!ini.Save(path))
    return S(tr("Could not save the profile."));
  m_controller_message = S(replaced ? tr("Replaced profile %1.").arg(Q(trimmed)) :
                                      tr("Saved profile %1.").arg(Q(trimmed)));
  return {};
}
bool BigPictureSettings::StartCapture(const ControllerTarget& target, const std::string& key)
{
  auto* controller = target.config->GetController(target.index);
  const auto lock = controller->GetStateLock();
  const auto controls = MappingControls(*controller);
  if (std::ranges::find(controls, key, &MappingControl::key) == controls.end())
    return false;
  m_capture_device = m_detection_source->FindDevice(controller->GetDefaultDevice());
  if (!m_capture_device)
  {
    m_map_message = S(tr("The input device %1 is not connected.")
                          .arg(Q(controller->GetDefaultDevice().ToString())));
    return false;
  }
  // Detection never reports an input that is held when it starts, so wait until the press that
  // opened this page is released. Inputs that never rest only delay the start briefly.
  m_capture_release_deadline = std::chrono::steady_clock::now() + CAPTURE_RELEASE_WAIT;
  m_map_control = key;
  if (!m_capture_timer)
  {
    m_capture_timer = new QTimer(this);
    m_capture_timer->setInterval(16);
    connect(m_capture_timer, &QTimer::timeout, this, &BigPictureSettings::UpdateCapture);
  }
  m_capture_timer->start();
  return true;
}
void BigPictureSettings::UpdateCapture()
{
  if (!m_capture_device)
    return;
  const auto targets = Targets();
  const auto* target = Target(targets);
  if (!target || m_page != Page::Capture)
  {
    CancelCapture();
    return;
  }
  auto* controller = target->config->GetController(target->index);
  std::string bound;
  {
    const auto lock = controller->GetStateLock();
    if (!m_capture)
    {
      const bool held = std::ranges::any_of(m_capture_device->Inputs(), [](const auto* input) {
        return input->IsPressedForCapture();
      });
      if (held && std::chrono::steady_clock::now() < m_capture_release_deadline)
        return;
      const std::string device = controller->GetDefaultDevice().ToString();
      m_capture = std::make_unique<ciface::Core::InputDetector>();
      m_capture->Start(*m_detection_source, std::span(&device, 1));
      return;
    }
    m_capture->Update(CAPTURE_INITIAL_WAIT, {}, CAPTURE_MAXIMUM_WAIT);
    if (!m_capture->IsComplete())
      return;
    auto results = m_capture->TakeResults();
    m_capture.reset();
    m_capture_device.reset();
    const auto controls = MappingControls(*controller);
    const auto control = std::ranges::find(controls, m_map_control, &MappingControl::key);
    if (control != controls.end() && ciface::MappingCommon::ContainsCompleteDetection(results))
    {
      ciface::MappingCommon::RemoveSpuriousTriggerCombinations(&results);
      auto* reference = control->control->control_ref.get();
      reference->SetExpression(ciface::MappingCommon::BuildExpression(
          results, controller->GetDefaultDevice(), ciface::MappingCommon::Quote::On));
      controller->UpdateSingleControlReference(g_controller_interface, reference);
      bound = control->label;
    }
  }
  m_capture_timer->stop();
  if (!bound.empty())
  {
    target->config->SaveConfig();
    target->config->GenerateControllerTextures();
    m_map_message = S(tr("Bound %1.").arg(Q(bound)));
  }
  else
  {
    m_map_message = S(tr("No input was detected; the binding is unchanged."));
  }
  m_page = Page::Mapping;
  ++m_revision;
  if (m_changed)
    m_changed();
}
void BigPictureSettings::CancelCapture()
{
  if (!m_capture_device)
    return;
  m_capture.reset();
  m_capture_device.reset();
  m_capture_timer->stop();
  if (m_page == Page::Capture)
    m_page = Page::Mapping;
  ++m_revision;
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
  case Page::Text:
  case Page::Browse:
    m_page = Page::Edit;
    break;
  case Page::Devices:
  case Page::Profiles:
  case Page::Mapping:
    m_page = Page::Controller;
    break;
  case Page::Controller:
    m_page = Page::Controllers;
    break;
  case Page::Capture:
    CancelCapture();
    break;
  case Page::MapControl:
  case Page::MapDefaults:
  case Page::MapExtension:
  case Page::MapSetting:
    m_page = Page::Mapping;
    break;
  case Page::ProfileName:
    m_page = Page::Controller;
    break;
  default:
    m_page = Page::Home;
    break;
  }
}

std::string BigPictureSettings::MenuId(bool game) const
{
  std::string_view category, edit, controller, preset_folder;
  switch (m_page)
  {
  case Page::Presets:
    preset_folder = m_preset_folder;
    [[fallthrough]];
  case Page::Edit:
  case Page::Text:
  case Page::Browse:
    edit = m_edit;
    [[fallthrough]];
  case Page::Category:
    category = m_category;
    break;
  case Page::MapControl:
  case Page::Capture:
    edit = m_map_control;
    [[fallthrough]];
  case Page::MapSetting:
    if (m_page == Page::MapSetting)
      edit = m_map_setting;
    [[fallthrough]];
  case Page::MapExtension:
  case Page::ProfileName:
  case Page::Controller:
  case Page::Devices:
  case Page::Profiles:
  case Page::Mapping:
  case Page::MapDefaults:
    controller = m_controller;
    break;
  default:
    break;
  }
  // Each browsed folder keeps its own focus and scroll.
  if (m_page == Page::Browse)
    preset_folder = m_browse_directory;
  // Keep the existing ID format: retained focus and scroll depend on these stable keys.
  return fmt::format("settings-{}-{}-{}-{}-{}{}", static_cast<int>(m_page), category, edit,
                     controller, preset_folder, game ? m_game.id : "global");
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
  view->id = MenuId(game);
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
         (m_page == Page::Home || m_page == Page::Category || m_page == Page::Edit ||
          m_page == Page::Presets)) ||
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
  if (m_page != Page::Home && m_page != Page::Category && m_page != Page::Controllers)
    view->back = BigPicture::SettingsRequest{m_revision, O::Back};
  if (IsTextPage())
  {
    view->text_input = true;
    view->title = S(m_page == Page::ProfileName ? tr("Profile Name") : tr("Enter Value"));
    view->draft = m_draft;
    view->description += "\n" + m_text_error;
    nav("text-done", tr("Done"), O::TextDone, {}, {}, true, true);
    nav("text-erase", tr("Erase"), O::TextErase, {}, {}, true, true);
    nav("text-clear", tr("Clear"), O::TextClear, {}, {}, true, true);
    const std::string characters =
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/\\:._-@+~()[]{}!#$%&=,;' ";
    for (std::size_t i = 0; i < characters.size(); ++i)
      nav("text-key-" + std::to_string(i),
          characters[i] == ' ' ? tr("Space") : Q(std::string(1, characters[i])), O::TextKey,
          std::string(1, characters[i]));
    view->default_focus = "text-key-0";
    return view;
  }
  const std::pair<const char*, QString> categories[] = {
      {"Interface", tr("Interface Settings")},
      {"General", tr("General Settings")},
      {"Display", tr("Display Settings")},
      {"Enhancements", tr("Enhancements")},
      {"GraphicsHacks", tr("Graphics Hacks")},
      {"GraphicsAdvanced", tr("Advanced Graphics")},
      {"PostProcessing", tr("Post-processing Settings")},
      {"OSD", tr("On-Screen Display")},
      {"Audio", tr("Audio Settings")},
      {"GameCube", tr("GameCube Settings")},
      {"Wii", tr("Wii Settings")},
      {"Paths", tr("Folder Settings")},
      {"MemoryCards", tr("Memory Card Sync")},
      {"Achievements", tr("Achievements")},
      {"Advanced", tr("Advanced Settings")},
      {"Triforce", tr("Triforce Settings")}};
  const auto category_of = [](const State& state) {
    if (state.descriptor.id == "display.post_processing_preset")
      return std::string("PostProcessing");
    if (state.descriptor.id == "display.internal_resolution")
      return std::string("Enhancements");
    const auto& original =
        UICommon::Settings::GetCatalog().Find(state.descriptor.id)->descriptor.category;
    return original == "Graphics" ? std::string("GraphicsHacks") : original;
  };
  for (const auto& [category, label] : categories)
    if (std::ranges::any_of(values->settings, [&](const auto& state) {
          return category_of(state) == category && (!game || state.descriptor.per_game);
        }))
      nav(category, label, O::Category, category, {}, true, true);
  nav("controllers", tr("Controllers"), O::Controllers, {}, {}, true, true);
  if (m_page == Page::Home || m_page == Page::Category)
  {
    view->category_page = true;
    for (const auto& [id, label] : categories)
      if (m_category == id)
        view->title = S(label);
    view->preferences = m_category == "Interface" && !game;
    for (const auto& state : values->settings)
    {
      if (category_of(state) != m_category || (game && !state.descriptor.per_game))
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
      auto& row = view->rows.back();
      row.category = m_category;
      row.value = ValueLabel(state);
      row.help = state.descriptor.help;
      row.value_enabled = state.enabled;
      if (!state.value_available)
        row.help += "\n" + S(tr("Stored value unavailable"));
      if (game)
        row.help += "\n" + S(state.overridden ? tr("Override") : tr("Inherited"));
      if (!state.enabled)
        row.help += "\n" + state.disabled_reason;
      if (state.descriptor.control == Control::Toggle)
      {
        row.toggle = std::get<bool>(state.display_value);
        if (state.enabled)
          row.edit =
              Request{values->revision, state.descriptor.id, Operation::SetDisplay, !*row.toggle};
      }
      row.section =
          !state.descriptor.section.empty() ?
              state.descriptor.section :
              S(m_category == "Interface" ?
                    (state.descriptor.id == "interface.big_picture_theme" ? tr("Appearance") :
                     state.descriptor.id == "interface.start_big_picture" ? tr("Startup") :
                                                                            tr("Interface")) :
                m_category == "Audio"                                ? tr("Audio Output") :
                m_category == "PostProcessing"                       ? tr("Post-processing") :
                state.descriptor.id == "display.backend"             ? tr("Renderer") :
                state.descriptor.id == "display.internal_resolution" ? tr("Enhancements") :
                m_category == "Display"                              ? tr("Presentation") :
                                                                       Q(view->title));
    }
    if (m_category == "Interface")
      std::stable_sort(view->rows.begin(), view->rows.end(), [](const auto& a, const auto& b) {
        return a.id == "interface.big_picture_theme" && b.id != "interface.big_picture_theme";
      });
    if (!view->rows.empty())
      view->default_focus = view->rows.front().id;
  }
  else if (m_page == Page::Edit || m_page == Page::Presets || m_page == Page::Browse)
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
    if (m_page == Page::Browse)
    {
      view->description +=
          "\n" + (m_browse_directory.empty() ? S(tr("Locations")) :
                                               S(QDir::toNativeSeparators(Q(m_browse_directory))));
      if (m_browse_loading)
        view->description += "\n" + S(tr("Loading…"));
      if (!m_browse_listing.error.empty())
        view->description += "\n" + m_browse_listing.error;
      if (state.descriptor.directory && !m_browse_directory.empty() && !m_browse_loading)
        nav("browse-use", tr("Use This Folder"), O::BrowseChoose, m_browse_directory, {},
            state.enabled);
      if (!m_browse_listing.parent.empty())
        nav("browse-up", tr("Parent Folder"), O::BrowseEnter, m_browse_listing.parent);
      if (!m_browse_directory.empty())
        nav("browse-roots", tr("Locations"), O::BrowseEnter);
      for (const auto& file : m_browse_listing.files)
        nav("browse:" + file.path, Q(file.name), file.directory ? O::BrowseEnter : O::BrowseChoose,
            file.path, {}, file.directory || state.enabled);
      nav("enter-value", tr("Enter Path"), O::TextStart, {}, {}, state.enabled);
      // Choosing the open folder comes first; otherwise start on the first entry.
      const auto first = std::ranges::find_if(
          view->rows, [](const auto& row) { return row.id.starts_with("browse:"); });
      view->default_focus = !view->rows.empty() && view->rows.front().id == "browse-use" ?
                                "browse-use" :
                            first != view->rows.end() ? first->id :
                                                        view->rows.front().id;
    }
    else if (m_page == Page::Presets)
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
      else if (m_edit == "display.post_processing_preset")
      {
        nav("presets", tr("Choose preset"), O::PresetFolder, {}, state.disabled_reason,
            state.enabled);
        edit("off", tr("Off"), state, std::string{});
      }
      else if (state.descriptor.control == Control::Number)
      {
        if (!state.descriptor.ticks.empty())
        {
          for (std::size_t i = 0; i < state.descriptor.ticks.size(); ++i)
            edit("tick-" + std::to_string(i), Q(Display(state.descriptor.ticks[i])), state,
                 state.descriptor.ticks[i]);
        }
        else if (state.descriptor.range)
        {
          const auto& range = *state.descriptor.range;
          const double current = std::visit(
              [](const auto& v) -> double {
                if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>)
                  return static_cast<double>(v);
                else
                  return 0;
              },
              state.display_value);
          // Display units: stored * scale + offset. The step is one stored step.
          const double minimum = range.minimum * range.scale + range.offset;
          const double maximum = range.maximum * range.scale + range.offset;
          const double step = range.step * range.scale;
          BigPicture::SettingsSlider slider{minimum,
                                            maximum,
                                            step,
                                            current,
                                            S(QString::number(minimum)) + range.unit,
                                            S(QString::number(maximum)) + range.unit};
          const auto change = [&](double value) -> std::optional<BigPicture::SettingsAction> {
            value = std::clamp(value, minimum, maximum);
            if (!state.enabled || value == current)
              return std::nullopt;
            Request request{values->revision, state.descriptor.id, Operation::SetDisplay, false};
            request.display_value = value;
            return BigPicture::SettingsAction{{}, request};
          };
          slider.decrease = change(current - step);
          slider.increase = change(current + step);
          slider.decrease_large = change(current - step * 10);
          slider.increase_large = change(current + step * 10);
          if (state.enabled)
            slider.set.edit =
                Request{values->revision, state.descriptor.id, Operation::SetDisplay, false};
          view->rows.push_back({"value", state.descriptor.label,
                                state.enabled ? "" : state.disabled_reason, state.enabled});
          view->rows.back().value = ValueLabel(state);
          view->rows.back().slider = std::move(slider);
          nav("enter-value", tr("Enter Value"), O::TextStart, {}, {}, state.enabled);
          view->default_focus = "value";
        }
      }
      else if (state.descriptor.control == Control::Path)
      {
        nav("browse", state.descriptor.directory ? tr("Choose Folder") : tr("Choose File"),
            O::BrowseStart, {}, {}, state.enabled);
        nav("enter-value", tr("Enter Path"), O::TextStart, {}, {}, state.enabled);
        view->default_focus = "browse";
      }
      else if (state.descriptor.control == Control::Text)
        nav("enter-value", tr("Enter Value"), O::TextStart, {}, {}, state.enabled);
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
      view->description += "\n" + S(tr("Select an emulated controller to choose its device, "
                                       "profile and button mapping. Controller types and "
                                       "extensions are configured on the desktop."));
      for (const auto& candidate : targets)
      {
        // Game profiles apply to emulated controllers, not to the menu's own controls.
        if (game && candidate.config == BigPictureInput::GetConfig())
          continue;
        nav(candidate.id, Q(candidate.label), O::Controller, candidate.id);
      }
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
        if (!game)
        {
          nav("devices", tr("Input device"), O::Devices, {}, device, unavailable.empty());
          view->rows.back().value = device;
          view->rows.back().help = S(tr("Select the input device for this controller."));
          nav("mapping", tr("Mapping"), O::Mapping, {}, {}, unavailable.empty());
          view->rows.back().help =
              S(tr("Bind each button, stick and trigger by pressing an input."));
          nav("save-profile", tr("Save as profile"), O::SaveProfile, {}, {}, unavailable.empty());
          view->rows.back().help = S(tr("Save this controller's mapping as a named profile."));
          if (!m_controller_message.empty())
            view->description += "\n" + m_controller_message;
        }
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
        view->rows.back().value = profile_value.empty() ? S(tr("None")) : profile_value;
        view->rows.back().help = S(game ? tr("Choose a profile to apply at the next game start.") :
                                          tr("Load an existing controller profile."));
        if (game)
          nav("clear-profile", tr("Use inherited profile"), O::ClearProfile, {}, {},
              unavailable.empty() && m_game_context.layer->Exists(ProfileLocation(*target)));
      }
      else if (m_page == Page::Mapping || m_page == Page::MapControl ||
               m_page == Page::MapDefaults || m_page == Page::Capture ||
               m_page == Page::MapExtension || m_page == Page::MapSetting)
      {
        const auto lock = controller->GetStateLock();
        const auto controls = MappingControls(*controller);
        const auto settings = MappingSettings(*controller);
        auto* const attachments = FindAttachments(*controller);
        const auto selected = std::ranges::find(controls, m_map_control, &MappingControl::key);
        if (m_page == Page::Mapping)
        {
          view->title = S(tr("%1 Mapping").arg(Q(target->label)));
          view->description += "\n" + S(tr("Input device: %1").arg(Q(device)));
          if (!m_map_message.empty())
            view->description += "\n" + m_map_message;
          nav("map-defaults", tr("Default Mapping"), O::MapDefaults, {}, {}, unavailable.empty());
          if (attachments)
          {
            const auto& list = attachments->GetAttachmentList();
            const auto selected_extension = attachments->GetSelectedAttachment();
            nav("map-extension", tr("Extension"), O::MapExtensions, {},
                attachments->GetSelectionSetting().IsSimpleValue() ?
                    std::string{} :
                    S(tr("Set by an input expression; change it on the desktop.")),
                unavailable.empty() && attachments->GetSelectionSetting().IsSimpleValue());
            if (selected_extension < list.size())
              view->rows.back().value = S(tr(list[selected_extension]->GetDisplayName().c_str()));
          }
          for (const auto& control : controls)
          {
            nav("map:" + control.key, Q(control.label), O::MapBind, control.key, {},
                unavailable.empty());
            auto& row = view->rows.back();
            row.value = BindingLabel(control.control->control_ref->GetExpression());
            row.section = control.group;
            row.help = S(tr("Press to bind a new input. Options: clear the binding."));
            if (row.enabled)
              row.options = BigPicture::SettingsRequest{m_revision, O::MapOptions, control.key};
          }
          for (const auto& setting : settings)
          {
            const bool simple = setting.setting->IsSimpleValue();
            const bool toggle = setting.setting->GetType() == ControllerEmu::SettingType::Bool;
            nav(setting.key, Q(setting.label), toggle ? O::MapSettingToggle : O::MapSettingEdit,
                setting.key,
                simple ? std::string{} :
                         S(tr("Set by an input expression; change it on the desktop.")),
                unavailable.empty() && simple);
            auto& row = view->rows.back();
            row.section = setting.group;
            if (toggle)
              row.toggle = GetNumber(*setting.setting) != 0;
            else
              row.value = SettingLabel(*setting.setting);
            if (const char* description = setting.setting->GetUIDescription())
              row.help = S(tr(description));
          }
          if (!controls.empty())
            view->default_focus = "map:" + controls.front().key;
        }
        else if (m_page == Page::MapExtension && attachments)
        {
          view->title = S(tr("Extension"));
          const auto& list = attachments->GetAttachmentList();
          for (std::size_t i = 0; i < list.size(); ++i)
          {
            const auto id = "ext-" + std::to_string(i);
            nav(id, tr(list[i]->GetDisplayName().c_str()), O::MapExtension, std::to_string(i), {},
                unavailable.empty() && attachments->GetSelectionSetting().IsSimpleValue());
            view->rows.back().active = i == attachments->GetSelectedAttachment();
            if (view->rows.back().active)
              view->default_focus = id;
          }
        }
        else if (m_page == Page::MapSetting)
        {
          const auto setting = std::ranges::find(settings, m_map_setting, &MappingSetting::key);
          if (setting != settings.end())
          {
            const auto bounds = GetBounds(*setting->setting);
            const double current = GetNumber(*setting->setting);
            view->title = setting->label;
            view->description += "\n" + S(tr("Current value: %1 (%2 to %3)")
                                              .arg(Q(SettingLabel(*setting->setting)))
                                              .arg(bounds.minimum)
                                              .arg(bounds.maximum));
            std::string suffix;
            if (const char* ui_suffix = setting->setting->GetUISuffix(); ui_suffix && *ui_suffix)
              suffix = " " + S(tr(ui_suffix));
            BigPicture::SettingsSlider slider{bounds.minimum,
                                              bounds.maximum,
                                              bounds.step,
                                              current,
                                              S(QString::number(bounds.minimum)) + suffix,
                                              S(QString::number(bounds.maximum)) + suffix};
            const auto change = [&](int steps) -> std::optional<BigPicture::SettingsAction> {
              const double value =
                  std::clamp(current + steps * bounds.step, bounds.minimum, bounds.maximum);
              if (!unavailable.empty() || value == current)
                return std::nullopt;
              return BigPicture::SettingsAction{BigPicture::SettingsRequest{
                  m_revision, O::MapSettingStep, std::to_string(steps)}};
            };
            slider.decrease = change(-1);
            slider.increase = change(1);
            slider.decrease_large = change(-10);
            slider.increase_large = change(10);
            if (unavailable.empty())
              slider.set.navigation = BigPicture::SettingsRequest{m_revision, O::MapSettingSet};
            view->rows.push_back({"value", setting->label, unavailable, unavailable.empty()});
            view->rows.back().value = SettingLabel(*setting->setting);
            view->rows.back().slider = std::move(slider);
            nav("reset", tr("Reset to default"), O::MapSettingReset, {}, {}, unavailable.empty());
            view->default_focus = "value";
          }
        }
        else if (m_page == Page::MapDefaults)
        {
          view->title = S(tr("Default Mapping"));
          view->description +=
              "\n" + S(tr("Replace every binding of %1 with Dolphin's defaults? This also "
                          "selects the default input device.")
                           .arg(Q(target->label)));
          nav("map-defaults-cancel", tr("Cancel"), O::Back);
          nav("map-defaults-confirm", tr("Replace All Bindings"), O::MapDefaultsConfirm, {}, {},
              unavailable.empty());
          view->default_focus = "map-defaults-cancel";
        }
        else if (selected != controls.end())
        {
          const auto binding = BindingLabel(selected->control->control_ref->GetExpression());
          view->title = selected->label;
          if (m_page == Page::Capture)
          {
            view->capturing = true;
            view->title = S(tr("Press an Input"));
            view->description +=
                "\n" + S(tr("Press the button, key or direction for %1 on %2. Waiting ends after "
                            "a few seconds without input.")
                             .arg(Q(selected->label), Q(device)));
          }
          else
          {
            view->description += "\n" + S(tr("Current binding: %1").arg(Q(binding)));
            nav("map-bind", tr("Bind New Input"), O::MapBind, selected->key, {},
                unavailable.empty());
            nav("map-clear", tr("Clear Binding"), O::MapClear, selected->key, {},
                unavailable.empty() && !selected->control->control_ref->GetExpression().empty());
            view->default_focus = "map-bind";
          }
        }
      }
      else if (m_page == Page::Devices)
      {
        view->description += "\n" + device;
        if (std::ranges::find(m_devices, device) == m_devices.end())
          view->description +=
              "\n" + S(tr("Stored device is disconnected; it has been preserved."));
        for (const auto& available : m_devices)
        {
          nav("device-" + available, Q(available), O::Device, available, {}, unavailable.empty());
          view->rows.back().active = available == device;
        }
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
  {
    m_service.Save();
    emit SettingApplied(Q(request.id));
  }
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
    const auto same = [&](const std::optional<BigPicture::SettingsRequest>& published) {
      return published && published->operation == request.operation &&
             published->value == request.value;
    };
    const auto slider = [&](const BigPicture::SettingsSlider& published) {
      for (const auto* action : {&published.decrease, &published.increase,
                                 &published.decrease_large, &published.increase_large})
      {
        if (*action && same((*action)->navigation))
          return true;
      }
      return published.set.navigation && published.set.navigation->revision == request.revision &&
             published.set.navigation->operation == request.operation;
    };
    return row.enabled &&
           (same(row.navigation) || same(row.options) || (row.slider && slider(*row.slider)));
  };
  const bool text_key =
      IsTextPage() && request.operation == O::TextKey && request.value.size() <= 4096 &&
      request.value.find('\0') == std::string::npos && S(Q(request.value)) == request.value;
  if (!text_key && !(view->back && request.operation == O::Back) &&
      !std::ranges::any_of(view->rows, matches) && !std::ranges::any_of(view->toolbar, matches))
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
    m_controller_message.clear();
    m_page = Page::Controller;
    break;
  case O::SaveProfile:
    m_draft.clear();
    m_text_error.clear();
    m_page = Page::ProfileName;
    break;
  case O::MapExtensions:
    m_page = Page::MapExtension;
    break;
  case O::MapSettingEdit:
    m_map_setting = request.value;
    m_page = Page::MapSetting;
    break;
  case O::MapExtension:
  case O::MapSettingStep:
  case O::MapSettingSet:
  case O::MapSettingToggle:
  case O::MapSettingReset:
  {
    if (!target || !ControllerUnavailable().empty())
      return Status::Unavailable;
    auto* controller = target->config->GetController(target->index);
    {
      const auto lock = controller->GetStateLock();
      if (request.operation == O::MapExtension)
      {
        auto* attachments = FindAttachments(*controller);
        const auto index = Q(request.value).toUInt();
        if (!attachments || index >= attachments->GetAttachmentList().size())
          return Status::InvalidValue;
        attachments->SetSelectedAttachment(index);
        m_map_message =
            S(tr("Extension: %1")
                  .arg(tr(attachments->GetAttachmentList()[index]->GetDisplayName().c_str())));
        m_page = Page::Mapping;
      }
      else
      {
        const auto settings = MappingSettings(*controller);
        const auto key = request.operation == O::MapSettingToggle ? request.value : m_map_setting;
        const auto setting = std::ranges::find(settings, key, &MappingSetting::key);
        if (setting == settings.end() || !setting->setting->IsSimpleValue())
          return Status::InvalidValue;
        auto& numeric = *setting->setting;
        if (request.operation == O::MapSettingReset)
          numeric.SetToDefault();
        else if (request.operation == O::MapSettingToggle)
          static_cast<ControllerEmu::NumericSetting<bool>&>(numeric).SetValue(GetNumber(numeric) ==
                                                                              0);
        else if (request.operation == O::MapSettingSet)
        {
          bool valid = false;
          const double value = Q(request.value).toDouble(&valid);
          if (!valid || !std::isfinite(value))
            return Status::InvalidValue;
          SetNumber(numeric, value);
        }
        else
          SetNumber(numeric,
                    GetNumber(numeric) + Q(request.value).toInt() * GetBounds(numeric).step);
      }
    }
    target->config->SaveConfig();
    break;
  }
  case O::Devices:
    m_page = Page::Devices;
    break;
  case O::Profiles:
    m_page = Page::Profiles;
    break;
  case O::Mapping:
    m_map_message.clear();
    m_page = Page::Mapping;
    break;
  case O::MapOptions:
    m_map_control = request.value;
    m_page = Page::MapControl;
    break;
  case O::MapDefaults:
    m_page = Page::MapDefaults;
    break;
  case O::MapBind:
    if (!target || !ControllerUnavailable().empty())
      return Status::Unavailable;
    // A disconnected device reports why on the mapping page instead.
    m_page = StartCapture(*target, request.value) ? Page::Capture : Page::Mapping;
    break;
  case O::MapClear:
  case O::MapDefaultsConfirm:
  {
    if (!target || !ControllerUnavailable().empty())
      return Status::Unavailable;
    auto* controller = target->config->GetController(target->index);
    if (request.operation == O::MapDefaultsConfirm)
    {
      // As the desktop mapping window's Default button.
      controller->LoadDefaults(g_controller_interface);
      controller->UpdateReferences(g_controller_interface);
      m_map_message = S(tr("Restored the default mapping."));
    }
    else
    {
      const auto lock = controller->GetStateLock();
      const auto controls = MappingControls(*controller);
      const auto control = std::ranges::find(controls, request.value, &MappingControl::key);
      if (control == controls.end())
        return Status::InvalidValue;
      control->control->control_ref->SetExpression({});
      controller->UpdateSingleControlReference(g_controller_interface,
                                               control->control->control_ref.get());
      m_map_message = S(tr("Cleared %1.").arg(Q(control->label)));
    }
    target->config->SaveConfig();
    target->config->GenerateControllerTextures();
    m_page = Page::Mapping;
    break;
  }
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
  case O::BrowseStart:
  case O::BrowseChoose:
  {
    const auto values = ReadSnapshot();
    const auto state =
        std::ranges::find(values->settings, m_edit, [](const auto& s) { return s.descriptor.id; });
    if (state == values->settings.end() || !state->enabled ||
        state->descriptor.control != Control::Path)
      return Status::Unavailable;
    if (request.operation == O::BrowseStart)
    {
      m_browse_folders = state->descriptor.directory;
      Browse(BrowseStartDirectory(*state));
      m_page = Page::Browse;
      break;
    }
    // The listing may be stale; the choice must still be the right kind of path.
    const QFileInfo chosen(Q(request.value));
    if (state->descriptor.directory ? !chosen.isDir() : !chosen.isFile())
      return Status::InvalidValue;
    // Stored like the desktop path pickers store it.
    const auto result = Apply({values->revision, m_edit, Operation::Set,
                               S(QDir::toNativeSeparators(chosen.absoluteFilePath()))});
    if (result.status != Status::Success)
      return result.status;
    m_page = Page::Edit;
    break;
  }
  case O::BrowseEnter:
    Browse(request.value);
    break;
  case O::TextStart:
  {
    const auto values = ReadSnapshot();
    const auto state =
        std::ranges::find(values->settings, m_edit, [](const auto& s) { return s.descriptor.id; });
    if (state == values->settings.end() || !state->enabled)
      return Status::Unavailable;
    m_draft = Display(state->display_value);
    m_text_error.clear();
    m_page = Page::Text;
    break;
  }
  case O::TextKey:
    if (!IsTextPage() || m_draft.size() + request.value.size() > 4096)
      return Status::InvalidValue;
    m_draft += request.value;
    m_text_error.clear();
    return Status::Success;
  case O::TextErase:
  {
    auto text = Q(m_draft);
    if (!text.isEmpty())
    {
      const int count = text.size() >= 2 && text.at(text.size() - 1).isLowSurrogate() &&
                                text.at(text.size() - 2).isHighSurrogate() ?
                            2 :
                            1;
      text.chop(count);
      m_draft = S(text);
    }
    m_text_error.clear();
    return Status::Success;
  }
  case O::TextClear:
    m_draft.clear();
    m_text_error.clear();
    return Status::Success;
  case O::TextDone:
  {
    if (m_page == Page::ProfileName)
    {
      if (!target || !ControllerUnavailable().empty())
        return Status::Unavailable;
      m_text_error = SaveProfile(*target, m_draft);
      if (m_text_error.empty())
        m_page = Page::Controller;
      break;
    }
    const auto values = ReadSnapshot();
    const auto state =
        std::ranges::find(values->settings, m_edit, [](const auto& s) { return s.descriptor.id; });
    if (state == values->settings.end() || !state->enabled)
      return Status::Unavailable;
    Request edit_request{values->revision, m_edit};
    if (state->descriptor.control == Control::Number)
    {
      bool valid = false;
      edit_request.display_value = Q(m_draft).toDouble(&valid);
      if (!valid || !std::isfinite(edit_request.display_value))
      {
        m_text_error = S(tr("Enter a valid number."));
        return Status::Success;
      }
      edit_request.operation = Operation::SetDisplay;
    }
    else
      edit_request.value = m_draft;
    const auto result = Apply(edit_request);
    if (result.status != Status::Success)
    {
      m_text_error = S(tr("The value is not valid for this setting."));
      return Status::Success;
    }
    m_page = Page::Edit;
    m_text_error.clear();
    break;
  }
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
