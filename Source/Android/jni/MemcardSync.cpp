// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Memory card sync (Core/HW/GCMemcard/MemcardSync.h) for the Android app, whose Kotlin side keeps
// the settings (secrets in the Android Keystore, through emusync's AAR) and starts and stops sync
// through these functions (features/memcardsync/MemcardSyncNative.kt).

#include <string>

#include <jni.h>

#include "Common/CommonTypes.h"
#include "Core/HW/GCMemcard/MemcardSync.h"
#include "jni/AndroidCommon/AndroidCommon.h"

namespace
{
std::string OptionalString(JNIEnv* env, jstring str)
{
  return str ? GetJString(env, str) : std::string();
}
}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_isAvailable(JNIEnv*, jclass)
{
  return MemcardSync::IsAvailable();
}

// Returns null when sync runs, else a sentence for the user. Blocks (database, up to ~10 s of
// network): the Kotlin side calls it on its own thread.
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_start(
    JNIEnv* env, jclass, jstring data_dir, jstring device_name, jstring app_version,
    jstring remote_uri, jstring access_key_id, jstring secret_access_key, jstring passphrase)
{
  MemcardSync::Config config;
  config.data_dir = GetJString(env, data_dir);
  config.device_name = OptionalString(env, device_name);
  config.emulator_version = OptionalString(env, app_version);
  config.remote_uri = GetJString(env, remote_uri);
  config.access_key_id = GetJString(env, access_key_id);
  config.secret_access_key = GetJString(env, secret_access_key);
  // Null or empty: no passphrase (see MemcardSync::Config::passphrase).
  config.passphrase = OptionalString(env, passphrase);
  std::string error;
  const bool started = MemcardSync::Start(config, &error);
  MemcardSync::WipeSecret(config.secret_access_key);
  MemcardSync::WipeSecret(config.passphrase);
  return started ? nullptr : ToJString(env, error);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_stop(JNIEnv*, jclass)
{
  MemcardSync::Stop(2000);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_isRunning(JNIEnv*, jclass)
{
  return MemcardSync::IsRunning();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_syncNow(JNIEnv*, jclass)
{
  MemcardSync::SyncNow();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_status(JNIEnv* env, jclass)
{
  return ToJString(env, MemcardSync::GetStatus());
}

// The main thread: hands the work to the sync worker and returns at once.
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_memcardsync_MemcardSyncNative_setAppBackgrounded(
    JNIEnv*, jclass, jboolean backgrounded)
{
  MemcardSync::SetAppBackgrounded(backgrounded == JNI_TRUE);
}
}
