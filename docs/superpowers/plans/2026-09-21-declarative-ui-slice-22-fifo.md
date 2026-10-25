# Declarative UI Slice 22: FIFO

**Status:** Complete

**Goal:** Move the FIFO Player and FIFO Analyzer static layouts to Qt Designer forms without
changing playback, recording, decoding, or search behavior.

The approved design groups FIFO with the standalone-dialog migrations. Achievements and NetPlay
used slices 20 and 21 on this branch, so FIFO continues as slice 22.

## Task 1: Preserve the behavior boundary

- [x] Keep FIFO loading, saving, playback, recording, callbacks, range updates, and emulation-state
  handling in C++.
- [x] Keep analyzer tree population, command decoding, descriptions, search results, and debug-font
  updates in C++.
- [x] Preserve window geometry, splitter state, feature settings, button roles, and Escape-to-hide
  behavior.
- [x] Move only static structure, widget properties, labels, tab order, and layout positioning to
  `.ui` files.

## Task 2: Migrate the static layouts

- [x] Add a Designer form for the FIFO Player shell, play/record page, groups, range controls,
  recording controls, and button box.
- [x] Add a Designer form for the FIFO Analyzer tree, detail list, description browser, search
  splitters, and search controls.
- [x] Keep the analyzer tab and custom action-role buttons populated in C++ because they require
  runtime objects and `QDialogButtonBox` roles.
- [x] Replace legacy tooltip checkboxes with stock `QCheckBox` controls and the generic persistent
  description helper.
- [x] Express non-default search-button behavior as stock `QPushButton` properties in the form.
- [x] Remove all C++ layout construction from `DolphinQt/FIFO`.

## Task 3: Cover the form structure

- [x] Add Qt tests for the player tabs, groups, ranges, defaults, button box, analyzer splitters,
  search controls, and keyboard order.
- [x] Add the FIFO forms to DolphinQt and Qt test AUTOUIC inputs.
- [x] Run translation extraction and preserve existing strings and descriptions.

## Task 4: Verify and clean up

- [x] Build DolphinQt and both test binaries on macOS and run both test suites.
- [x] Review the play/record and analyzer tabs on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review the play/record and analyzer tabs on Windows.
- [x] Remove temporary profiles, scripts, captures, and transferred source changes from both
  hosts.

## Definition of done

- [x] No `new Q*Layout` construction remains in `DolphinQt/FIFO`.
- [x] FIFO retains its existing behavior, strings, dynamic data, settings, and persistence.
- [x] Both forms open as stock Qt widgets in Designer.
- [x] macOS and Windows builds, tests, extraction, and native review pass.

## Verification

- Both forms pass XML validation and Qt `uic` generation. The UI extraction regression passes,
  and a targeted before/after comparison retains all 51 FIFO msgids.
- macOS builds `dolphin-emu`, `qt-tests`, and `tests`. Qt passes 152/152 tests; core passes
  1,185/1,187 tests with the two expected preset-dependent skips.
- Native macOS review confirms the play/record groups, action buttons, analyzer splitters, and
  fixed-height search row retain their previous layout without clipping.
- Windows builds the app and both test binaries with MSVC. Qt passes 152/152 tests; core passes
  1,507/1,509 tests with the same two expected skips.
- Windows renders made with the repository's Qt 6.8.3 runtime confirm both tabs fit at the
  authored 600 by 580 size with native button ordering and no clipped controls.
- The Windows checkout is reset to its original clean commit. Temporary profiles, patches, logs,
  scripts, harnesses, and captures were removed from both hosts.
