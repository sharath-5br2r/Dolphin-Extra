// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GameListDiscGroupsTest {
    private data class Disc(val path: String, val number: Int)

    private val first = Disc("a-disc1.iso", 0)
    private val second = Disc("disc2.iso", 1)
    private val third = Disc("disc3.iso", 2)
    private val copy = Disc("z-disc1.rvz", 0)
    private val unrelated = Disc("different-revision.iso", 1)

    private fun groups(vararg groups: List<Disc>) =
        GameListDiscGroups(groups.toList(), Disc::path, Disc::number)

    @Test
    fun onlyCollapsesMembersOfNativeGroupsWithoutChangingTheFullList() {
        val all = listOf(second, unrelated, copy, first)
        val grouped = groups(listOf(first, copy, second))
        assertEquals(listOf(unrelated, first), grouped.collapse(all))
        assertEquals(listOf(second, unrelated, copy, first), all)
        assertEquals(listOf(first, copy, second), grouped.getDiscs(second))
        assertTrue(grouped.getDiscs(unrelated).isEmpty())
    }

    @Test
    fun ungroupedAndEmptyLibrariesAreUnchanged() {
        val grouped = groups()
        assertEquals(listOf(first, copy, unrelated), grouped.collapse(listOf(first, copy, unrelated)))
        assertTrue(grouped.collapse(emptyList()).isEmpty())
    }

    @Test
    fun startingDiscTwoKeepsItFirstAndIncludesOtherDiscNumbers() {
        val grouped = groups(listOf(first, copy, second, third))
        assertEquals(listOf(second, first, third), grouped.getBootDiscs(second))
    }

    @Test
    fun selectingACopyDoesNotReplaceItWithAnotherCopyOfThatDisc() {
        val grouped = groups(listOf(first, copy, second))
        assertEquals(listOf(copy, second), grouped.getBootDiscs(copy))
    }

    @Test
    fun anUngroupedDiscBootsAlone() {
        assertEquals(listOf(unrelated), groups(listOf(first, second)).getBootDiscs(unrelated))
    }

    @Test
    fun sourceListChangesCannotChangeAnOpenChoiceSnapshot() {
        val source = mutableListOf(first, second)
        val grouped = groups(source)
        val choices = grouped.getDiscs(first)
        source.clear()
        assertEquals(listOf(first, second), choices)
        assertEquals(listOf(second, first), grouped.getBootDiscs(second))
    }

    @Test
    fun rescanRemovingTheFirstDiscRestoresTheRemainingEntry() {
        val before = groups(listOf(first, second))
        val after = groups()
        assertEquals(listOf(first), before.collapse(listOf(first, second)))
        assertEquals(listOf(second), after.collapse(listOf(second)))
        assertTrue(after.getDiscs(second).isEmpty())
    }
}
