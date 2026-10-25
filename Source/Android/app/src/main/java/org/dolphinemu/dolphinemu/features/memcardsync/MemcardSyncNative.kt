// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.memcardsync

/**
 * The core's memory card sync (Core/HW/GCMemcard/MemcardSync.h), through jni/MemcardSync.cpp. The
 * core owns the emusync client; this only starts and stops it and reports on it.
 */
object MemcardSyncNative {
    /** Whether this build includes memory card sync. */
    @JvmStatic
    external fun isAvailable(): Boolean

    /**
     * Starts sync (restarting it if it runs). Blocks for up to ~10 s: never on the main thread.
     * Returns null when sync runs, else a sentence for the user.
     */
    @JvmStatic
    external fun start(
        dataDir: String,
        deviceName: String,
        appVersion: String,
        remoteUri: String,
        accessKeyId: String,
        secretAccessKey: String,
        passphrase: String?
    ): String?

    @JvmStatic
    external fun stop()

    @JvmStatic
    external fun isRunning(): Boolean

    @JvmStatic
    external fun syncNow()

    /** One line about the last sync; empty when nothing happened yet. */
    @JvmStatic
    external fun status(): String

    /** Main thread; returns at once. */
    @JvmStatic
    external fun setAppBackgrounded(backgrounded: Boolean)
}
