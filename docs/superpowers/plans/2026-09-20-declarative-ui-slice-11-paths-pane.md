# Declarative UI Slice 11: Paths Settings

**Status:** Complete

**Goal:** Move the Paths settings pane into a Qt Designer form while preserving game-folder
management, recursive scanning, background refresh, default-game selection, user-path editing, and
all browse actions.

## Task 1: Define the complete form

- [x] Add `PathPane.ui` with the Game Folders list, Add and Remove buttons, scan options, and six
  path rows.
- [x] Preserve the existing vertical arrangement, list spacing, button defaults, and stretch
  behavior.
- [x] Add buddies for every path label and an explicit tab order covering list, buttons,
  checkboxes, line edits, and browse buttons.
- [x] Keep the layout usable at the settings pane's narrow width without horizontal clipping.

## Task 2: Bind settings and preserve path behavior

- [x] Bind Search Subfolders and the default ISO to their existing typed config keys.
- [x] Bind the Wii NAND, dump, load, resource-pack, and WFS fields through `BindUserPath`.
- [x] Preserve background-refresh initialization and writes through `Settings`.
- [x] Preserve browse starting locations, native path separators, file filters, and immediate
  config updates.

## Task 3: Preserve game-folder behavior

- [x] Populate the list from `Settings::GetPaths` and retain PathAdded and PathRemoved updates.
- [x] Keep Remove disabled without a selection and remove the current path through `Settings`.
- [x] Preserve the recursive-scan game-list refresh side effect.
- [x] Preserve Add and Remove as non-default dialog buttons.

## Task 4: Preserve compatibility

- [x] Keep the existing 21 translated msgids and seven explicit config symbols.
- [x] Preserve all seven concrete config locations represented by browse and binding sites.
- [x] Add a Qt form test for structure, row placement, buddies, tab order, button defaults, list
  spacing, and narrow layout fit.
- [x] Verify `PathPane.cpp` constructs no permanent widget or layout.

Implementation checkpoint: exact commit `6d478e26cc33907cd978aea6518c9daa2a15b975`
builds the app and both test binaries on macOS and Windows. The form test raises the Qt suite to
117 tests. The UI extraction regression suite passes, the targeted catalog retains the same 21
msgids, and the source audit retains the same seven explicit config symbols and seven concrete
config locations.

## Task 5: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config comparisons.
- [x] Review Paths settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, scrolling, selection state, tab traversal, and path-field
  defaults on Windows.

## Definition of done

- [x] The Designer form owns the complete Paths pane layout and permanent controls.
- [x] Existing configuration, browsing, game-list side effects, and settings signals remain
  intact.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Verification

- Exact implementation commit `6d478e26cc33907cd978aea6518c9daa2a15b975` builds
  `dolphin-emu`, `qt-tests`, and `tests` on macOS. All 117 Qt tests pass. The core suite runs
  1,185 tests: 1,183 pass and the two real-preset shader tests skip because `SLANG_PRESET` is not
  set.
- The UI extraction regression suite passes. The targeted Paths catalog contains the same 21
  msgids before and after the migration, and the source audit contains the same seven explicit
  config symbols and seven concrete config locations. `PathPane.cpp` creates no permanent widget
  or layout.
- The rebuilt macOS app shows the complete Paths pane in the built-in light and dark themes.
  The empty game-folder list keeps Remove disabled, Search Subfolders starts unchecked,
  background refresh starts checked, Default ISO starts empty, and the five effective user paths
  are populated. Labels, path fields, and browse buttons remain aligned and fully visible.
- Windows Release builds the app and both test binaries at the same implementation commit. All
  117 Qt tests pass. The core suite runs 1,507 tests: 1,505 pass and the same two real-preset
  shader tests skip.
- Native Windows review passes in light and dark themes at 980 by 760 and 700 by 520. A populated
  game-folder list keeps Remove disabled until selection and enables it afterward. Search
  Subfolders, background refresh, Default ISO, and all effective user-path defaults are correct.
  The narrow layout scrolls from the game-folder section through WFS without horizontal clipping,
  and keyboard traversal advances through Add, Remove, both checkboxes, and Default ISO in the
  form-defined order.
