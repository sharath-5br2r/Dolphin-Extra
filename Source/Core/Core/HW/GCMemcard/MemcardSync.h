// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Core
{
class System;
}

// Memory card sync through the cloud, with emusync (https://github.com/ilyalissoboi/emusync).
//
// The GameCube cards in slots A and B are synced, per save, with every other device using the
// same storage: Dolphin on other computers and on Android, and anything running `emusync watch`.
// A save made on one device shows up on the others; two devices playing different games never
// conflict, and when the same save was changed on two devices the newest version wins and the
// other stays in emusync's history.
//
// Which cards: one per slot and region, "MemoryCardA.USA" for slot A's USA card, whether the slot
// holds a raw card (GC/MemoryCardA.USA.raw), a GCI folder (GC/USA/Card A) or either at a custom
// path. So a slot switched between a raw card and a GCI folder, or a different kind on each
// device, is still one card. A region's card is synced once it exists on this computer, or from
// the moment a game of that region boots: its card is then created, if it doesn't exist yet, for
// the saves made elsewhere.
//
// When cards are read and written (with the hooks below in place):
//   Boot: OnEmulationStarting(), before the cards are opened. Newer saves from other devices are
//   written into them first, waiting at most 3 s for the network; offline, the game starts with
//   the cards as they are. Wii games, NetPlay and movies don't wait, and get nothing new. From then
//   until OnEmulationStopped(), emusync never writes to the cards; changes that arrive meanwhile
//   wait and are applied after the game.
//   Play: OnCardWritten() after Dolphin writes a raw card or a save of a GCI folder to disk
//   uploads the change. Snapshots go to an on-disk queue first, so a save is not lost if Dolphin
//   exits before the upload.
//   Shutdown: OnEmulationStopped() (cards closed) ends the session, uploads, and applies waiting
//   changes.
//
// Settings: [MemoryCardSync] in Dolphin.ini (Enabled, RemoteURI, AccessKeyID, DeviceName), with
// the secret access key and the optional encryption passphrase in the system's credential store
// (Credential Manager, Keychain, Secret Service) through emusync, never in the INI. The Android
// app keeps its settings itself and calls Start() and Stop().
//
// Without HAVE_EMUSYNC (emusync not found at build time) every function here is a no-op,
// IsAvailable() is false and starting fails, saying the build has no sync.
//
// Threading: everything is thread-safe. Start() and OnEmulationStarting() block (local database
// open, and the network as described); every other hook returns quickly.
namespace MemcardSync
{
// Whether this build includes memory card sync.
bool IsAvailable();

struct Config
{
  // Private folder for emusync's database and queue. Bound to one storage for good, so use one
  // per remote_uri (DataDirFor()).
  std::string data_dir;
  // Shown on other devices, e.g. in a save's history. Empty: the computer's name.
  std::string device_name;
  std::string emulator_version;
  // The storage, e.g. "s3://my-saves/emusync/v1?region=eu-west-1".
  std::string remote_uri;
  std::string access_key_id;
  std::string secret_access_key;
  // Optional: the storage's end-to-end encryption passphrase. Set, it creates the storage
  // encrypted if it is new, or unlocks it if it already is; storage that is not encrypted is then
  // refused. Empty, unencrypted storage is used as it is, and encrypted storage this computer has
  // already unlocked keeps working (emusync keeps its key in data_dir).
  std::string passphrase;
};

// Starts syncing, replacing a client that is already running. Blocks while the local database
// opens and for up to 10 s while the storage is checked. Returns false with `error` set when sync
// cannot start (wrong address, keys rejected, unreadable data folder). Storage that cannot be
// reached right now is not an error: sync starts working once it can be reached. Does not keep
// config.passphrase; the caller should WipeSecret() its own copy afterwards.
bool Start(const Config& config, std::string* error);

// Stops syncing. Cards are left as they are. Waits up to `timeout_ms` for queued uploads.
void Stop(u32 timeout_ms);

bool IsRunning();

// One line about the last sync, for the settings page.
std::string GetStatus();

// Queues a full sync: download, apply to cards no game has open, upload.
void SyncNow();

// The app went to the background (true) or came back (false): on Android, uploads what the cards
// hold now before the system may stop the app.
void SetAppBackgrounded(bool backgrounded);

// ---- Settings (Dolphin.ini and the system's credential store) --------------------------------

// The emusync data folder for `remote_uri`: <User>/emusync/<hash of the address>.
std::string DataDirFor(const std::string& remote_uri);

// Names of the secrets in the system's credential store (service "Dolphin").
inline constexpr const char* SECRET_ACCESS_KEY = "s3-secret-access-key";
inline constexpr const char* ENCRYPTION_PASSPHRASE = "encryption-passphrase";

// Stores a secret in the system's credential store; an empty value deletes it. Records whether one
// is stored in Dolphin.ini (so the settings page can say so without asking the store). May block
// while the system asks to unlock the keychain: not on the UI thread.
bool StoreSecret(const char* name, const std::string& value, std::string* error);

// The stored secret, if any. May block like StoreSecret().
std::optional<std::string> LoadSecret(const char* name, std::string* error);

// Whether a secret is recorded as stored (from Dolphin.ini; does not ask the store).
bool HasSecret(const char* name);

// Applies the settings on a background thread: optionally stores new secrets first, then starts
// sync with the settings, or stops it when it is switched off or incomplete. Requests made while
// one runs are folded into one more run with the latest settings. `done`, if set, is called on
// that thread with an empty string when sync runs or is deliberately off, else a sentence for the
// user.
struct SecretUpdate
{
  const char* name;
  std::string value;
};
void ApplySettingsAsync(std::vector<SecretUpdate> secrets = {},
                        std::function<void(const std::string& error)> done = {});

// The error of the last ApplySettingsAsync() run, empty if it started (or stopped) cleanly.
std::string GetLastStartError();

// Desktop: starts sync from the settings at startup, and stops it at exit, giving queued uploads a
// few seconds.
void Initialize();
void Shutdown();

// Overwrites a secret's characters, then empties the string and frees its buffer. Plain clear()
// only changes the size, leaving the characters readable in memory.
void WipeSecret(std::string& secret);

// ---- Core hooks ----------------------------------------------------------------------------

// The emulation thread, after the game's metadata and the movie are set up and before HW::Init
// opens the cards. Begins the session (see above).
void OnEmulationStarting(Core::System& system);

// Dolphin wrote a raw card (`path`, the file) or a GCI folder's saves (`path`, the folder) to
// disk. Called on Dolphin's card threads; queues the upload and returns at once.
void OnCardWritten(const std::string& path);

// The emulation thread, after HW::Shutdown closed the cards. Ends the session; safe to call when
// none is open.
void OnEmulationStopped();
}  // namespace MemcardSync
