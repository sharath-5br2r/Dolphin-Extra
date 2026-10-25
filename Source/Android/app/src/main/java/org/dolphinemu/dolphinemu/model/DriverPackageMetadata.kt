// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import kotlinx.serialization.Serializable
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import java.io.File

/**
 * The `meta.json` manifest of an adrenotools GPU driver package.
 *
 * Decoding is lenient so packs from any of the community sources load: unknown keys are
 * ignored and every field has a default. Only `schemaVersion` is checked (absent or 1 is
 * accepted; anything else is rejected).
 */
@Serializable
data class GpuDriverMetadata(
    val schemaVersion: Int = SCHEMA_VERSION_V1,
    val name: String = "",
    val author: String = "",
    val packageVersion: String = "",
    val vendor: String = "",
    val driverVersion: String = "",
    val minApi: Int = 0,
    val description: String = "",
    val libraryName: String = DEFAULT_LIBRARY_NAME,
) {
    val label get() = "${name}-v${packageVersion}"

    fun toJson(): String = json.encodeToString(this)

    companion object {
        const val SCHEMA_VERSION_V1 = 1
        const val DEFAULT_LIBRARY_NAME = "libvulkan_freedreno.so"

        private val json = Json {
            ignoreUnknownKeys = true
            isLenient = true
            encodeDefaults = true
            prettyPrint = true
        }

        @Throws(SerializationException::class)
        fun parse(text: String): GpuDriverMetadata {
            val metadata = json.decodeFromString<GpuDriverMetadata>(text)
            if (metadata.schemaVersion != SCHEMA_VERSION_V1) {
                throw SerializationException(
                    "Unsupported metadata schema version ${metadata.schemaVersion}"
                )
            }
            return metadata
        }

        @Throws(SerializationException::class)
        fun deserialize(metadataFile: File): GpuDriverMetadata = parse(metadataFile.readText())

        /**
         * Builds a minimal manifest for packs that ship a bare `.so` without `meta.json`
         * (e.g. freedreno-builder releases).
         */
        fun synthesize(id: String, libraryName: String) = GpuDriverMetadata(
            name = id,
            description = "Manifest synthesized by Dolphin (package had no meta.json)",
            libraryName = libraryName,
        )
    }
}
