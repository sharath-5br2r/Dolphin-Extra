// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.services.GameFileCacheManager

class GameDiscSelectionDialog : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val args = requireArguments()
        val paths = args.getStringArray(ARG_PATHS)!!
        val properties = args.getBoolean(ARG_PROPERTIES)
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(if (properties) R.string.disc_properties else R.string.select_disc)
            .setItems(args.getStringArray(ARG_LABELS)!!) { _, which ->
                if (properties) {
                    GamePropertiesDialog.newInstance(
                        GameFileCacheManager.addOrGet(paths[which]), false
                    )
                        .show(parentFragmentManager, GamePropertiesDialog.TAG)
                } else {
                    EmulationActivity.launch(
                        requireActivity(), args.getStringArray("$ARG_BOOT_PATHS$which")!!, false
                    )
                }
                finishLinkActivityIfNeeded()
            }
            .setNegativeButton(R.string.cancel) { _, _ -> finishLinkActivityIfNeeded() }
            .create()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        finishLinkActivityIfNeeded()
    }

    private fun finishLinkActivityIfNeeded() {
        if (requireArguments().getBoolean(ARG_FINISH_ACTIVITY)) {
            activity?.finish()
        }
    }

    companion object {
        private const val TAG = "GameDiscSelectionDialog"
        private const val ARG_PATHS = "paths"
        private const val ARG_LABELS = "labels"
        private const val ARG_BOOT_PATHS = "boot_paths_"
        private const val ARG_PROPERTIES = "properties"
        private const val ARG_FINISH_ACTIVITY = "finish_activity"

        fun launchGame(activity: FragmentActivity, game: GameFile, finishActivity: Boolean = false) {
            val discs = GameFileCacheManager.getDiscChoices(game)
            if (discs.isEmpty()) {
                EmulationActivity.launch(
                    activity, GameFileCacheManager.findSecondDiscAndGetPaths(game), false
                )
                if (finishActivity) activity.finish()
            } else {
                show(activity, discs, false, finishActivity)
            }
        }

        fun showProperties(activity: FragmentActivity, game: GameFile) {
            show(activity, GameFileCacheManager.getDiscChoices(game), true, false)
        }

        private fun show(
            activity: FragmentActivity,
            discs: List<GameFile>,
            properties: Boolean,
            finishActivity: Boolean
        ) {
            val manager = activity.supportFragmentManager
            if (discs.isEmpty() || manager.isStateSaved || manager.findFragmentByTag(TAG) != null) return

            // Label each disc by file name, falling back to the full path when names collide.
            val label = { disc: GameFile, name: String ->
                activity.getString(R.string.disc_selection_label, disc.getDiscNumber() + 1, name)
            }
            val shortLabels = discs.map { label(it, it.getFileName()) }
            val labelCounts = shortLabels.groupingBy { it }.eachCount()
            val labels = discs.mapIndexed { index, disc ->
                if (labelCounts.getValue(shortLabels[index]) > 1) label(disc, disc.getPath())
                else shortLabels[index]
            }
            // Store paths and launch choices now: rotation/rescans must not change an open chooser.
            val args = Bundle().apply {
                putStringArray(ARG_PATHS, discs.map { it.getPath() }.toTypedArray())
                putStringArray(ARG_LABELS, labels.toTypedArray())
                putBoolean(ARG_PROPERTIES, properties)
                putBoolean(ARG_FINISH_ACTIVITY, finishActivity)
                discs.forEachIndexed { index, disc ->
                    putStringArray(
                        "$ARG_BOOT_PATHS$index", GameFileCacheManager.findSecondDiscAndGetPaths(disc)
                    )
                }
            }
            GameDiscSelectionDialog().apply { arguments = args }.show(manager, TAG)
        }
    }
}
