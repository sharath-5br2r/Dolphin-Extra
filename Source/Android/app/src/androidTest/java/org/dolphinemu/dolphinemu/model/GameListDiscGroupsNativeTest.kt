// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import android.content.Intent
import android.os.SystemClock
import android.util.TypedValue
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.AppLinkActivity
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.utils.AppLinkHelper
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.junit.After
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class GameListDiscGroupsNativeTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private lateinit var directory: File

    @Before
    fun setUp() {
        waitUntil { DirectoryInitialization.areDolphinDirectoriesReady() }
        directory = File.createTempFile("disc-groups", "", instrumentation.targetContext.cacheDir)
        assertTrue(directory.delete())
        assertTrue(directory.mkdir())
    }

    @After
    fun tearDown() {
        if (this::directory.isInitialized) directory.deleteRecursively()
    }

    private fun disc(name: String, number: Int, revision: Int = 0, id: String = "GAFE01"): GameFile {
        // Metadata-only synthetic GameCube header; no game data or bootable image.
        val header = ByteArray(0x3000)
        id.toByteArray().copyInto(header)
        header[6] = number.toByte()
        header[7] = revision.toByte()
        byteArrayOf(0xc2.toByte(), 0x33, 0x9f.toByte(), 0x3d).copyInto(header, 0x1c)
        header[0x422] = 0x10
        val file = File(directory, name)
        file.writeBytes(header)
        return requireNotNull(GameFile.parse(file.path))
    }

    @Test
    fun jniUsesDesktopDiscAndPathOrder() {
        val second = disc("disc2.iso", 1)
        val copy = disc("z-disc1.iso", 0)
        val first = disc("a-disc1.iso", 0)
        val groups = GameFileCache.getDiscGroups(arrayOf(second, copy, first))
        assertEquals(1, groups.size)
        assertArrayEquals(intArrayOf(2, 1, 0), groups.single())
        assertEquals("a-disc1.iso", first.getFileName())
    }

    @Test
    fun jniKeepsDifferentRevisionsRegionsAndSingleDiscCopiesSeparate() {
        val first = disc("disc1.iso", 0)
        val copy = disc("copy.iso", 0)
        val revision = disc("revision.iso", 1, revision = 1)
        val region = disc("region.iso", 1, id = "GAFP01")
        val otherTitle = disc("other.iso", 1, id = "GZLE01")
        assertTrue(GameFileCache.getDiscGroups(arrayOf(first, copy, revision, region, otherTitle)).isEmpty())
    }

    @Test
    fun settingRefreshesTheLibraryButRetainsAllDiscsAndHandlesRemoval() {
        val first = disc("disc1.iso", 0)
        val second = disc("disc2.iso", 1)
        val revision = disc("revision.iso", 1, revision = 1)
        val oldPaths = GameFileCache.getIsoPaths()
        val oldCollapse = BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.boolean
        try {
            instrumentation.runOnMainSync {
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, false)
                GameFileCache.setIsoPaths(arrayOf(directory.path))
                GameFileCacheManager.startLoad()
                GameFileCacheManager.startRescan()
            }
            waitUntil {
                GameFileCacheManager.getGameFiles().value?.map { it.getPath() }?.toSet() ==
                        setOf(first.getPath(), second.getPath(), revision.getPath()) &&
                        !GameFileCacheManager.isLoadingOrRescanning()
            }
            waitUntil { GameFileCacheManager.getLibraryGameFiles().value?.size == 3 }
            instrumentation.runOnMainSync {
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, true)
            }
            waitUntil { GameFileCacheManager.getLibraryGameFiles().value?.size == 2 }
            assertEquals(3, GameFileCacheManager.getGameFiles().value!!.size)
            assertEquals(setOf(first.getPath(), revision.getPath()),
                GameFileCacheManager.getLibraryGameFiles().value!!.map { it.getPath() }.toSet())
            instrumentation.runOnMainSync {
                assertArrayEquals(arrayOf(second.getPath(), first.getPath()),
                    GameFileCacheManager.findSecondDiscAndGetPaths(second))
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, false)
            }
            waitUntil { GameFileCacheManager.getLibraryGameFiles().value?.size == 3 }
            instrumentation.runOnMainSync {
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, true)
            }
            waitUntil { GameFileCacheManager.getLibraryGameFiles().value?.size == 2 }
            assertTrue(File(first.getPath()).delete())
            instrumentation.runOnMainSync { GameFileCacheManager.startRescan() }
            waitUntil {
                !GameFileCacheManager.isLoadingOrRescanning() &&
                        GameFileCacheManager.getLibraryGameFiles().value?.map { it.getPath() }?.toSet() ==
                        setOf(second.getPath(), revision.getPath())
            }
        } finally {
            instrumentation.runOnMainSync {
                GameFileCache.setIsoPaths(oldPaths)
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, oldCollapse)
                GameFileCacheManager.startRescan()
            }
            waitUntil { !GameFileCacheManager.isLoadingOrRescanning() }
        }
    }

    @Test
    fun appLinkHasAMaterialThemeAndShowsTheDiscChooserWithoutALibraryActivity() {
        val first = disc("disc1.iso", 0)
        val second = disc("disc2.iso", 1)
        val oldPaths = GameFileCache.getIsoPaths()
        val oldCollapse = BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.boolean
        try {
            instrumentation.runOnMainSync {
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, true)
                GameFileCache.setIsoPaths(arrayOf(directory.path))
                GameFileCacheManager.startLoad()
                GameFileCacheManager.startRescan()
            }
            waitUntil {
                GameFileCacheManager.getGameFiles().value?.map { it.getPath() }?.toSet() ==
                        setOf(first.getPath(), second.getPath()) &&
                        !GameFileCacheManager.isLoadingOrRescanning()
            }
            val intent = Intent(instrumentation.targetContext, AppLinkActivity::class.java)
                .setAction(Intent.ACTION_VIEW)
                .setData(AppLinkHelper.buildGameUri(0, first.getGameId()))
            ActivityScenario.launch<AppLinkActivity>(intent).use { scenario ->
                scenario.onActivity { activity ->
                    val value = TypedValue()
                    assertTrue(activity.theme.resolveAttribute(
                        com.google.android.material.R.attr.isMaterialTheme, value, true
                    ))
                    assertTrue(value.data != 0)
                }
                onView(withText(R.string.select_disc)).check(matches(isDisplayed()))
                onView(withText(instrumentation.targetContext.getString(
                    R.string.disc_selection_label, 1, first.getFileName()
                ))).check(matches(isDisplayed()))
                onView(withText(instrumentation.targetContext.getString(
                    R.string.disc_selection_label, 2, second.getFileName()
                ))).check(matches(isDisplayed()))
                scenario.recreate()
                onView(withText(R.string.select_disc)).check(matches(isDisplayed()))
                onView(withText(R.string.cancel)).perform(click())
                waitUntil { scenario.state == Lifecycle.State.DESTROYED }
            }
        } finally {
            instrumentation.runOnMainSync {
                GameFileCache.setIsoPaths(oldPaths)
                BooleanSetting.MAIN_COLLAPSE_MULTI_DISC_GAMES.setBoolean(NativeConfig.LAYER_BASE, oldCollapse)
                GameFileCacheManager.startRescan()
            }
            waitUntil { !GameFileCacheManager.isLoadingOrRescanning() }
        }
    }

    private fun waitUntil(condition: () -> Boolean) {
        val deadline = SystemClock.elapsedRealtime() + 30_000
        while (!condition() && SystemClock.elapsedRealtime() < deadline) SystemClock.sleep(25)
        assertTrue("Timed out waiting for library initialization/update", condition())
    }
}
