# Declarative UI Slice 19: Cover Management

**Status:** Complete

**Goal:** Let users assign, replace, and remove game-library cover art while preserving Dolphin's
existing cover cache, custom-cover convention, and automatic GameTDB downloads.

The approved design called cover management slice 18. The branch used slice 17 for requested
settings layout polish and slice 18 for the setup wizard, so this plan continues with slice 19.

## Task 1: Define the managed-cover contract

- [x] Keep Dolphin's existing adjacent `<game name>.cover.png` custom-cover convention.
- [x] Normalize supported source images to PNG and write the result atomically.
- [x] Treat the shared `cover.png` fallback as externally managed so removing one game's cover
  cannot affect other games in the same directory.
- [x] Keep custom covers usable when automatic GameTDB cover downloads are disabled.

## Task 2: Refresh cached cover state

- [x] Detect custom-cover additions, replacements, and removals during metadata refresh.
- [x] Preserve custom-over-default cover priority and the existing banner fallback.
- [x] Repaint the grid after cover metadata changes without restarting Dolphin.

## Task 3: Add game-library actions

- [x] Add **Set Cover Image...** for a single selected game.
- [x] Confirm replacement when a managed custom cover already exists.
- [x] Add **Remove Custom Cover** only when Dolphin owns a removable per-game cover.
- [x] Report invalid images, write failures, and removal failures with native modal errors.
- [x] Trigger a metadata refresh after successful changes.

## Task 4: Cover the behavior

- [x] Add Qt tests for path derivation, PNG normalization, replacement, invalid input, and removal.
- [x] Keep the context actions out of multi-selection menus.
- [x] Run translation extraction and review the added strings.

## Task 5: Verify and clean up

- [x] Build DolphinQt and both test binaries on macOS and run both test suites.
- [x] Review set, replace, remove, automatic-download-disabled, light, and dark behavior on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review the context actions and cover refresh behavior on Windows.
- [x] Remove temporary games, images, scripts, captures, and transferred source changes from both
  hosts.

## Definition of done

- [x] A user can set, replace, and remove per-game cover art from the game library.
- [x] Grid cover changes appear without restarting Dolphin.
- [x] Shared fallback covers are never deleted by the per-game remove action.
- [x] Custom covers remain visible independently of automatic GameTDB downloads.
- [x] macOS and Windows builds, tests, extraction, and native review pass.

## Verification

- macOS built `dolphin-emu`, `qt-tests`, and `tests`.
- macOS Qt tests: 140 passed from 33 suites.
- macOS core tests: 1,185 passed and the two environment-dependent real-preset tests skipped from
  1,187 total.
- Windows built `dolphin-emu`, `qt-tests`, and `tests` with MSVC and Ninja.
- Windows Qt tests: 140 passed from 33 suites.
- Windows core tests: 1,507 passed and the same two environment-dependent real-preset tests skipped
  from 1,509 total.
- Translation extraction, `git diff --check`, and clang-format validation passed.
- Native macOS review confirmed portrait grid sizing, cover addition/replacement/removal refresh,
  banner fallback, custom covers with automatic downloads disabled, and light/dark presentation.
- Native Windows review exercised the actual context actions and confirmations. It confirmed that
  remove starts disabled, becomes enabled after setting a cover, replacement updates the displayed
  image and stored bytes, removal restores the banner fallback, and multi-selection omits cover
  actions.
- The Windows run exposed a test fixture that depended on an optional BMP writer; the fixture now
  uses Qt's built-in PNG support. A macOS native-style run also exposed manual nested-layout geometry
  in the Interface pane test; the test now activates the real layout hierarchy and passes on both
  platforms.
