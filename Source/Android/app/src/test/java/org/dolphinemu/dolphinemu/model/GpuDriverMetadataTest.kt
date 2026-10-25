// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import kotlinx.serialization.SerializationException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class GpuDriverMetadataTest {
    private val fullV1 = """
        {
          "schemaVersion": 1,
          "name": "Turnip",
          "author": "Mesa",
          "packageVersion": "24.1.0",
          "vendor": "Mesa",
          "driverVersion": "24.1.0-devel",
          "minApi": 28,
          "description": "Freedreno Turnip",
          "libraryName": "vulkan.freedreno.so"
        }
    """.trimIndent()

    @Test
    fun `full v1 manifest parses every field`() {
        val meta = GpuDriverMetadata.parse(fullV1)
        assertEquals("Turnip", meta.name)
        assertEquals("Mesa", meta.author)
        assertEquals("24.1.0", meta.packageVersion)
        assertEquals("Mesa", meta.vendor)
        assertEquals("24.1.0-devel", meta.driverVersion)
        assertEquals(28, meta.minApi)
        assertEquals("Freedreno Turnip", meta.description)
        assertEquals("vulkan.freedreno.so", meta.libraryName)
    }

    @Test
    fun `missing schemaVersion is accepted`() {
        val meta = GpuDriverMetadata.parse("""{"name": "NoSchema", "libraryName": "a.so"}""")
        assertEquals("NoSchema", meta.name)
    }

    @Test
    fun `unknown keys are ignored`() {
        val meta = GpuDriverMetadata.parse("""{"schemaVersion": 1, "name": "X", "someFutureKey": true}""")
        assertEquals("X", meta.name)
    }

    @Test
    fun `missing libraryName defaults to libvulkan_freedreno`() {
        val meta = GpuDriverMetadata.parse("""{"schemaVersion": 1, "name": "X"}""")
        assertEquals("libvulkan_freedreno.so", meta.libraryName)
    }

    @Test
    fun `missing minApi defaults to zero`() {
        val meta = GpuDriverMetadata.parse("""{"schemaVersion": 1, "name": "X"}""")
        assertEquals(0, meta.minApi)
    }

    @Test
    fun `unsupported schemaVersion is rejected`() {
        assertThrows(SerializationException::class.java) {
            GpuDriverMetadata.parse("""{"schemaVersion": 2, "name": "X"}""")
        }
    }

    @Test
    fun `malformed json is rejected`() {
        assertThrows(SerializationException::class.java) {
            GpuDriverMetadata.parse("not json")
        }
    }

    @Test
    fun `synthesized manifest round-trips through toJson and parse`() {
        val synthesized = GpuDriverMetadata.synthesize("freedrenobuilder-v1-turnip", "libvulkan_freedreno.so")
        val reparsed = GpuDriverMetadata.parse(synthesized.toJson())
        assertEquals(synthesized, reparsed)
        assertEquals("freedrenobuilder-v1-turnip", reparsed.name)
        assertEquals("libvulkan_freedreno.so", reparsed.libraryName)
    }
}
