// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

/** Immutable presentation snapshot. Group membership and ordering come from C++ UICommon. */
class GameListDiscGroups<T>(
    groups: List<List<T>>,
    private val path: (T) -> String,
    private val discNumber: (T) -> Int
) {
    private val groupsByPath = buildMap {
        for (group in groups) {
            val snapshot = group.toList()
            for (game in snapshot) {
                put(path(game), snapshot)
            }
        }
    }

    fun getDiscs(game: T): List<T> = groupsByPath[path(game)] ?: emptyList()

    fun collapse(games: List<T>): List<T> = games.filter { game ->
        val discs = getDiscs(game)
        discs.isEmpty() || path(discs.first()) == path(game)
    }

    fun getBootDiscs(game: T): List<T> = (listOf(game) + getDiscs(game)).distinctBy(discNumber)
}
