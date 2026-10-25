# Declarative UI Slice 12: GameCube Settings

**Status:** Complete

**Goal:** Move the GameCube settings pane into a Qt Designer form while preserving IPL settings,
EXI device selection and configuration, memory-card and cartridge path validation, optional mGBA
paths, NetPlay restrictions, and live device changes.

## Task 1: Define the complete form

- [x] Add `GameCubePane.ui` with IPL Settings, Device Settings, and GBA Settings groups.
- [x] Define the Slot A, Slot B, and SP1 rows plus form-owned memory-card, GCI-folder, AGP, and
  override-warning rows for both memory-card slots.
- [x] Define the BIOS, five ROM, save-location, and saves rows for builds with mGBA support.
- [x] Add buddies, word wrapping, non-default browse/config buttons, and an explicit tab order.
- [x] Keep the complete pane usable at the settings pane's narrow width without horizontal
  clipping.

## Task 2: Bind settings and populate dynamic choices

- [x] Bind Skip Main Menu, System Language, the mGBA BIOS and saves paths, all five ROM paths, and
  the same-directory checkbox to their existing typed settings.
- [x] Populate Slot A and Slot B with the existing EXI device choices and SP1 with its existing
  platform-dependent choices and values.
- [x] Keep device selection writes grouped under the existing config-change guard and preserve
  live `ChangeDevice` calls while emulation is running.
- [x] Keep the IPL availability description and disabled state tied to installed IPL ROMs.

## Task 3: Preserve conditional device behavior

- [x] Keep each slot's config button enabled only for devices with a configuration action.
- [x] Show memory-card, GCI-folder, and AGP path rows only for the selected device and only when
  custom paths are active.
- [x] Keep GCI override warnings slot-specific, hidden by default, and visible only when the
  selected folder device has an override.
- [x] Preserve microphone mapping, broadband adapter dialogs, and Triforce window dispatch.

## Task 4: Preserve path and mGBA behavior

- [x] Preserve memory-card filenames, region validation, card validity checks, duplicate-slot
  rejection, and live reinsertion.
- [x] Preserve GCI-folder default reset, region validation, duplicate-slot rejection, and live
  reinsertion.
- [x] Preserve AGP browsing and live cartridge changes.
- [x] Preserve mGBA browse filters, effective user paths, same-directory dependency, and NetPlay
  enablement.

## Task 5: Preserve compatibility

- [x] Keep the existing 43 translated msgids and 11 explicit config symbols.
- [x] Preserve all 21 concrete config locations represented by bindings, paths, and dynamic slot
  lookups.
- [x] Add a Qt form test for group order, row placement, language choices, buddies, initial
  visibility, tab order, button defaults, and narrow layout fit.
- [x] Verify `GameCubePane.cpp` constructs no permanent widget or layout.

Implementation checkpoint: exact commit `8a283de466e4771701e56068c983f237fe172303` builds the app
and both test binaries on macOS and Windows. The form test raises the Qt suite to 118 tests. The UI
extraction regression suite passes, the targeted catalog retains the same 43 msgids, and the
source audit retains the same 11 explicit config symbols and 21 concrete config locations.

## Task 6: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config comparisons.
- [x] Review GameCube settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, scrolling, dynamic device rows, tab traversal, and mGBA
  dependency states on Windows.

## Definition of done

- [x] The Designer form owns the complete GameCube pane layout and permanent controls.
- [x] Existing configuration, validation, dialogs, runtime side effects, and settings signals
  remain intact.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Verification

- Exact implementation commit `8a283de466e4771701e56068c983f237fe172303` builds
  `dolphin-emu`, `qt-tests`, and `tests` on macOS. All 118 Qt tests pass. The core suite runs
  1,185 tests: 1,183 pass and the two real-preset shader tests skip because `SLANG_PRESET` is not
  set.
- The UI extraction regression suite passes. The targeted GameCube catalog contains the same 43
  msgids before and after the migration, and the source audit contains the same 11 explicit
  config symbols and 21 concrete config locations. `GameCubePane.cpp` creates no permanent widget
  or layout.
- The rebuilt macOS app shows the complete GameCube pane in the built-in light and dark themes.
  IPL availability, authored language choices, default EXI devices, effective GBA paths, and the
  same-directory saves dependency all retain their prior behavior. Labels, fields, and buttons
  remain aligned and fully visible.
- Windows Release builds the app and both test binaries at the same implementation commit. All
  118 Qt tests pass. The core suite runs 1,507 tests: 1,505 pass and the same two real-preset
  shader tests skip.
- Native Windows review passes in light and dark themes at 980 by 760 and 700 by 520. The
  slot-specific GCI override warning and custom GCI and Advance Game Port rows appear for the
  selected devices, and the same-directory checkbox disables and restores the Saves controls.
  The narrow layout scrolls from IPL Settings through GBA Settings without horizontal clipping,
  and keyboard traversal follows the form-defined device and GBA control order.
