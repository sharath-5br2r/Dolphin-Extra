// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

/** Vulkan properties of the system GPU driver, as probed by the native `getSystemDriverInfo`. */
data class SystemGpuInfo(
    val driverId: String,
    val driverVersion: String,
    val deviceName: String,
    val vendorId: Int?,
    val apiVersion: String,
) {
    companion object {
        /**
         * Decodes the JNI array `[driverId, driverVersion, deviceName, vendorId, apiVersion]`.
         * Older native builds return only the first two entries; the rest read as unknown.
         */
        fun fromJniArray(array: Array<String>?): SystemGpuInfo? {
            if (array.isNullOrEmpty()) return null
            return SystemGpuInfo(
                driverId = array[0],
                driverVersion = array.getOrElse(1) { "" },
                deviceName = array.getOrElse(2) { "" },
                vendorId = array.getOrNull(3)?.removePrefix("0x")?.toIntOrNull(16),
                apiVersion = array.getOrElse(4) { "" },
            )
        }
    }
}
