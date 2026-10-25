// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu

import android.app.Activity
import android.app.Application
import android.content.Context
import android.hardware.usb.UsbManager
import androidx.lifecycle.Observer
import org.dolphinemu.dolphinemu.features.memcardsync.MemcardSync
import org.dolphinemu.dolphinemu.utils.ActivityTracker
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.GCAdapter
import org.dolphinemu.dolphinemu.utils.WiimoteAdapter

class DolphinApplication : Application() {
    private val activityTracker = ActivityTracker()

    override fun onCreate() {
        super.onCreate()
        instance = this
        registerActivityLifecycleCallbacks(activityTracker)
        System.loadLibrary("main")

        val usbManager = getSystemService(USB_SERVICE) as UsbManager
        GCAdapter.manager = usbManager
        WiimoteAdapter.manager = usbManager

        // Memory card sync starts once Dolphin's directories and settings are ready: on a first
        // run that can be after the user grants storage access.
        MemcardSync.init(this)
        val directories = DirectoryInitialization.getDolphinDirectoriesState()
        directories.observeForever(object : Observer<DirectoryInitialization.DirectoryInitializationState> {
            override fun onChanged(value: DirectoryInitialization.DirectoryInitializationState) {
                if (value == DirectoryInitialization.DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED) {
                    directories.removeObserver(this)
                    MemcardSync.startInBackground()
                }
            }
        })

        if (DirectoryInitialization.shouldStart(applicationContext)) {
            DirectoryInitialization.start(applicationContext)
        }
    }

    companion object {
        @JvmStatic
        lateinit var instance: DolphinApplication
            private set

        @JvmStatic
        fun getAppContext(): Context = instance.applicationContext

        @JvmStatic
        fun getAppActivity(): Activity? = instance.activityTracker.currentActivity
    }
}
