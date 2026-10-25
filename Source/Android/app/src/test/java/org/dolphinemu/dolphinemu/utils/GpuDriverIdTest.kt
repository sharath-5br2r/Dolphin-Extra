// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.junit.Assert.assertEquals
import org.junit.Test

class GpuDriverIdTest {
    @Test
    fun `id joins prefix, tag and asset base with dashes`() {
        assertEquals("balemuni-v1.2-apex_a740", GpuDriverId.makeId("balemuni", "v1.2", "Apex A740.zip"))
    }

    @Test
    fun `empty prefix omits the leading dash`() {
        assertEquals("v1.2-apex_a740", GpuDriverId.makeId("", "v1.2", "Apex A740.zip"))
    }

    @Test
    fun `empty tag is omitted`() {
        assertEquals("local-mydriver", GpuDriverId.makeId("local", "", "MyDriver.zip"))
    }

    @Test
    fun `zip extension is stripped case-insensitively`() {
        assertEquals("v1-turnip", GpuDriverId.makeId("", "v1", "Turnip.ZIP"))
    }

    @Test
    fun `only lower-case alphanumerics, dot, underscore and dash survive`() {
        assertEquals("v1-foo__bar__baz_", GpuDriverId.makeId("", "v1", "Foo (Bar)+Baz!.zip"))
    }

    @Test
    fun `sanitize lower-cases and replaces disallowed characters`() {
        assertEquals("turnip_driver-v24.1.0", GpuDriverId.sanitize("Turnip Driver-v24.1.0"))
    }
}
