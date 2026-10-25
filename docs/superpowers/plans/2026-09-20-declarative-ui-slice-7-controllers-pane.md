# Declarative UI Slice 7: Controller Settings

**Status:** Complete

**Goal:** Move the Controllers settings pane and its GameCube, Wii Remote, and common-input
sections into Qt Designer forms while preserving device selection, mapping-window launches,
Bluetooth passthrough, asynchronous refresh actions, emulation-state restrictions, and config
saves.

PCSX2's controller pages model PlayStation-specific devices and do not provide a useful parity
feature for Dolphin's GameCube and Wii hardware. This slice is therefore a behavior-preserving
form migration, consistent with the design's explicit exclusion of controller-mapping parity
features.

## Task 1: Migrate the pane shell

- [x] Add `ControllersPane.ui` for the GameCube, Wii Remote, and common-input section order.
- [x] Keep construction of the three section controllers in C++ and place them into form-owned
  layout slots.
- [x] Preserve the shared settings scroll wrapper and top-aligned sizing.
- [x] Add a Qt form test for the permanent pane structure.

## Task 2: Migrate GameCube controller selection

- [x] Add `GamecubeControllersWidget.ui` for the four port rows.
- [x] Keep platform-dependent device choices and translated numbered port labels in C++.
- [x] Preserve device-to-menu mappings, Configure availability, NetPlay restrictions, config
  writes, and every mapping-dialog type.
- [x] Replace the custom non-default buttons with stock form buttons whose `autoDefault` property
  is disabled.

## Task 3: Migrate Wii Remote selection

- [x] Add `WiimoteControllersWidget.ui` for passthrough, emulated remotes, shared options, and
  refresh controls.
- [x] Keep Bluetooth adapter discovery and device entries dynamic.
- [x] Keep the Windows-only host Sync and Reset menu actions dynamic.
- [x] Preserve all emulation-state, NetPlay, controller-interface, continuous-scanning, and
  Balance Board dependencies.
- [x] Preserve asynchronous refresh indicator behavior and shutdown synchronization.

## Task 4: Migrate common input controls

- [x] Add `CommonControllersWidget.ui` for Background Input and the two standalone configuration
  dialogs.
- [x] Preserve explicit config persistence and both window-launch paths.
- [x] Preserve the translator note for the Common group title.

## Task 5: Preserve compatibility

- [x] Keep every existing config key and device enum mapping.
- [x] Keep all labels, group titles, combo choices, action text, and dialog text.
- [x] Keep config-change and emulation-state refresh connections.
- [x] Record the targeted gettext msgid comparison.
- [x] Verify no migrated section constructs a permanent control or layout in C++.

Implementation checkpoint: all four forms build in `dolphin-emu` and `qt-tests`; all 113 Qt tests
pass. The UI extraction regression suite passes, the targeted catalog retains the same 41 msgids,
and the source audit retains the same eight explicit config keys. The exact macOS app shows the
form-owned sections and expected enabled states in the light theme.

## Task 6: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite.
- [x] Review Controller settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, keyboard traversal, disabled states, and platform-only
  refresh actions on Windows.

## Definition of done

- [x] Four Designer forms own the Controllers pane and section layouts.
- [x] Device discovery, dynamic menu contents, and behavior remain in C++.
- [x] Existing controller selection, mapping, config persistence, and runtime restrictions remain
  intact.
- [x] macOS and Windows builds, tests, gettext checks, and visual review pass.

## Verification

- Exact implementation commit `17bfe9809e83cb9a48b5a86d5eccb119987dba7a` builds
  `dolphin-emu`, `qt-tests`, and `tests` on macOS. All 113 Qt tests pass. The core suite runs
  1,185 tests: 1,183 pass and the two real-preset shader tests skip because `SLANG_PRESET` is not
  set.
- The UI extraction regression suite passes. The targeted Controller catalog contains the same
  41 msgids before and after the migration, and the source audit contains the same eight explicit
  config keys.
- The rebuilt macOS app shows the complete GameCube, Wii Remote, and Common sections in the
  built-in light and dark themes. Enabled and disabled states, the fixed help panel, scrolling,
  and section spacing are intact.
- Windows builds the app and both test binaries at the same implementation commit. All 113 Qt
  tests pass. The core suite runs 1,507 tests: 1,505 pass and the same two real-preset shader tests
  skip.
- Native Windows review passes in light and dark themes at 980 by 760 and 700 by 650. Top, middle,
  and bottom scroll positions have no horizontal clipping; keyboard traversal advances from
  Port 1 and Wii Remote 1 to their Configure buttons; disabled controls remain legible; and the
  Refresh split menu exposes Refresh, Sync, and Reset.
