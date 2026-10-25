# Declarative UI Slice 10: Audio Settings

**Status:** Complete

**Goal:** Move the Audio settings pane into a Qt Designer form while preserving DSP selection,
backend and device discovery, volume and latency controls, playback behavior, and Wii Remote audio
routing.

## Task 1: Define the complete form

- [x] Add `AudioPane.ui` with DSP Options, Backend Settings, Audio Playback Settings, Wii Remote
  Audio Routing, and Volume sections.
- [x] Keep the existing main-column and vertical-volume arrangement.
- [x] Define slider ranges, steps, labels, buddies, and explicit tab order in the form.
- [x] Keep platform- and backend-dependent rows form-owned and hide them when unavailable.

## Task 2: Bind settings and populate dynamic choices

- [x] Bind the DSP engine combo to its existing HLE/JIT setting pair.
- [x] Populate and bind the audio backend, WASAPI device, and Wii Remote device combos at runtime.
- [x] Bind Dolby quality through its existing enum values and bind all stock checkboxes and
  sliders to the existing settings.
- [x] Preserve the volume, latency, and audio-buffer value labels and the buffer's 8 ms stepping.

## Task 3: Preserve runtime behavior

- [x] Preserve backend-dependent Dolby, latency, volume, and WASAPI states.
- [x] Preserve controls disabled while emulation is active.
- [x] Preserve Wii Remote routing requirements for Cubeb, speaker data, Bluetooth passthrough,
  emulated remotes, and per-remote enablement.
- [x] Move all existing help titles and descriptions to `ConfigWidget::SetDescription`.

## Task 4: Preserve compatibility

- [x] Keep the existing 48 translated msgids and 17 explicit config symbols.
- [x] Preserve all 21 concrete setting locations represented by the platform and per-remote
  bindings.
- [x] Add a Qt form test for section order, placement, ranges, steps, buddies, tab order, and
  narrow layout fit.
- [x] Verify `AudioPane.cpp` constructs no permanent widget or layout.

Implementation checkpoint: exact commit `67c577ca738558d61a8fcad362b848600e968a17`
builds the app and both test binaries on macOS and Windows. The form test raises the Qt suite to
116 tests. The UI extraction regression suite passes, the targeted catalog retains the same 48
msgids, and the source audit retains the same 17 explicit config symbols covering 21 setting
locations.

## Task 5: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config comparisons.
- [x] Review Audio settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, dynamic choices, scrolling, tab traversal, and dependency
  states on Windows.

## Definition of done

- [x] The Designer form owns the complete Audio pane layout and permanent controls.
- [x] Existing discovery, configuration, help, side effects, and runtime restrictions remain
  intact.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Verification

- Exact implementation commit `67c577ca738558d61a8fcad362b848600e968a17` builds
  `dolphin-emu`, `qt-tests`, and `tests` on macOS. All 116 Qt tests pass. The core suite runs
  1,185 tests: 1,183 pass and the two real-preset shader tests skip because `SLANG_PRESET` is not
  set.
- The UI extraction regression suite passes. The targeted Audio catalog contains the same 48
  msgids before and after the migration, and the source audit contains the same 17 explicit
  config symbols covering 21 setting locations. `AudioPane.cpp` creates no permanent widget or
  layout.
- The rebuilt macOS app shows the complete Audio pane in the built-in light and dark themes.
  HLE, Cubeb, the 80 ms audio buffer, and 100% volume defaults remain intact. Dolby controls and
  Wii Remote routing have the expected disabled states, all labels fit, and the platform rows
  hidden on macOS leave no layout gaps.
- Windows Release builds the app and both test binaries at the same implementation commit. All
  116 Qt tests pass. The core suite runs 1,507 tests: 1,505 pass and the same two real-preset
  shader tests skip.
- Native Windows review passes in light and dark themes at 980 by 760 and 700 by 650. An isolated
  WASAPI profile exposes a populated Default Device row and enabled 20 ms latency control. Top
  and bottom scroll positions expose the complete pane and vertical volume control without
  horizontal clipping; the 80 ms buffer and 100% volume values fit; HLE keeps Dolby disabled;
  Wii Remote routing dependencies remain disabled; and keyboard traversal advances from Audio
  Buffer Size to Fill Audio Gaps to Preserve Audio Pitch.
