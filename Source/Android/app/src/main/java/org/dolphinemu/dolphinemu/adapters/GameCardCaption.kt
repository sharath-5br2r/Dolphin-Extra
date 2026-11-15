// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import android.content.Context
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.services.GameFileCacheManager

/**
 * Caption shown on a game card: the disc count of a collapsed multi-disc entry, the disc number
 * of an uncollapsed multi-disc game, or otherwise the game's publisher.
 */
internal fun getGameCardCaption(context: Context, gameFile: GameFile): String {
    val discs = GameFileCacheManager.getDiscChoices(gameFile)
    return when {
        discs.isNotEmpty() -> context.getString(
            R.string.multi_disc_count, discs.distinctBy { it.getDiscNumber() }.size
        )
        GameFileCacheManager.findSecondDisc(gameFile) != null ->
            context.getString(R.string.disc_number, gameFile.getDiscNumber() + 1)
        else -> gameFile.getCompany()
    }
}
