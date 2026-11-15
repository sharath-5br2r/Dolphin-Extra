// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Emulator state saving support.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"

namespace Core
{
class System;
}

namespace BigPicture
{
struct LibraryImage;
}

namespace State
{
// number of states
static const u32 NUM_STATES = 10;

struct StateHeaderLegacy
{
  char game_id[6];
  char reserved1[2];
  u32 lzo_size = 0;  // Must be zero for new states. Used to support legacy decompression algorithm.
  char reserved2[4];
  double time;
};
constexpr size_t STATE_HEADER_SIZE = sizeof(StateHeaderLegacy);
static_assert(STATE_HEADER_SIZE == 24);
static_assert(offsetof(StateHeaderLegacy, lzo_size) == 8);
static_assert(offsetof(StateHeaderLegacy, time) == 16);
static_assert(std::is_trivially_copyable_v<StateHeaderLegacy>);

struct StateHeaderVersion
{
  u32 version_cookie;
  u32 version_string_length;
};
static_assert(std::is_trivially_copyable_v<StateHeaderVersion>);

struct StateHeader
{
  StateHeaderLegacy legacy_header;
  StateHeaderVersion version_header;
  std::string version_string;
};

enum CompressionType : u16
{
  Uncompressed = 0,
  LZ4 = 1,
  // Add new compression types after this, as the compression type
  // is numerically stored in the state file.
};

struct StateExtendedBaseHeader
{
  u16 header_version;
  u16 compression_type;
  u32 payload_offset;
  u64 uncompressed_size;
};
constexpr size_t EXTENDED_BASE_HEADER_SIZE = sizeof(StateExtendedBaseHeader);
static_assert(EXTENDED_BASE_HEADER_SIZE == 16);
static_assert(offsetof(StateExtendedBaseHeader, payload_offset) == 4);
static_assert(offsetof(StateExtendedBaseHeader, uncompressed_size) == 8);
static_assert(std::is_trivially_copyable_v<StateExtendedBaseHeader>);

struct StateExtendedHeader
{
  StateExtendedBaseHeader base_header;
  // Feel free to add new fields here, adjusting COMPRESSED_DATA_OFFSET accordingly, as well as
  // CreateExtendedHeader(). Add the appropriate IOFile read/write calls within LoadFileStateData()
  // and WriteHeadersToFile()
};

void Init(Core::System& system);
void Shutdown();

// Returns a string containing information of the savestate in the given slot
// which can be presented to the user for identification purposes
std::string GetInfoStringOfSlot(u32 slot, bool translate = true);

// Returns when the savestate in the given slot was created, or 0 if the slot is empty.
u64 GetUnixTimeOfSlot(u32 slot);

struct SlotInfo
{
  bool exists = false;
  bool readable = false;
  std::string description;
  std::shared_ptr<const BigPicture::LibraryImage> preview;
};
// A single locked metadata read for menu snapshots. This does not validate the state payload.
SlotInfo GetSlotInfo(u32 slot);
// The same, for a game that need not be running. An invalid game ID reads as an empty slot.
SlotInfo GetSlotInfo(const std::string& game_id, u32 slot);
// The numbered slot file for a game; empty if the game ID cannot name a file.
std::string GetSlotPath(const std::string& game_id, u32 slot);

// These don't happen instantly - they get scheduled as events.
// ...But only if we're not in the main CPU thread.
//    If we're in the main CPU thread then they run immediately instead.
void Save(Core::System& system, u32 slot);
void Load(Core::System& system, u32 slot);

u32 GetSelectedSlot();
void SetSelectedSlot(u32 slot);

void SelectSlot(u32 slot);

u32 GetNextSlot();
u32 GetPreviousSlot();

void SaveSelected(Core::System& system);
void LoadSelected(Core::System& system);

void SaveAs(Core::System& system, std::string filename);
void LoadAs(Core::System& system, std::string filename);

// A continuation is separate from numbered slots and the user's undo-save backup.
std::string GetContinuationPath(const std::string& game_id);
bool DeleteContinuation(const std::string& filename);
// Host-thread operation: captures and finishes writing before returning. Failure must cancel stop.
bool SaveContinuation(Core::System& system, const std::string& filename);

void LoadLastSaved(Core::System& system, int i = 1);
void SaveFirstSaved(Core::System& system);
void UndoSaveState(Core::System& system);
void UndoLoadState(Core::System& system);

// for calling back into UI code without introducing a dependency on it in core
using AfterLoadCallbackFunc = std::function<void()>;
void SetOnAfterLoadCallback(AfterLoadCallbackFunc callback);
// Invoked on the save worker after writing the state and its optional preview finishes.
// Callbacks must queue UI work rather than reading slots while the save task holds its lock.
[[nodiscard]] Common::EventHook AddAfterSaveCallback(std::function<void()> callback);

struct OperationResult
{
  enum class Kind
  {
    Save,
    Load
  };
  enum class Error
  {
    None,
    // Loading is disabled (NetPlay or RetroAchievements hardcore mode).
    NotAllowed,
    // The emulated system could not be serialized.
    Capture,
    // The state file could not be written or moved into place.
    Write,
    // The state file is missing or unreadable.
    Unreadable,
    // The state was read but could not be applied; the previous state was restored.
    Incompatible
  };
  Kind kind = Kind::Save;
  std::string filename;
  Error error = Error::None;
};
// Reports the outcome of every save to or load from a state file, including continuations, on
// the thread that finishes it (the CPU or savestate worker thread). Callbacks must queue UI work.
[[nodiscard]] Common::EventHook
AddResultCallback(std::function<void(const OperationResult&)> callback);
}  // namespace State
