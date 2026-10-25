// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

/**
 * Builds directory-safe ids for installed GPU drivers. The id becomes the path component that
 * adrenotools hands to dlopen, so it is restricted to lower-case `[a-z0-9._-]`.
 */
object GpuDriverId {
    private val DISALLOWED = Regex("[^a-z0-9._-]")
    private val ZIP_SUFFIX = Regex("\\.zip$", RegexOption.IGNORE_CASE)

    fun sanitize(raw: String): String = raw.lowercase().replace(DISALLOWED, "_")

    /** `<prefix>-<tag>-<assetBase>`; empty parts are omitted along with their dash. */
    fun makeId(prefix: String, tag: String, assetName: String): String {
        val base = assetName.replace(ZIP_SUFFIX, "")
        return sanitize(listOf(prefix, tag, base).filter { it.isNotEmpty() }.joinToString("-"))
    }
}
