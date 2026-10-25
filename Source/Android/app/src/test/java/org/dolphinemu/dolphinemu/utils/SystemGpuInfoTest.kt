// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SystemGpuInfoTest {
    @Test
    fun `five element array is fully parsed`() {
        val info = SystemGpuInfo.fromJniArray(
            arrayOf("4", "512.676.53", "Adreno (TM) 740", "0x5143", "1.3.128")
        )!!
        assertEquals("4", info.driverId)
        assertEquals("512.676.53", info.driverVersion)
        assertEquals("Adreno (TM) 740", info.deviceName)
        assertEquals(0x5143, info.vendorId)
        assertEquals("1.3.128", info.apiVersion)
    }

    @Test
    fun `legacy two element array leaves the new fields empty`() {
        val info = SystemGpuInfo.fromJniArray(arrayOf("4", "512.676.53"))!!
        assertEquals("4", info.driverId)
        assertEquals("512.676.53", info.driverVersion)
        assertEquals("", info.deviceName)
        assertNull(info.vendorId)
        assertEquals("", info.apiVersion)
    }

    @Test
    fun `null or empty array yields null`() {
        assertNull(SystemGpuInfo.fromJniArray(null))
        assertNull(SystemGpuInfo.fromJniArray(emptyArray()))
    }

    @Test
    fun `unparseable vendor id yields null vendor`() {
        val info = SystemGpuInfo.fromJniArray(arrayOf("4", "1.0.0", "GPU", "garbage", "1.1.0"))!!
        assertNull(info.vendorId)
    }
}
