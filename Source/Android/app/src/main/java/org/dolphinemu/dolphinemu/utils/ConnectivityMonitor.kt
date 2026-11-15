// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import org.dolphinemu.dolphinemu.features.settings.model.AchievementModel

/**
 * Reports changes in the device's default network to native code, so RetroAchievements can save
 * unlocks while offline and submit them once a connection is back.
 */
object ConnectivityMonitor {
    private var started = false
    private var validated = false

    fun start(context: Context) {
        if (started) return

        val manager =
            context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        if (manager == null) {
            Log.warning("[ConnectivityMonitor] ConnectivityManager unavailable; assuming online.")
            return
        }

        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                report(true)
            }

            override fun onCapabilitiesChanged(
                network: Network,
                networkCapabilities: NetworkCapabilities
            ) {
                val hasInternet =
                    networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                val isValidated =
                    networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
                val becameValidated = isValidated && !validated
                validated = isValidated
                // Only "no network at all" counts as offline. Requiring validation would hold
                // back submissions on networks where Android's connectivity check is blocked.
                // Gaining validation is still worth reporting: it means the connection works
                // now, and every report of a connection retries the saved submissions.
                if (!hasInternet || becameValidated) report(hasInternet)
            }

            override fun onLost(network: Network) {
                validated = false
                report(false)
            }
        }

        try {
            if (manager.activeNetwork == null) report(false)
            manager.registerDefaultNetworkCallback(callback)
            started = true
        } catch (e: RuntimeException) {
            // Registration can throw SecurityException or TooManyRequestsException on some
            // devices. Native code then treats the device as always online and relies on failed
            // requests to decide when to queue.
            Log.warning("[ConnectivityMonitor] Could not monitor network changes: ${e.message}")
            report(true)
        }
    }

    private fun report(available: Boolean) {
        AchievementModel.setNetworkAvailable(available)
    }
}
