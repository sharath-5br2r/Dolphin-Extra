// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.memcardsync

import android.content.Context
import io.github.ilyalissoboi.emusync.SyncBackend
import io.github.ilyalissoboi.emusync.SyncConfig
import io.github.ilyalissoboi.emusync.SyncController
import io.github.ilyalissoboi.emusync.SyncSettings
import org.dolphinemu.dolphinemu.BuildConfig

/**
 * Memory card sync with emusync: the GameCube cards in slots A and B sync, per save, with Dolphin
 * on other devices and on computers, through S3 storage the user owns.
 *
 * The core does the syncing (Core/HW/GCMemcard/MemcardSync.h); this keeps the settings, with the
 * secret access key and the encryption passphrase in the Android Keystore (emusync's
 * [SyncSettings]), starts and stops sync in the background to match them, and tells the core when
 * the app leaves the screen so new saves upload ([SyncController]).
 */
object MemcardSync {
    /** The core, through JNI. */
    private object NativeBackend : SyncBackend {
        override val isAvailable: Boolean
            get() = MemcardSyncNative.isAvailable()

        override fun start(config: SyncConfig): String? = MemcardSyncNative.start(
            config.dataDir.path,
            config.deviceName,
            config.appVersion,
            config.remoteUri,
            config.accessKeyId,
            config.secretAccessKey,
            config.passphrase
        )

        override fun stop() = MemcardSyncNative.stop()

        override val isRunning: Boolean
            get() = MemcardSyncNative.isRunning()

        override fun syncNow() = MemcardSyncNative.syncNow()

        override fun status(): String = MemcardSyncNative.status()

        override fun setAppBackgrounded(backgrounded: Boolean) =
            MemcardSyncNative.setAppBackgrounded(backgrounded)
    }

    private lateinit var controllerInstance: SyncController

    val controller: SyncController
        get() = controllerInstance

    val settings: SyncSettings
        get() = controllerInstance.settings

    /**
     * Creates the controller and follows the app's lifecycle. Main thread, once, after the native
     * library is loaded. Sync itself starts with [startInBackground], once Dolphin's directories
     * and settings are ready.
     */
    fun init(context: Context) {
        controllerInstance =
            SyncController(SyncSettings(context), NativeBackend, BuildConfig.VERSION_NAME)
        controllerInstance.attachToAppLifecycle()
    }

    /** Starts sync from the stored settings, with nothing waiting for it. */
    fun startInBackground() = controllerInstance.startInBackground()
}
