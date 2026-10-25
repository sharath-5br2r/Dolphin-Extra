// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Partially based on:
// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

// Partially based on:
// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.os.Build
import kotlinx.serialization.SerializationException
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.model.GpuDriverMetadata
import java.io.BufferedInputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.zip.ZipInputStream

private const val GPU_DRIVER_META_FILE = "meta.json"

/** A driver package extracted under `GPUDrivers/Installed/<id>/`. */
data class InstalledGpuDriver(val id: String, val metadata: GpuDriverMetadata, val dir: File) {
    val libraryFile: File get() = File(dir, metadata.libraryName)
}

interface GpuDriverHelper {
    companion object {
        /**
         * Returns information about the system GPU driver.
         * @return `[driverId, driverVersion, deviceName, vendorId, apiVersion]`, or `null` if an
         * error occurred. Older native builds return only the first two entries.
         */
        private external fun getSystemDriverInfo(): Array<String>?

        /**
         * Queries the driver for custom driver loading support.
         * @return `true` if the device supports loading custom drivers, `false` otherwise
         */
        external fun supportsCustomDriverLoading(): Boolean

        /**
         * Queries the driver for manual max clock forcing support
         */
        external fun supportsForceMaxGpuClocks(): Boolean

        /**
         * Calls into the driver to force the GPU to run at the maximum possible clock speed
         * @param force Whether to enable or disable the forced clocks
         */
        external fun forceMaxGpuClocks(enable: Boolean)

        /**
         * Vulkan properties of the system driver, or `null` if it could not be probed. Probed once
         * per process: the native side loads the system Vulkan library and creates a throwaway
         * instance, and the answer cannot change while Dolphin is running.
         */
        private val cachedSystemGpuInfo: SystemGpuInfo? by lazy {
            SystemGpuInfo.fromJniArray(getSystemDriverInfo())
        }

        fun getSystemGpuInfo(): SystemGpuInfo? = cachedSystemGpuInfo

        /**
         * Fetches metadata about the system driver.
         * @return A [GpuDriverMetadata] object containing data about the system driver
         */
        fun getSystemDriverMetadata(context: Context): GpuDriverMetadata? {
            val info = getSystemGpuInfo() ?: return null
            return GpuDriverMetadata(
                name = context.getString(R.string.system_driver),
                vendor = info.driverId,
                driverVersion = info.driverVersion,
                description = context.getString(R.string.system_driver_desc),
                libraryName = ""
            )
        }

        private fun installedRoot() = File(DirectoryInitialization.getInstalledDriversDirectory())

        /**
         * Lists the drivers installed under `Installed/<id>/`, sorted by name. Directories whose
         * metadata is unreadable or whose library is missing are skipped. Migrates a driver left
         * in the legacy single-slot `Extracted/` directory first.
         *
         * @param settings Used to record the migrated driver as the active package; when `null`
         * the config is written and saved through [NativeConfig] directly.
         */
        fun listInstalled(settings: Settings? = null): List<InstalledGpuDriver> {
            migrateLegacyDriver(settings)

            val dirs = installedRoot().listFiles { f -> f.isDirectory } ?: return emptyList()
            return dirs.mapNotNull { dir ->
                val metadata = readMetadata(File(dir, GPU_DRIVER_META_FILE))
                    ?: return@mapNotNull null
                InstalledGpuDriver(dir.name, metadata, dir).takeIf { it.libraryFile.isFile }
            }.sortedBy { it.metadata.name.lowercase() }
        }

        fun getInstalledMetadata(id: String): GpuDriverMetadata? =
            readMetadata(File(File(installedRoot(), id), GPU_DRIVER_META_FILE))

        /** Removes the installed driver `id`. The caller is responsible for clearing the config. */
        fun delete(id: String) {
            File(installedRoot(), id).deleteRecursively()
        }

        /**
         * Installs a driver package into `Installed/<id>/`, replacing any previous install with
         * the same id. Nested paths inside the archive are flattened so the library and
         * `meta.json` always end up at the root of the driver directory; a `meta.json` is
         * synthesized for packs that ship exactly one bare `.so`.
         * @param stream InputStream of a driver package (zip)
         * @return The exit status of the installation process
         */
        fun installFromStream(stream: InputStream, id: String): GpuDriverInstallResult {
            val root = installedRoot()
            val targetDir = File(root, id)
            val tmpDir = File(root, "$id.tmp")
            tmpDir.deleteRecursively()
            if (!tmpDir.mkdirs()) {
                return GpuDriverInstallResult.InvalidArchive
            }

            fun fail(result: GpuDriverInstallResult): GpuDriverInstallResult {
                tmpDir.deleteRecursively()
                return result
            }

            try {
                extractFlattened(stream, tmpDir)
            } catch (e: IOException) {
                Log.warning("[GpuDriverHelper] Failed to extract driver package: ${e.message}")
                return fail(GpuDriverInstallResult.InvalidArchive)
            }

            val metadataFile = File(tmpDir, GPU_DRIVER_META_FILE)
            if (!metadataFile.isFile) {
                val libraries = tmpDir.listFiles { f -> f.isFile && f.name.endsWith(".so") }.orEmpty()
                if (libraries.size != 1) {
                    return fail(GpuDriverInstallResult.MissingMetadata)
                }
                metadataFile.writeText(GpuDriverMetadata.synthesize(id, libraries[0].name).toJson())
            }

            val metadata = try {
                GpuDriverMetadata.deserialize(metadataFile)
            } catch (e: SerializationException) {
                return fail(GpuDriverInstallResult.InvalidMetadata)
            }

            if (!File(tmpDir, metadata.libraryName).isFile) {
                return fail(GpuDriverInstallResult.MissingLibrary)
            }

            // Check that the device satisfies the driver's minimum Android version requirements
            if (Build.VERSION.SDK_INT < metadata.minApi) {
                return fail(GpuDriverInstallResult.UnsupportedAndroidVersion)
            }

            targetDir.deleteRecursively()
            if (!tmpDir.renameTo(targetDir)) {
                return fail(GpuDriverInstallResult.InvalidArchive)
            }
            return GpuDriverInstallResult.Success
        }

        /**
         * Extracts every file entry of the zip into [targetDir] by its base name only.
         * @exception IOException on a malformed archive or an entry that tries to escape the
         * target directory
         */
        @Throws(IOException::class)
        private fun extractFlattened(stream: InputStream, targetDir: File) {
            ZipInputStream(BufferedInputStream(stream)).use { zis ->
                while (true) {
                    val entry = zis.nextEntry ?: break
                    val name = entry.name
                    if (name.startsWith("/") || name.split('/', '\\').any { it == ".." }) {
                        throw IOException("Unsafe zip entry: $name")
                    }
                    // Skip directory entries and macOS resource-fork junk.
                    if (entry.isDirectory || name.startsWith("__MACOSX/")) continue
                    val fileName = name.substringAfterLast('/')
                    if (fileName.isEmpty()) continue
                    File(targetDir, fileName).outputStream().buffered().use { zis.copyTo(it) }
                }
            }
        }

        /**
         * Moves a driver installed by older Dolphin builds into the legacy single-slot
         * `Extracted/` directory into `Installed/<id>/`. When that driver was active
         * (`DriverLibName` set) the new id is recorded as the active package so the loader keeps
         * finding it. Idempotent: `Extracted/meta.json` is gone once the move has happened.
         */
        private fun migrateLegacyDriver(settings: Settings?) {
            val legacyDir = File(DirectoryInitialization.getExtractedDriverDirectory())
            val legacyMetadataFile = File(legacyDir, GPU_DRIVER_META_FILE)
            if (!legacyMetadataFile.isFile) return

            val metadata = readMetadata(legacyMetadataFile)
            val id = if (metadata != null) GpuDriverId.sanitize(metadata.label) else "legacy"
            val targetDir = File(installedRoot(), id)
            targetDir.deleteRecursively()
            if (!targetDir.mkdirs()) {
                Log.warning("[GpuDriverHelper] Could not create $targetDir for legacy driver")
                return
            }
            legacyDir.listFiles()?.forEach { file ->
                if (!file.renameTo(File(targetDir, file.name))) {
                    Log.warning("[GpuDriverHelper] Failed to move ${file.name} into $targetDir")
                }
            }
            Log.info("[GpuDriverHelper] Migrated legacy driver to Installed/$id")

            if (StringSetting.GFX_DRIVER_LIB_NAME.string.isEmpty()) return
            if (settings != null) {
                StringSetting.GFX_DRIVER_PACKAGE.setString(settings, id)
                settings.saveSettings()
            } else {
                StringSetting.GFX_DRIVER_PACKAGE.setString(NativeConfig.LAYER_BASE_OR_CURRENT, id)
                NativeConfig.save(NativeConfig.LAYER_BASE)
            }
        }

        private fun readMetadata(file: File): GpuDriverMetadata? {
            if (!file.isFile) return null
            return runCatching { GpuDriverMetadata.deserialize(file) }.getOrNull()
        }
    }
}

enum class GpuDriverInstallResult {
    Success,
    InvalidArchive,
    MissingMetadata,
    InvalidMetadata,
    MissingLibrary,
    UnsupportedAndroidVersion,
    AlreadyInstalled,
    FileNotFound
}
