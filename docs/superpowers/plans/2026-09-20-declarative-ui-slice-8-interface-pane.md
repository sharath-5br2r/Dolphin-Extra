# Declarative UI Slice 8: Interface Settings

**Status:** Complete

**Goal:** Move the Interface settings pane into a Qt Designer form while preserving language,
theme, style, game-library metadata, debugging, render-window, cursor, and emulation-state
behavior.

The visible PCSX2-style theme and settings-shell work already landed in Slices 2 and 4. This slice
therefore keeps Dolphin's Interface feature set and moves its permanent structure to the shared
declarative UI system.

## Task 1: Migrate the User Interface section

- [x] Add `InterfacePane.ui` with Language, Theme, and Style rows and all user-interface
  checkboxes.
- [x] Keep language, theme-directory, and custom-style discovery dynamic in C++.
- [x] Bind stock form controls to the existing typed settings.
- [x] Preserve the language restart notice, immediate style application, title-database refresh,
  and cover metadata refresh.

## Task 2: Migrate the Render Window section

- [x] Define all render-window checkboxes and the nested Mouse Cursor Visibility group in the
  form.
- [x] Bind the three cursor radio buttons to the existing enum values.
- [x] Keep Lock Mouse Cursor hidden outside Windows and preserve its signal path on Windows.
- [x] Preserve Keep Window on Top, Confirm on Stop, panic handler, active-title, and focus-loss
  behavior.

## Task 3: Preserve runtime restrictions and help

- [x] Keep Debugging UI synchronized with the debug-mode state and disabled in Hardcore Mode.
- [x] Keep play-time tracking disabled while emulation is active.
- [x] Move every existing tooltip title and description to `ConfigWidget::SetDescription`.
- [x] Preserve the settings registry entries produced by the former config-control subclasses.

## Task 4: Preserve compatibility

- [x] Keep every existing config key, style value, cursor enum mapping, label, and dynamic option.
- [x] Preserve the existing translated msgids and the non-Latin language display names.
- [x] Add a Qt form test for section order, form buddies, stock controls, tab order, and narrow
  layout fit.
- [x] Verify `InterfacePane.cpp` constructs no permanent widget or layout.

Implementation checkpoint: the form builds in `dolphin-emu` and `qt-tests`; all 114 Qt tests pass.
The UI extraction regression suite passes, the targeted catalog retains the same 51 msgids, and
the source audit retains the same 14 explicit config keys. The exact macOS app shows the
form-owned sections and expected idle states in the light and dark themes.

## Task 5: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config-key comparisons.
- [x] Review Interface settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, tab traversal, dynamic choices, disabled states, and the
  Windows-only Lock Mouse Cursor control.

## Definition of done

- [x] The Designer form owns the complete Interface pane layout and permanent controls.
- [x] Dynamic discovery and runtime behavior remain in C++.
- [x] Existing configuration, help, theme, refresh, and emulation-state behavior remain intact.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Verification

- Exact implementation commit `ba848fee121badc79861f56c12e9afb387f11ef7` builds
  `dolphin-emu`, `qt-tests`, and `tests` on macOS. All 114 Qt tests pass. The core suite runs
  1,185 tests: 1,183 pass and the two real-preset shader tests skip because `SLANG_PRESET` is not
  set.
- The UI extraction regression suite passes. The targeted Interface catalog contains the same
  51 msgids before and after the migration, and the source audit contains the same 14 explicit
  config keys.
- The rebuilt macOS app shows the complete User Interface and Render Window sections in the
  built-in light and dark themes. Language, Theme, and Style are populated, Lock Mouse Cursor is
  omitted as expected, and the form has no clipping or spacing defects.
- Windows Release builds the app and both test binaries at the same implementation commit. All
  114 Qt tests pass. The core suite runs 1,507 tests: 1,505 pass and the same two real-preset
  shader tests skip.
- Native Windows review passes in light and dark themes at 980 by 760 and 700 by 650. Top and
  bottom scroll positions have no horizontal clipping; the long GameTDB cover label fits at the
  narrow width; Language, Theme, and Style are populated; keyboard traversal advances from
  Language to Theme to Style; idle enabled states remain intact; and Lock Mouse Cursor is visible.
