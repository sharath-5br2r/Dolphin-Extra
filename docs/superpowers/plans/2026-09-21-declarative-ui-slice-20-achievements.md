# Declarative UI Slice 20: Achievements

**Status:** Complete

**Goal:** Move the standalone Achievements window and its child widgets to Qt Designer forms
without changing RetroAchievements behavior.

The approved design called this standalone-dialog migration slice 19. The branch used slice 17 for
requested settings layout polish, slice 18 for the setup wizard, and slice 19 for cover management,
so this plan continues with slice 20.

## Task 1: Preserve the behavior boundary

- [x] Keep account login, logout, configuration, and confirmation behavior in C++.
- [x] Keep achievement and leaderboard data population dynamic.
- [x] Keep existing tab visibility, scrolling, update, and feature-flag behavior.
- [x] Move only static structure, widget properties, and layout positioning to `.ui` files.

## Task 2: Migrate the static layouts

- [x] Add Designer forms for the Achievements window, settings, header, achievement box, progress,
  and leaderboard widgets.
- [x] Replace settings-only tooltip checkbox subclasses with stock checkboxes and attach the same
  descriptions through `ConfigWidget`.
- [x] Add a reusable Designer-authored leaderboard cell so dynamic rows do not construct nested
  layouts in C++.
- [x] Remove all C++ layout construction from `DolphinQt/Achievements`.

## Task 3: Cover the form structure

- [x] Add Qt tests for the window shell, settings controls and tab order, progress overlays,
  dynamic-content layouts, and reusable leaderboard cells.
- [x] Add the Achievements forms to the Qt test AUTOUIC search path.
- [x] Run translation extraction and preserve existing strings and translator comments.

## Task 4: Verify and clean up

- [x] Build DolphinQt and both test binaries on macOS and run both test suites.
- [x] Review the logged-out Achievements window and its tabs on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review the Achievements window on Windows.
- [x] Remove temporary profiles, scripts, captures, and transferred source changes from both hosts.

## Definition of done

- [x] No `new Q*Layout` construction remains in `DolphinQt/Achievements`.
- [x] The Achievements window retains its existing behavior and strings.
- [x] Every new form opens as stock Qt widgets in Designer.
- [x] macOS and Windows builds, tests, extraction, and native review pass.

## Verification

- All seven forms passed XML validation and Qt 6.11.1 `uic` generation.
- Translation extraction passed with the existing strings, contexts, and translator comments.
- `clang-format --dry-run --Werror`, `git diff --check`, and the static search for C++ layout
  construction passed.
- macOS built `dolphin-emu`, `qt-tests`, and `tests`. All 145 Qt tests passed. The core suite
  passed 1,185 of 1,187 tests, with the two environment-dependent real-preset Slang tests skipped.
- macOS native review confirmed that the logged-out Settings tab fits at its default size and that
  enabling RetroAchievements exposes the account and feature controls without clipping.
- Windows built `Dolphin.exe`, `qt-tests.exe`, and `tests.exe` with MSVC 19.51 and Qt 6.8.3. All
  145 Qt tests passed. The core suite passed 1,507 of 1,509 tests, with the same two Slang tests
  skipped.
- Windows native review confirmed that both disabled and enabled integration states fit cleanly at
  native scale, with aligned stock controls and no clipped labels.
- The temporary macOS profile and all Windows patches, profiles, scheduled tasks, scripts,
  captures, processes, and transferred source changes were removed after validation.
