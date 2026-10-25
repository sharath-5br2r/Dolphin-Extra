// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.longOrNull
import org.dolphinemu.dolphinemu.BuildConfig
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/** A GitHub repository whose release assets are adrenotools driver packages. */
data class DriverSource(
    val label: String,
    val releasesApiUrl: String,
    /** Folded into every driver id from this source so two repositories cannot collide. */
    val idPrefix: String,
)

/** A driver package offered by a [DriverSource] release (not yet installed). */
data class RemoteGpuDriver(
    val id: String,
    val source: DriverSource,
    val releaseName: String,
    val tagName: String,
    val assetName: String,
    val downloadUrl: String,
    val sizeBytes: Long,
    val publishedAt: String,
)

/**
 * Catalog of downloadable GPU drivers, backed by the GitHub releases API of a fixed set of
 * community repositories.
 */
object GpuDriverCatalog {
    private const val LIST_TIMEOUT_MS = 15_000
    private const val DOWNLOAD_TIMEOUT_MS = 60_000
    private const val USER_AGENT = "Dolphin-Android/" + BuildConfig.VERSION_NAME

    private fun githubReleases(repo: String) = "https://api.github.com/repos/$repo/releases"

    val K11MCH1 = DriverSource("K11MCH1 · AdrenoToolsDrivers", githubReleases("K11MCH1/AdrenoToolsDrivers"), "")
    val PURPLE_TURNIP = DriverSource("MrPurple666 · purple-turnip", githubReleases("MrPurple666/purple-turnip"), "purpleturnip")
    val STEVENMXZ = DriverSource("StevenMXZ · Adreno-Tools-Drivers", githubReleases("StevenMXZ/Adreno-Tools-Drivers"), "stevenmxz")
    val GAMEHUB_8ELITE = DriverSource("crueter · GameHub 8Elite", githubReleases("crueter/GameHub-8Elite-Drivers"), "gamehub8e")
    val FREEDRENO_BUILDER = DriverSource("PojavLauncherTeam · freedreno-builder", githubReleases("PojavLauncherTeam/freedreno-builder"), "freedrenobuilder")
    val EXYNOSTOOLS = DriverSource("WearyConcern1165 · ExynosTools", githubReleases("WearyConcern1165/ExynosTools"), "exynostools")
    val BALEMUNI = DriverSource("Balemuni · Aurora", githubReleases("Balemuni/Balemunis-Aurora"), "balemuni")

    /** All sources, in display order. */
    val SOURCES: List<DriverSource> = listOf(
        K11MCH1, PURPLE_TURNIP, STEVENMXZ, GAMEHUB_8ELITE, FREEDRENO_BUILDER, EXYNOSTOOLS, BALEMUNI
    )

    /**
     * Flattens a GitHub releases API response into one [RemoteGpuDriver] per `.zip` asset.
     * Anything unparseable (including an API error object) yields an empty list.
     */
    fun parseReleases(json: String, source: DriverSource): List<RemoteGpuDriver> {
        val releases = runCatching { Json.parseToJsonElement(json) }.getOrNull() as? JsonArray
            ?: return emptyList()

        return releases.flatMap { release ->
            val releaseObject = release as? JsonObject ?: return@flatMap emptyList()
            val tag = releaseObject.string("tag_name")
            val releaseName = releaseObject.string("name").ifEmpty { tag }
            val publishedAt = releaseObject.string("published_at")
            val assets = releaseObject["assets"] as? JsonArray ?: return@flatMap emptyList()

            assets.mapNotNull { asset ->
                val assetObject = asset as? JsonObject ?: return@mapNotNull null
                val assetName = assetObject.string("name")
                if (!assetName.endsWith(".zip", ignoreCase = true)) return@mapNotNull null
                val url = assetObject.string("browser_download_url")
                if (url.isEmpty()) return@mapNotNull null
                RemoteGpuDriver(
                    id = GpuDriverId.makeId(source.idPrefix, tag, assetName),
                    source = source,
                    releaseName = releaseName,
                    tagName = tag,
                    assetName = assetName,
                    downloadUrl = url,
                    sizeBytes = (assetObject["size"] as? JsonPrimitive)?.longOrNull ?: 0L,
                    publishedAt = publishedAt,
                )
            }
        }
    }

    /** Fetches one source's releases. Network or API failures yield an empty list. */
    fun fetch(source: DriverSource): List<RemoteGpuDriver> {
        val body = try {
            httpGetText(source.releasesApiUrl)
        } catch (e: IOException) {
            Log.warning("[GpuDriverCatalog] ${source.label}: ${e.message}")
            return emptyList()
        }
        return parseReleases(body, source)
    }

    /** Fetches every source sequentially; a failing source contributes nothing. Blocking. */
    fun fetchAll(): List<RemoteGpuDriver> = SOURCES.flatMap { fetch(it) }

    /**
     * Streams the driver's zip asset straight into [GpuDriverHelper.installFromStream].
     * @return the install result, or `null` if the download itself failed. Blocking.
     */
    fun download(remote: RemoteGpuDriver): GpuDriverInstallResult? {
        val connection = try {
            openConnection(remote.downloadUrl, DOWNLOAD_TIMEOUT_MS).also { it.connect() }
        } catch (e: IOException) {
            Log.warning("[GpuDriverCatalog] download ${remote.id}: ${e.message}")
            return null
        }
        try {
            if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                Log.warning("[GpuDriverCatalog] download ${remote.id}: HTTP ${connection.responseCode}")
                return null
            }
            return connection.inputStream.use { GpuDriverHelper.installFromStream(it, remote.id) }
        } catch (e: IOException) {
            Log.warning("[GpuDriverCatalog] download ${remote.id}: ${e.message}")
            return null
        } finally {
            connection.disconnect()
        }
    }

    @Throws(IOException::class)
    private fun httpGetText(url: String): String {
        val connection = openConnection(url, LIST_TIMEOUT_MS)
        try {
            if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                throw IOException("HTTP ${connection.responseCode}")
            }
            return connection.inputStream.bufferedReader().use { it.readText() }
        } finally {
            connection.disconnect()
        }
    }

    private fun openConnection(url: String, timeoutMs: Int): HttpURLConnection {
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.connectTimeout = timeoutMs
        connection.readTimeout = timeoutMs
        connection.setRequestProperty("User-Agent", USER_AGENT)
        return connection
    }

    private fun JsonObject.string(key: String): String =
        (this[key] as? JsonPrimitive)?.contentOrNull ?: ""
}
