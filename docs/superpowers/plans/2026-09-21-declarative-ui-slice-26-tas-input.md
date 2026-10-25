# Declarative UI Slice 26: TAS Input

**Status:** Complete

**Goal:** Move the shared and controller-specific TAS input shells to Qt Designer forms without
changing input overrides, turbo behavior, shortcuts, live controller values, attachment handling,
or stick and IR interaction.

This slice covers the shared TAS scroll and settings shell plus the GameCube, Game Boy Advance, and
Wii TAS input layouts.

## Task 1: Preserve the runtime boundary

- [x] Keep `TASCheckBox`, `TASSlider`, `TASSpinBox`, `StickWidget`, and `IRWidget` construction and
      signal wiring in C++.
- [x] Keep input-override registration, value conversion, keyboard shortcuts, turbo state, and
      live controller updates in C++.
- [x] Keep Wii extension and MotionPlus discovery, callbacks, visibility, resizing, and override
      selection in C++.
- [x] Move the scroll shell, settings controls, fixed group boxes, and placement layouts to `.ui`
      files.

## Task 2: Migrate the shared shell

- [x] Add a `TASInputWindow` form that owns the scroll area, content insertion layout, settings
      group, controller-input checkbox, turbo labels, and frame spin boxes.
- [x] Preserve scroll-bar policies, zero outer margins, control ranges, tooltips, buddies, and focus
      order.
- [x] Replace `SetupScrollArea` and C++ construction of the shared settings shell with form setup.

## Task 3: Migrate controller-specific layouts

- [x] Add forms for the GameCube, Game Boy Advance, and Wii TAS input content.
- [x] Retain named empty layouts as insertion targets for runtime-created stick, IR, slider, and
      button controls.
- [x] Preserve each controller's group titles, button order, spacers, aspect-ratio wrappers, and
      extension-dependent containers.
- [x] Remove C++ construction of permanent controller-specific group boxes and layouts.

## Task 4: Cover and verify

- [x] Add Qt form tests for shell hierarchy, insertion targets, settings defaults, labels, buddies,
      focus order, stretches, and scroll behavior.
- [x] Run XML, formatting, translation extraction, and targeted string-parity checks.
- [x] Build the application and both test binaries and run both suites on macOS and Windows.
- [x] Review representative GameCube, Game Boy Advance, Wii Remote, Nunchuk, and Classic Controller
      TAS layouts with native Qt rendering on both platforms.
- [x] Remove temporary scripts, captures, harnesses, and transferred changes.

## Definition of done

- [x] All permanent TAS input shells and fixed controller containers are form-owned.
- [x] Runtime controls, shortcuts, attachment decisions, input overrides, strings, and translator
      comments remain intact.
- [x] macOS and Windows builds, tests, extraction, and visual review pass.

## Verification

- Implementation checkpoint: `efcbf14e34` (`DolphinQt: migrate TAS input UI to forms`).
- XML validation, `clang-format`, `git diff --check`, and
  `Languages/tests/test-ui-extraction.sh` passed.
- Targeted old/new TAS translation catalogs were identical across 188 extracted messages,
  excluding the POT header.
- macOS built `DolphinQt`, `qt-tests`, and `tests`. Qt passed 178/178 tests. Core passed
  1,185/1,187 tests; the two real-preset tests skipped because `SLANG_PRESET` was not set.
- Windows Release built `dolphin-emu`, `qt-tests`, and `tests`. Qt passed 178/178 tests. Core passed
  1,507/1,509 tests with the same two expected skips.
- Native Qt renders on both platforms covered the shared settings shell, GameCube, Game Boy
  Advance, Wii Remote, Wii Remote plus Nunchuk, and Classic Controller layouts. The Windows
  Nunchuk layout correctly used a vertical scrollbar at the desktop height cap; a scrolled-bottom
  render confirmed that the Nunchuk Buttons and Settings groups remained complete and padded.
- The Windows checkout was reset to `21ba25ee0e596a62b5709e5347890b14becc8739`.
  Transferred patches, logs, harness files, captures, and local slice scratch were removed after
  review.
