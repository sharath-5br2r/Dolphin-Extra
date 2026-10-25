# Declarative UI Slice 9: On-Screen Display Settings

**Status:** Complete

**Goal:** Move the On-Screen Display settings pane into a Qt Designer form while preserving
message, performance, movie, NetPlay, debug-overlay, help, and movie-window dependency behavior.

This slice covers the desktop settings pane only. The fullscreen OSD remains a separate project
as defined by the desktop UI design.

## Task 1: Define the complete form

- [x] Add `OnScreenDisplayPane.ui` with General, Performance Statistics, Movie Window, Netplay,
  and Debug sections.
- [x] Keep the existing two-column arrangement within each section.
- [x] Define the font-size and performance-sample ranges and steps in the form.
- [x] Add explicit buddies and tab order for keyboard navigation.

## Task 2: Bind settings and preserve behavior

- [x] Bind all stock controls to the existing 21 typed config keys.
- [x] Preserve the movie-window dependency for rerecord, lag, frame, input, and clock controls.
- [x] Move every existing tooltip title and description to `ConfigWidget::SetDescription`.
- [x] Preserve the settings registry entries produced by the former config-control subclasses.

## Task 3: Preserve compatibility

- [x] Keep every existing key, label, range, step, default, and translated msgid.
- [x] Add a Qt form test for section order, grid placement, ranges, buddies, tab order, and narrow
  layout fit.
- [x] Verify `OnScreenDisplayPane.cpp` constructs no permanent widget or layout.

Implementation checkpoint: the form builds in `dolphin-emu` and `qt-tests`; all 115 Qt tests pass.
The UI extraction regression suite passes, the targeted catalog retains the same 49 msgids, and
the source audit retains the same 21 explicit config keys.

## Task 4: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config-key comparisons.
- [x] Review On-Screen Display settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, scrolling, tab traversal, and movie-window disabled states
  on Windows.

## Definition of done

- [x] The Designer form owns the complete On-Screen Display pane layout and permanent controls.
- [x] Existing configuration, help, ranges, and dependency behavior remain intact.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Verification

- Exact implementation commit `d8b40ac898668618d914897c3c78b0d9723382f0` builds
  `dolphin-emu`, `qt-tests`, and `tests` on macOS. All 115 Qt tests pass. The core suite runs
  1,185 tests: 1,183 pass and the two real-preset shader tests skip because `SLANG_PRESET` is not
  set.
- The UI extraction regression suite passes. The targeted On-Screen Display catalog contains the
  same 49 msgids before and after the migration, and the source audit contains the same 21
  explicit config keys.
- The rebuilt macOS app shows General, Performance Statistics, Movie Window, Netplay, and Debug
  in the built-in light and dark themes. The 13-point font and 1,000 ms sample defaults remain
  intact, all labels fit, and the five movie-window options enable and disable with their master
  checkbox.
- Windows Release builds the app and both test binaries at the same implementation commit. All
  115 Qt tests pass. The core suite runs 1,507 tests: 1,505 pass and the same two real-preset
  shader tests skip.
- Native Windows review passes in light and dark themes at 980 by 760 and 700 by 650. Top and
  bottom scroll positions expose the complete pane without horizontal clipping; the long
  performance-sample label fits at the narrow width; keyboard traversal advances from Show
  Messages to Font Size to Show FPS; and all five movie-window children switch from disabled to
  enabled and back with their master checkbox.
