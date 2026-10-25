// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/Config/Config.h"

namespace Config
{
// Main.MemoryCardSync: memory card sync with emusync (Core/HW/GCMemcard/MemcardSync.h). The
// secret access key and the encryption passphrase are not here: they are in the system's
// credential store, and these only record that one is stored.

extern const Info<bool> MAIN_MEMCARD_SYNC_ENABLED;
extern const Info<std::string> MAIN_MEMCARD_SYNC_REMOTE_URI;
extern const Info<std::string> MAIN_MEMCARD_SYNC_ACCESS_KEY_ID;
extern const Info<std::string> MAIN_MEMCARD_SYNC_DEVICE_NAME;
extern const Info<bool> MAIN_MEMCARD_SYNC_SECRET_STORED;
extern const Info<bool> MAIN_MEMCARD_SYNC_PASSPHRASE_STORED;
}  // namespace Config
