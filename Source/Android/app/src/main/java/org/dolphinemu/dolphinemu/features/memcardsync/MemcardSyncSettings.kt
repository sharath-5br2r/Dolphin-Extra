// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.memcardsync

import android.content.Context
import android.widget.Toast
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

/**
 * The memory card sync settings as Dolphin settings rows. They are not in Dolphin.ini: emusync's
 * SyncSettings keeps them, with the secrets in the Android Keystore. Each change applies at once:
 * sync restarts (or stops) in the background, and a problem is shown as a toast.
 */
object MemcardSyncSettings {
    /** The last start's problem, for the status row; null when sync runs or is off. */
    @Volatile
    var lastError: String? = null
        private set

    /** Whether an [apply] is still connecting. Main thread. */
    private var starting = 0

    /** Applies the stored settings in the background. */
    fun apply() {
        starting++
        MemcardSync.controller.startAsync { error ->
            starting--
            lastError = error
            if (error != null) {
                val context = DolphinApplication.getAppContext()
                Toast.makeText(context, error, Toast.LENGTH_LONG).show()
            }
        }
    }

    /** The status row's text. */
    fun status(context: Context): String {
        val controller = MemcardSync.controller
        val values = MemcardSync.settings.load()
        return when {
            !controller.isAvailable -> context.getString(R.string.memcard_sync_unavailable)
            starting > 0 -> context.getString(R.string.memcard_sync_status_connecting)
            !values.enabled -> context.getString(R.string.memcard_sync_status_off)
            lastError != null -> lastError!!
            !controller.isRunning -> context.getString(R.string.memcard_sync_status_incomplete)
            else -> controller.status().ifEmpty { context.getString(R.string.memcard_sync_status_running) }
        }
    }

    class StringSetting(
        private val read: () -> String,
        private val write: (String) -> Unit
    ) : AbstractStringSetting {
        override val string: String
            get() = read()

        override fun setString(settings: Settings, newValue: String) {
            // OK in a dialog left unchanged: nothing to store, and no restart.
            if (newValue == read()) return
            write(newValue)
            apply()
        }

        override val isOverridden: Boolean = false
        override val isRuntimeEditable: Boolean = true

        override fun delete(settings: Settings): Boolean {
            write("")
            apply()
            return true
        }
    }

    class BooleanSetting(
        private val read: () -> Boolean,
        private val write: (Boolean) -> Unit
    ) : AbstractBooleanSetting {
        override val boolean: Boolean
            get() = read()

        override fun setBoolean(settings: Settings, newValue: Boolean) {
            write(newValue)
            apply()
        }

        override val isOverridden: Boolean = false
        override val isRuntimeEditable: Boolean = true

        override fun delete(settings: Settings): Boolean {
            write(false)
            apply()
            return true
        }
    }
}
