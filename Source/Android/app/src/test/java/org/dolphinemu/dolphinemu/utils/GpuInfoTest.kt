// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test

class GpuInfoTest {
    private val qualcomm = 0x5143

    @Test
    fun `Adreno 7xx recommends Balemuni Aurora`() {
        assertSame(GpuDriverCatalog.BALEMUNI, GpuInfo.recommend("Adreno (TM) 740", qualcomm))
    }

    @Test
    fun `Adreno 6xx recommends purple-turnip`() {
        assertSame(GpuDriverCatalog.PURPLE_TURNIP, GpuInfo.recommend("Adreno (TM) 650", qualcomm))
    }

    @Test
    fun `Adreno 8xx recommends crueter GameHub`() {
        assertSame(GpuDriverCatalog.GAMEHUB_8ELITE, GpuInfo.recommend("Adreno (TM) 830", qualcomm))
    }

    @Test
    fun `Samsung Xclipse recommends ExynosTools`() {
        assertSame(GpuDriverCatalog.EXYNOSTOOLS, GpuInfo.recommend("Samsung Xclipse 940", 0x144d))
    }

    @Test
    fun `Mali recommends the system driver`() {
        assertNull(GpuInfo.recommend("Mali-G715", 0x13b5))
    }

    @Test
    fun `Qualcomm vendor with an unrecognized model recommends K11MCH1`() {
        assertSame(GpuDriverCatalog.K11MCH1, GpuInfo.recommend("Adreno (TM) 540", qualcomm))
        assertSame(GpuDriverCatalog.K11MCH1, GpuInfo.recommend("Unknown", qualcomm))
    }

    @Test
    fun `unknown device and vendor recommends the system driver`() {
        assertNull(GpuInfo.recommend(null, null))
        assertNull(GpuInfo.recommend("", null))
    }
}
