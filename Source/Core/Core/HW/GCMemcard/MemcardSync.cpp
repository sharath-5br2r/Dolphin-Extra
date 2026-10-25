// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/MemcardSync.h"

#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "Common/Config/Config.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"

#include "Core/Config/MemoryCardSyncSettings.h"

namespace MemcardSync
{
// ---- Common to both builds ---------------------------------------------------------------

void WipeSecret(std::string& secret)
{
  // Through a volatile pointer, so the stores can't be dropped as dead before the release.
  volatile char* p = secret.data();
  for (size_t i = 0; i < secret.size(); i++)
    p[i] = 0;
  secret.clear();
  secret.shrink_to_fit();
}

std::string DataDirFor(const std::string& remote_uri)
{
  // emusync binds a data folder to one storage for good: one folder per address, so switching to
  // another storage (a new folder for encrypted saves, say) starts a fresh one, and switching back
  // finds the old one again. SHA-1 only names the folder; nothing depends on it being secure.
  const auto digest = Common::SHA1::CalculateDigest(remote_uri);
  std::string id;
  for (size_t i = 0; i < 8; i++)
    id += fmt::format("{:02x}", digest[i]);
  return File::GetUserPath(D_USER_IDX) + "emusync/" + id;
}

bool HasSecret(const char* name)
{
  const std::string_view n = name;
  if (n == SECRET_ACCESS_KEY)
    return ::Config::Get(::Config::MAIN_MEMCARD_SYNC_SECRET_STORED);
  if (n == ENCRYPTION_PASSPHRASE)
    return ::Config::Get(::Config::MAIN_MEMCARD_SYNC_PASSPHRASE_STORED);
  return false;
}
}  // namespace MemcardSync

#ifndef HAVE_EMUSYNC

namespace MemcardSync
{
static constexpr const char* NO_SYNC = "This build of Dolphin has no memory card sync.";

bool IsAvailable()
{
  return false;
}
bool Start(const Config&, std::string* error)
{
  if (error)
    *error = NO_SYNC;
  return false;
}
void Stop(u32)
{
}
bool IsRunning()
{
  return false;
}
std::string GetStatus()
{
  return {};
}
void SyncNow()
{
}
void SetAppBackgrounded(bool)
{
}
bool StoreSecret(const char*, const std::string&, std::string* error)
{
  if (error)
    *error = NO_SYNC;
  return false;
}
std::optional<std::string> LoadSecret(const char*, std::string* error)
{
  if (error)
    *error = NO_SYNC;
  return std::nullopt;
}
void ApplySettingsAsync(std::vector<SecretUpdate>, std::function<void(const std::string&)> done)
{
  if (done)
    done(NO_SYNC);
}
std::string GetLastStartError()
{
  return {};
}
void Initialize()
{
}
void Shutdown()
{
}
void OnEmulationStarting(Core::System&)
{
}
void OnCardWritten(const std::string&)
{
}
void OnEmulationStopped()
{
}
}  // namespace MemcardSync

#else

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <emusync.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"
#include "DiscIO/Enums.h"

namespace MemcardSync
{
// How long boot waits for the network before the game starts with the cards as they are.
static constexpr u32 BOOT_SYNC_TIMEOUT_MS = 3000;
// The session timeout for "don't wait": 0 would mean the default.
static constexpr u32 NO_WAIT_MS = 1;
// A sync with no event to trigger it.
static constexpr std::chrono::minutes PERIODIC_SYNC{5};
// What exiting Dolphin gives queued uploads.
static constexpr u32 EXIT_UPLOAD_MS = 5000;
// How long a boot waits for a start in progress: the start's own check of the storage takes up to
// ~10 s.
static constexpr std::chrono::seconds BOOT_WAITS_FOR_START{12};
// What going to the background gives uploads (Android).
static constexpr u32 SUSPEND_UPLOAD_MS = 3000;
// The credential-store service the secrets are kept under.
static constexpr const char* SECRET_SERVICE = "Dolphin";
// Longer than any secret emusync stores (1024 bytes), plus the NUL.
static constexpr size_t SECRET_BUFFER = 1025;
// The regions whose cards are synced, in the order they are registered.
static constexpr std::array<DiscIO::Region, 3> REGIONS = {
    DiscIO::Region::NTSC_U, DiscIO::Region::PAL, DiscIO::Region::NTSC_J};

struct Container
{
  es_container_id id = 0;
  // The file (raw card) or folder (GCI folder) Dolphin opens, as Config::GetMemcardPath and
  // Config::GetGCIFolderPath give it, without a trailing separator.
  std::string path;
};

// Serialises Start() and Stop() with each other.
static std::mutex s_lifecycle_lock;
// The client's lifetime. Every es_* call holds it shared; Start() and Stop() hold it unique, so the
// client is never destroyed under a call. The longest shared hold is the boot sync (3 s).
static std::shared_mutex s_client_lock;
static es_client* s_client = nullptr;

// Everything else, held briefly.
static std::mutex s_state_lock;
static Config s_config;
static std::vector<Container> s_containers;
static es_session_id s_session = 0;
static std::chrono::steady_clock::time_point s_session_started;
// A game is running (from OnEmulationStarting to OnEmulationStopped), and its region.
static bool s_emulating = false;
static DiscIO::Region s_emulating_region = DiscIO::Region::Unknown;
static std::string s_status;
static std::string s_last_start_error;

// The worker thread: periodic syncs, uploads after card writes, the app's background moves, and
// draining emusync's events. Card writes and background moves are handed to it so the threads
// reporting them never wait for emusync.
static std::thread s_worker;
static std::mutex s_worker_lock;
static std::condition_variable s_worker_cv;
static bool s_worker_stop = false;
static bool s_worker_wake = false;
static std::vector<es_container_id> s_flush_queue;
static std::optional<bool> s_background_change;

// The apply thread (ApplySettingsAsync): starts and stops the client off the UI and emulation
// threads.
static std::thread s_apply_thread;
static std::mutex s_apply_lock;
static std::condition_variable s_apply_cv;
static bool s_apply_pending = false;
// A run of the apply thread is in progress (s_apply_pending is cleared when it begins).
static bool s_apply_running = false;
static std::condition_variable s_apply_idle_cv;
static bool s_apply_quit = false;
static std::vector<SecretUpdate> s_apply_secrets;
static std::vector<std::function<void(const std::string&)>> s_apply_done;

static void SetStatus(std::string status)
{
  std::lock_guard lock(s_state_lock);
  s_status = std::move(status);
}

static std::string TimeOfDay()
{
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buf[16];
  std::strftime(buf, sizeof(buf), "%H:%M", &local);
  return buf;
}

static void Log(void*, es_log_level level, const char*, const char* msg)
{
  switch (level)
  {
  case ES_LOG_ERROR:
    ERROR_LOG_FMT(CORE, "MemcardSync: {}", msg);
    break;
  case ES_LOG_WARN:
    WARN_LOG_FMT(CORE, "MemcardSync: {}", msg);
    break;
  case ES_LOG_INFO:
    INFO_LOG_FMT(CORE, "MemcardSync: {}", msg);
    break;
  default:
    DEBUG_LOG_FMT(CORE, "MemcardSync: {}", msg);
    break;
  }
}

static void CopyTo(char* dst, size_t cap, const std::string& src)
{
  const size_t n = std::min(src.size(), cap - 1);
  std::memcpy(dst, src.data(), n);
  dst[n] = '\0';
}

// The credential provider: the keys from Start(), kept in memory only.
static int GetCredentials(void*, const char*, es_credentials* out)
{
  std::lock_guard lock(s_state_lock);
  if (s_config.access_key_id.empty() || s_config.secret_access_key.empty())
    return ES_E_AUTH;
  out->kind = ES_CRED_S3;
  CopyTo(out->access_key_id, sizeof(out->access_key_id), s_config.access_key_id);
  CopyTo(out->secret_access_key, sizeof(out->secret_access_key), s_config.secret_access_key);
  return 0;
}

static void WakeWorker()
{
  {
    std::lock_guard lock(s_worker_lock);
    s_worker_wake = true;
  }
  s_worker_cv.notify_one();
}

// Called by emusync on its own thread when events are waiting; must not call into emusync.
static void OnEventsWaiting(void*)
{
  WakeWorker();
}

static std::string WithoutTrailingSeparator(std::string path)
{
  while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
    path.pop_back();
  return path;
}

// The size of the cards Dolphin creates, as ExpansionInterfaceManager::Init works it out: raw
// cards of other sizes have it in their file name.
static u16 MemoryCardSizeMbits()
{
  const int size_override = ::Config::Get(::Config::MAIN_MEMORY_CARD_SIZE);
  if (size_override >= 0 && size_override <= 4)
    return Memcard::MBIT_SIZE_MEMORY_CARD_59 << size_override;
  return Memcard::MBIT_SIZE_MEMORY_CARD_2043;
}

// Registers the slot A and B cards that are synced: for each slot holding a raw card or a GCI
// folder, each region's card that exists, and `booting`'s even if it doesn't (it is created for
// the saves from other devices). A card is "MemoryCard<slot>.<region>" whichever kind the slot
// holds; a slot switched to the other kind moves its card (ES_CF_REPLACE). Needs the client lock
// (shared), and no open session.
static std::vector<Container> RegisterCards(std::optional<DiscIO::Region> booting)
{
  std::vector<Container> out;
  const u16 size_mbits = MemoryCardSizeMbits();
  for (const ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    const ExpansionInterface::EXIDeviceType device =
        ::Config::Get(::Config::GetInfoForEXIDevice(slot));
    const bool raw = device == ExpansionInterface::EXIDeviceType::MemoryCard;
    if (!raw && device != ExpansionInterface::EXIDeviceType::MemoryCardFolder)
      continue;
    for (const DiscIO::Region region : REGIONS)
    {
      const std::string path =
          raw ? ::Config::GetMemcardPath(slot, region, size_mbits) :
                WithoutTrailingSeparator(::Config::GetGCIFolderPath(slot, region));
      const bool exists = raw ? File::IsFile(path) : File::IsDirectory(path);
      if (!exists && booting != region)
        continue;
      const std::string card =
          fmt::format("MemoryCard{}.{}", slot == ExpansionInterface::Slot::A ? 'A' : 'B',
                      ::Config::GetDirectoryForRegion(::Config::ToGameCubeRegion(region)));
      es_container_desc desc = ES_CONTAINER_DESC_INIT;
      desc.format = raw ? "gc.card" : "gc.gci";
      desc.location = path.c_str();
      desc.platform = "gc";
      desc.card = card.c_str();
      desc.flags = ES_CF_REPLACE;
      es_container_id id = 0;
      const es_status st = es_container_add(s_client, &desc, &id);
      if (st != ES_OK)
      {
        WARN_LOG_FMT(CORE, "MemcardSync: not syncing {}: {}", path, es_status_str(st));
        continue;
      }
      out.push_back({id, path});
    }
  }
  return out;
}

// Begins a session on `containers`. Needs the client lock (shared). The state lock must NOT be
// held: this can wait for the network.
static es_session_id BeginSession(const std::vector<Container>& containers, const char* platform,
                                  const std::string& game_id, const std::string& title_name,
                                  u32 timeout_ms, es_session_info* info)
{
  std::vector<es_container_id> ids;
  for (const Container& c : containers)
    ids.push_back(c.id);
  if (ids.empty())
    return 0;

  es_title title = ES_TITLE_INIT;
  title.platform = platform;
  // The game ID ("GM8E01", the game and maker codes) picks which saves are fetched first: their
  // title keys are "gc:GM8E01". The IPL and homebrew have none.
  title.serial = game_id.empty() ? "IPL" : game_id.c_str();
  title.display_name = title_name.empty() ? nullptr : title_name.c_str();

  es_session_begin_args args = ES_SESSION_BEGIN_INIT;
  args.title = &title;
  args.mounted = ids.data();
  args.mounted_count = ids.size();
  args.timeout_ms = timeout_ms;
  *info = ES_SESSION_INFO_INIT;
  const es_status st = es_session_begin(s_client, &args, info);
  if (st < 0)
  {
    WARN_LOG_FMT(CORE, "MemcardSync: could not start syncing the cards in use: {}",
                 es_status_str(st));
    return 0;
  }
  return info->id;
}

// Ends the open session, if any. Needs the client lock (shared).
static void EndSession()
{
  es_session_id id;
  u64 played_ms;
  {
    std::lock_guard lock(s_state_lock);
    id = s_session;
    s_session = 0;
    played_ms = static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - s_session_started)
                                     .count());
  }
  if (id != 0)
    es_session_end(s_client, id, played_ms);
}

// Starts following the cards of a running game, when sync starts mid-game. Never waits for the
// network (the game is running).
static void FollowRunningGame()
{
  DiscIO::Region region;
  {
    std::lock_guard lock(s_state_lock);
    region = s_emulating_region;
  }
  EndSession();
  std::vector<Container> containers = RegisterCards(region);
  es_session_info info;
  // 1 ms: the cards are marked in use at once; what the network brings waits for the game to end.
  const es_session_id id = BeginSession(containers, "gc", SConfig::GetInstance().GetGameID(),
                                        SConfig::GetInstance().GetTitleName(), NO_WAIT_MS, &info);
  std::lock_guard lock(s_state_lock);
  s_containers = std::move(containers);
  s_session = id;
  s_session_started = std::chrono::steady_clock::now();
}

static constexpr const char* LOCKED_STATUS =
    "This storage is encrypted. Enter its passphrase in the Memory Card Sync settings to sync.";

static std::string DescribeEvent(const es_event& ev)
{
  const char* save = ev.unit_name ? ev.unit_name : "a save";
  if (ev.status == ES_E_LOCKED && (ev.type == ES_EV_SYNC_FINISHED || ev.type == ES_EV_ERROR))
    return LOCKED_STATUS;
  switch (ev.type)
  {
  case ES_EV_SYNC_FINISHED:
    if (ev.status == ES_OK)
      return fmt::format("Synced at {}.", TimeOfDay());
    if (ev.status == ES_OFFLINE)
      return fmt::format("Offline at {}: changes upload once the storage can be reached.",
                         TimeOfDay());
    return fmt::format("Sync failed at {}: {}", TimeOfDay(),
                       ev.message ? ev.message : es_status_str(ev.status));
  case ES_EV_UNIT_APPLIED:
    return fmt::format("Updated {}{} at {}.", save,
                       ev.device_name ? fmt::format(" from {}", ev.device_name) : std::string(),
                       TimeOfDay());
  case ES_EV_APPLY_BLOCKED:
    if (ev.status == ES_E_NO_SPACE)
      return fmt::format("{} does not fit on the card. Free some space on it.", save);
    if (ev.status == ES_E_BUSY)
      return {};  // a card in use; it is applied when the game ends
    return fmt::format("Could not update {}: {}", save,
                       ev.message ? ev.message : es_status_str(ev.status));
  case ES_EV_AUTH_REQUIRED:
    return "The storage rejected the access key. Check it in the Memory Card Sync settings.";
  case ES_EV_ERROR:
    return fmt::format("Sync error: {}", ev.message ? ev.message : es_status_str(ev.status));
  default:
    return {};
  }
}

// Drains emusync's events into the log and the status line. Needs the client lock (shared).
static void DrainEvents()
{
  es_event ev = ES_EVENT_INIT;
  while (es_event_poll(s_client, &ev) == 1)
  {
    std::string text = DescribeEvent(ev);
    if (ev.type == ES_EV_UNIT_APPLIED || ev.type == ES_EV_UNIT_UPLOADED)
    {
      INFO_LOG_FMT(CORE, "MemcardSync: {} {}",
                   ev.type == ES_EV_UNIT_APPLIED ? "applied" : "uploaded",
                   ev.unit_id ? ev.unit_id : "?");
    }
    else if (!text.empty() && ev.type != ES_EV_SYNC_FINISHED)
    {
      WARN_LOG_FMT(CORE, "MemcardSync: {}", text);
    }
    if (!text.empty())
      SetStatus(std::move(text));
    es_event_release(s_client, &ev);
    ev = ES_EVENT_INIT;
  }
}

static void WorkerLoop()
{
  auto next_sync = std::chrono::steady_clock::now() + PERIODIC_SYNC;
  for (;;)
  {
    std::vector<es_container_id> flushed;
    std::optional<bool> background;
    {
      std::unique_lock lock(s_worker_lock);
      s_worker_cv.wait_until(lock, next_sync, [] { return s_worker_stop || s_worker_wake; });
      if (s_worker_stop)
        return;
      s_worker_wake = false;
      flushed = std::exchange(s_flush_queue, {});
      background = std::exchange(s_background_change, std::nullopt);
    }

    std::shared_lock client(s_client_lock);
    if (!s_client)
      return;
    for (const es_container_id id : flushed)
      es_container_flushed(s_client, id);
    if (background.has_value())
    {
      if (*background)
        es_app_suspend(s_client, SUSPEND_UPLOAD_MS);
      else
        es_app_resume(s_client);
    }
    if (std::chrono::steady_clock::now() >= next_sync)
    {
      es_sync_now(s_client);
      next_sync = std::chrono::steady_clock::now() + PERIODIC_SYNC;
    }
    DrainEvents();
  }
}

// Stops the worker and destroys the client. Needs the lifecycle lock and the client lock (unique).
static void StopLocked(u32 timeout_ms)
{
  if (!s_client)
    return;
  {
    std::lock_guard lock(s_worker_lock);
    s_worker_stop = true;
  }
  s_worker_cv.notify_one();
  // The worker takes the client lock shared, which this thread holds unique: release it while the
  // worker winds down, so a worker blocked on it can see the stop flag. Nothing else can start or
  // stop the client meanwhile: the lifecycle lock is held.
  s_client_lock.unlock();
  if (s_worker.joinable())
    s_worker.join();
  s_client_lock.lock();

  es_event_set_notifier(s_client, nullptr, nullptr);
  es_client_destroy(s_client, timeout_ms);
  s_client = nullptr;

  {
    std::lock_guard lock(s_worker_lock);
    s_flush_queue.clear();
    s_background_change.reset();
  }
  std::lock_guard lock(s_state_lock);
  s_session = 0;
  s_containers.clear();
  WipeSecret(s_config.secret_access_key);
  s_config = {};
  s_status.clear();
}

bool IsAvailable()
{
  return true;
}

bool Start(const Config& config, std::string* error)
{
  std::lock_guard lifecycle(s_lifecycle_lock);
  std::unique_lock client(s_client_lock);
  StopLocked(2000);

  {
    std::lock_guard lock(s_state_lock);
    s_config = config;
    // Only needed below, once: not kept for the life of the client like the keys.
    WipeSecret(s_config.passphrase);
  }

  es_config cfg = ES_CONFIG_INIT;
  cfg.data_dir = config.data_dir.c_str();
  cfg.device_name = config.device_name.empty() ? nullptr : config.device_name.c_str();
  cfg.emulator_id = "dolphin";
  cfg.emulator_version = config.emulator_version.c_str();
  // No conflict screen yet: the newest version wins and the other stays in emusync's history.
  cfg.conflict_policy = ES_POLICY_NEWEST;
  cfg.log = &Log;
  cfg.log_level = ES_LOG_INFO;
  es_status st = es_client_create(&cfg, &s_client);
  if (st != ES_OK)
  {
    if (error)
    {
      *error =
          fmt::format("Could not open the sync data in {}: {}", config.data_dir, es_status_str(st));
    }
    s_client = nullptr;
    // StopLocked() returns early without a client, so this is the only place the copied keys can
    // go.
    std::lock_guard lock(s_state_lock);
    WipeSecret(s_config.secret_access_key);
    s_config = {};
    return false;
  }

  const bool encrypted = !config.passphrase.empty();
  // Applies to the es_remote_add below: create the storage encrypted, or unlock it.
  if (encrypted)
    st = es_repo_create_encrypted(s_client, config.passphrase.c_str());
  const es_credential_provider creds = {nullptr, &GetCredentials};
  if (st == ES_OK)
    st = es_remote_add(s_client, config.remote_uri.c_str(), &creds);
  // Locked: encrypted storage this computer has no key for, and no passphrase was given. The remote
  // is added; sync runs and snapshots the cards, and uploads once it is unlocked.
  if (st != ES_OK && st != ES_OFFLINE && st != ES_E_LOCKED)
  {
    if (error)
    {
      if (st == ES_E_AUTH)
        *error = "The storage rejected the access key ID or secret.";
      else if (st == ES_E_PASSPHRASE)
        *error = "The passphrase does not unlock this storage.";
      else if (st == ES_E_INCOMPATIBLE && encrypted)
        *error = "This storage is not encrypted, or was set up by a newer version of emusync. "
                 "Remove the passphrase to use it, or choose a new folder for encrypted saves.";
      else if (st == ES_E_INCOMPATIBLE)
        *error = "This storage was set up by a newer version of emusync, or has changed since "
                 "this computer joined it.";
      else if (st == ES_E_INVALID_ARG)
        *error = "The storage address is not valid.";
      else
        *error = fmt::format("Could not use the storage: {}", es_status_str(st));
    }
    es_client_destroy(s_client, 0);
    s_client = nullptr;
    std::lock_guard lock(s_state_lock);
    WipeSecret(s_config.secret_access_key);
    s_config = {};
    return false;
  }
  SetStatus(st == ES_OFFLINE  ? "Storage not reachable yet: sync starts once it is." :
            st == ES_E_LOCKED ? std::string(LOCKED_STATUS) :
                                "Starting...");

  {
    std::lock_guard lock(s_worker_lock);
    s_worker_stop = false;
    s_worker_wake = false;
  }
  es_event_set_notifier(s_client, &OnEventsWaiting, nullptr);
  s_worker = std::thread(&WorkerLoop);

  // A game already running keeps its cards: follow them without touching them. One starting counts
  // too: OnEmulationStarting() may have run before this client existed.
  bool emulating;
  {
    std::lock_guard lock(s_state_lock);
    emulating = s_emulating;
  }
  if (emulating)
  {
    FollowRunningGame();
  }
  else
  {
    std::vector<Container> containers = RegisterCards(std::nullopt);
    std::lock_guard lock(s_state_lock);
    s_containers = std::move(containers);
  }
  es_sync_now(s_client);
  NOTICE_LOG_FMT(CORE, "MemcardSync: syncing memory cards with {}", config.remote_uri);
  return true;
}

void Stop(u32 timeout_ms)
{
  std::lock_guard lifecycle(s_lifecycle_lock);
  std::unique_lock client(s_client_lock);
  StopLocked(timeout_ms);
}

bool IsRunning()
{
  std::shared_lock client(s_client_lock);
  return s_client != nullptr;
}

std::string GetStatus()
{
  std::lock_guard lock(s_state_lock);
  return s_status;
}

void SyncNow()
{
  std::shared_lock client(s_client_lock);
  if (s_client)
    es_sync_now(s_client);
}

void SetAppBackgrounded(bool backgrounded)
{
  {
    std::lock_guard lock(s_worker_lock);
    s_background_change = backgrounded;
  }
  WakeWorker();
}

// ---- Settings ---------------------------------------------------------------------------

bool StoreSecret(const char* name, const std::string& value, std::string* error)
{
  // Without a Secret Service (Linux), the secret goes to a file only this user can read here.
  const std::string fallback = File::GetUserPath(D_USER_IDX);
  const es_status st = value.empty() ?
                           es_secret_delete(SECRET_SERVICE, name, fallback.c_str()) :
                           es_secret_set(SECRET_SERVICE, name, value.c_str(), fallback.c_str());
  if (st != ES_OK)
  {
    if (error)
    {
      *error = fmt::format("Could not store the secret in the system's credential store: {}",
                           es_status_str(st));
    }
    return false;
  }
  const std::string_view n = name;
  if (n == SECRET_ACCESS_KEY)
    ::Config::SetBase(::Config::MAIN_MEMCARD_SYNC_SECRET_STORED, !value.empty());
  else if (n == ENCRYPTION_PASSPHRASE)
    ::Config::SetBase(::Config::MAIN_MEMCARD_SYNC_PASSPHRASE_STORED, !value.empty());
  return true;
}

std::optional<std::string> LoadSecret(const char* name, std::string* error)
{
  std::string buf(SECRET_BUFFER, '\0');
  size_t len = 0;
  const std::string fallback = File::GetUserPath(D_USER_IDX);
  const es_status st =
      es_secret_get(SECRET_SERVICE, name, fallback.c_str(), buf.data(), buf.size(), &len);
  if (st == ES_E_NOT_FOUND)
  {
    WipeSecret(buf);
    return std::nullopt;
  }
  if (st != ES_OK)
  {
    WipeSecret(buf);
    if (error)
    {
      *error = fmt::format("Could not read the secret from the system's credential store: {}",
                           es_status_str(st));
    }
    return std::nullopt;
  }
  std::string value(buf.c_str());
  WipeSecret(buf);
  return value;
}

std::string GetLastStartError()
{
  std::lock_guard lock(s_state_lock);
  return s_last_start_error;
}

// Starts sync with the settings, or stops it when it is switched off or incomplete. Returns an
// empty string when sync runs or is deliberately off, else a sentence for the user.
static std::string StartFromSettings()
{
  Config c;
  c.remote_uri =
      std::string(StripWhitespace(::Config::Get(::Config::MAIN_MEMCARD_SYNC_REMOTE_URI)));
  c.access_key_id =
      std::string(StripWhitespace(::Config::Get(::Config::MAIN_MEMCARD_SYNC_ACCESS_KEY_ID)));
  c.device_name =
      std::string(StripWhitespace(::Config::Get(::Config::MAIN_MEMCARD_SYNC_DEVICE_NAME)));
  if (!::Config::Get(::Config::MAIN_MEMCARD_SYNC_ENABLED) || c.remote_uri.empty() ||
      c.access_key_id.empty())
  {
    Stop(2000);
    return {};
  }

  std::string error;
  std::optional<std::string> secret = LoadSecret(SECRET_ACCESS_KEY, &error);
  if (!secret.has_value())
  {
    // No secret yet is "incomplete", not an error; a store that can't be read is.
    Stop(2000);
    return error;
  }
  std::optional<std::string> passphrase = LoadSecret(ENCRYPTION_PASSPHRASE, &error);
  if (!passphrase.has_value() && !error.empty())
  {
    WipeSecret(*secret);
    Stop(2000);
    return error;
  }

  c.secret_access_key = std::move(*secret);
  if (passphrase.has_value())
    c.passphrase = std::move(*passphrase);
  c.data_dir = DataDirFor(c.remote_uri);
  c.emulator_version = Common::GetScmDescStr();
  if (!File::CreateFullPath(c.data_dir + "/") || !File::IsDirectory(c.data_dir))
  {
    WipeSecret(c.secret_access_key);
    WipeSecret(c.passphrase);
    return fmt::format("Could not create the sync data folder {}.", c.data_dir);
  }
  const bool started = Start(c, &error);
  WipeSecret(c.secret_access_key);
  WipeSecret(c.passphrase);
  return started ? std::string() : error;
}

static void ApplyLoop()
{
  for (;;)
  {
    std::vector<SecretUpdate> secrets;
    std::vector<std::function<void(const std::string&)>> done;
    {
      std::unique_lock lock(s_apply_lock);
      s_apply_cv.wait(lock, [] { return s_apply_pending || s_apply_quit; });
      if (!s_apply_pending)
        return;
      s_apply_pending = false;
      s_apply_running = true;
      secrets = std::exchange(s_apply_secrets, {});
      done = std::exchange(s_apply_done, {});
    }

    std::string error;
    for (SecretUpdate& u : secrets)
    {
      std::string e;
      if (!StoreSecret(u.name, u.value, &e) && error.empty())
        error = std::move(e);
      WipeSecret(u.value);
    }
    if (error.empty())
      error = StartFromSettings();
    {
      std::lock_guard lock(s_state_lock);
      s_last_start_error = error;
    }
    if (!error.empty())
      WARN_LOG_FMT(CORE, "MemcardSync: not syncing: {}", error);
    for (const auto& cb : done)
      cb(error);
    {
      std::lock_guard lock(s_apply_lock);
      s_apply_running = false;
    }
    s_apply_idle_cv.notify_all();
  }
}

// Waits, up to BOOT_WAITS_FOR_START, for a start or stop queued or in progress: at Dolphin's
// startup, Initialize() queues one, and a game launched at once (from the command line, or by
// DolphinNoGUI) must not boot with no client and miss the saves from other devices.
static void WaitForPendingStart()
{
  std::unique_lock lock(s_apply_lock);
  if (!s_apply_idle_cv.wait_for(lock, BOOT_WAITS_FOR_START, [] {
        return s_apply_quit || (!s_apply_pending && !s_apply_running);
      }))
  {
    WARN_LOG_FMT(CORE, "MemcardSync: still starting; the game boots with the cards as they are.");
  }
}

void ApplySettingsAsync(std::vector<SecretUpdate> secrets,
                        std::function<void(const std::string&)> done)
{
  std::lock_guard lock(s_apply_lock);
  if (s_apply_quit)
    return;
  for (SecretUpdate& u : secrets)
    s_apply_secrets.push_back(std::move(u));
  if (done)
    s_apply_done.push_back(std::move(done));
  s_apply_pending = true;
  if (!s_apply_thread.joinable())
    s_apply_thread = std::thread(&ApplyLoop);
  s_apply_cv.notify_one();
}

void Initialize()
{
  if (::Config::Get(::Config::MAIN_MEMCARD_SYNC_ENABLED))
    ApplySettingsAsync();
}

void Shutdown()
{
  {
    std::lock_guard lock(s_apply_lock);
    s_apply_quit = true;
  }
  s_apply_cv.notify_one();
  s_apply_idle_cv.notify_all();
  // A start in progress finishes first (it can wait up to 10 s on the network).
  if (s_apply_thread.joinable())
    s_apply_thread.join();
  Stop(EXIT_UPLOAD_MS);
}

// ---- Core hooks -------------------------------------------------------------------------

void OnEmulationStarting(Core::System& system)
{
  WaitForPendingStart();
  const DiscIO::Region region = SConfig::GetInstance().m_region;
  {
    std::lock_guard lock(s_state_lock);
    s_emulating = true;
    s_emulating_region = region;
  }
  std::shared_lock client(s_client_lock);
  if (!s_client)
    return;

  EndSession();  // a session left by an emulation that never reached OnEmulationStopped()
  std::vector<Container> containers = RegisterCards(region);

  // A Wii game still opens the GameCube cards, and NetPlay and movies need the cards as they are:
  // the cards are held all the same, but nothing waits for the network.
  const bool wii = system.IsWii();
  const bool deterministic =
      NetPlay::IsNetPlayRunning() || system.GetMovie().IsMovieActive() || Core::WantsDeterminism();
  const u32 timeout = wii || deterministic ? NO_WAIT_MS : BOOT_SYNC_TIMEOUT_MS;
  es_session_info info;
  const es_session_id id =
      BeginSession(containers, wii ? "wii" : "gc", SConfig::GetInstance().GetGameID(),
                   SConfig::GetInstance().GetTitleName(), timeout, &info);
  {
    std::lock_guard lock(s_state_lock);
    s_containers = std::move(containers);
    s_session = id;
    s_session_started = std::chrono::steady_clock::now();
  }
  if (id != 0 && info.units_applied > 0)
  {
    NOTICE_LOG_FMT(CORE, "MemcardSync: {} save(s) updated from other devices before boot.",
                   info.units_applied);
    const std::string text =
        fmt::format("Updated {} save(s) from other devices", info.units_applied);
    Core::DisplayMessage(text, 5000);
    SetStatus(fmt::format("{} at {}.", text, TimeOfDay()));
  }
  else if (id != 0 && !info.remote_reached && timeout == BOOT_SYNC_TIMEOUT_MS)
  {
    WARN_LOG_FMT(CORE,
                 "MemcardSync: storage not reached before boot; starting with the cards as they "
                 "are.");
  }
}

void OnCardWritten(const std::string& path)
{
  const std::string written = WithoutTrailingSeparator(path);
  es_container_id id = 0;
  {
    std::lock_guard lock(s_state_lock);
    for (const Container& c : s_containers)
    {
      if (c.path == written)
        id = c.id;
    }
  }
  // Not a synced card: NetPlay's and movies' temporary cards, a card of a slot switched while the
  // game runs, or sync is off.
  if (id == 0)
    return;
  {
    std::lock_guard lock(s_worker_lock);
    s_flush_queue.push_back(id);
  }
  WakeWorker();
}

void OnEmulationStopped()
{
  {
    std::lock_guard lock(s_state_lock);
    s_emulating = false;
    s_emulating_region = DiscIO::Region::Unknown;
  }
  std::shared_lock client(s_client_lock);
  if (!s_client)
    return;
  // Uploads already queued by the cards' final writes are taken first; the session's end also
  // snapshots every card once more.
  EndSession();
}
}  // namespace MemcardSync

#endif  // HAVE_EMUSYNC
