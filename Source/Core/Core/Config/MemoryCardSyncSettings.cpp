// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/MemoryCardSyncSettings.h"

namespace Config
{
// Main.MemoryCardSync

const Info<bool> MAIN_MEMCARD_SYNC_ENABLED{{System::Main, "MemoryCardSync", "Enabled"}, false};
const Info<std::string> MAIN_MEMCARD_SYNC_REMOTE_URI{{System::Main, "MemoryCardSync", "RemoteURI"},
                                                     ""};
const Info<std::string> MAIN_MEMCARD_SYNC_ACCESS_KEY_ID{
    {System::Main, "MemoryCardSync", "AccessKeyID"}, ""};
const Info<std::string> MAIN_MEMCARD_SYNC_DEVICE_NAME{
    {System::Main, "MemoryCardSync", "DeviceName"}, ""};
const Info<bool> MAIN_MEMCARD_SYNC_SECRET_STORED{
    {System::Main, "MemoryCardSync", "SecretAccessKeyStored"}, false};
const Info<bool> MAIN_MEMCARD_SYNC_PASSPHRASE_STORED{
    {System::Main, "MemoryCardSync", "EncryptionPassphraseStored"}, false};
}  // namespace Config
