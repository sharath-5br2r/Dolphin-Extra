// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GpuDriverCatalogTest {
    private val source = DriverSource(
        label = "Balemuni · Aurora",
        releasesApiUrl = "https://api.github.com/repos/Balemuni/Balemunis-Aurora/releases",
        idPrefix = "balemuni"
    )

    private val twoReleases = """
        [
          {
            "tag_name": "v1.2",
            "name": "Apex A740 release",
            "published_at": "2026-01-02T03:04:05Z",
            "assets": [
              {"name": "Apex A740.zip", "browser_download_url": "https://example.com/a740.zip", "size": 12345678},
              {"name": "README.txt", "browser_download_url": "https://example.com/readme.txt", "size": 10}
            ]
          },
          {
            "tag_name": "v1.1",
            "name": null,
            "published_at": "2025-12-01T00:00:00Z",
            "assets": [
              {"name": "Apex-Universal.ZIP", "browser_download_url": "https://example.com/universal.zip", "size": 100}
            ]
          }
        ]
    """.trimIndent()

    @Test
    fun `only zip assets become drivers`() {
        val drivers = GpuDriverCatalog.parseReleases(twoReleases, source)
        assertEquals(listOf("Apex A740.zip", "Apex-Universal.ZIP"), drivers.map { it.assetName })
    }

    @Test
    fun `id is built from prefix, tag and asset base`() {
        val drivers = GpuDriverCatalog.parseReleases(twoReleases, source)
        assertEquals("balemuni-v1.2-apex_a740", drivers[0].id)
        assertEquals("balemuni-v1.1-apex-universal", drivers[1].id)
    }

    @Test
    fun `download url, size, release info and publish date are carried over`() {
        val driver = GpuDriverCatalog.parseReleases(twoReleases, source)[0]
        assertEquals("https://example.com/a740.zip", driver.downloadUrl)
        assertEquals(12345678L, driver.sizeBytes)
        assertEquals("2026-01-02T03:04:05Z", driver.publishedAt)
        assertEquals("Apex A740 release", driver.releaseName)
        assertEquals("v1.2", driver.tagName)
        assertEquals(source, driver.source)
    }

    @Test
    fun `null release name falls back to the tag`() {
        val driver = GpuDriverCatalog.parseReleases(twoReleases, source)[1]
        assertEquals("v1.1", driver.releaseName)
    }

    @Test
    fun `malformed json yields an empty list`() {
        assertTrue(GpuDriverCatalog.parseReleases("this is not json", source).isEmpty())
    }

    @Test
    fun `an error object instead of an array yields an empty list`() {
        val rateLimited = """{"message": "API rate limit exceeded", "documentation_url": "https://x"}"""
        assertTrue(GpuDriverCatalog.parseReleases(rateLimited, source).isEmpty())
    }

    @Test
    fun `releases without assets are skipped`() {
        val json = """
            [
              {"tag_name": "v3", "name": "No assets yet"},
              {"tag_name": "v2", "name": "Has one", "assets": [
                {"name": "turnip.zip", "browser_download_url": "https://example.com/t.zip", "size": 5}
              ]}
            ]
        """.trimIndent()
        val drivers = GpuDriverCatalog.parseReleases(json, source)
        assertEquals(1, drivers.size)
        assertEquals("v2", drivers[0].tagName)
    }

    @Test
    fun `catalog lists the seven sources in display order`() {
        assertEquals(
            listOf("", "purpleturnip", "stevenmxz", "gamehub8e", "freedrenobuilder", "exynostools", "balemuni"),
            GpuDriverCatalog.SOURCES.map { it.idPrefix }
        )
        assertTrue(GpuDriverCatalog.SOURCES.all { it.releasesApiUrl.startsWith("https://api.github.com/repos/") })
        assertTrue(GpuDriverCatalog.SOURCES.all { it.releasesApiUrl.endsWith("/releases") })
    }
}
