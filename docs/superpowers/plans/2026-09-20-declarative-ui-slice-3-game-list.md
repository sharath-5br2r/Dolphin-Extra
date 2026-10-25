# Declarative UI Slice 3: Game Library

**Status:** Complete

**Goal:** Move the game-library shell into a Qt Designer form and add compact, always-available
view and filtering controls while preserving the existing list, grid, menu, launch, and empty
library behavior.

## Task 1: Migrate the existing shell

- [x] Add `GameListWidget.ui` for the view stack and current search controls.
- [x] Convert `GameList` from a `QStackedWidget` into the form's controller.
- [x] Remove the separate C++-authored `SearchBar` layout.
- [x] Preserve `Ctrl+F`, Escape, Close, search filtering, and the empty-library double-click action.
- [x] Add a Qt form test and verify the behavior-preserving migration.

## Task 2: Add the library control row

- [x] Keep search visible and make `Ctrl+F` focus and select it.
- [x] Add accessible List and Grid buttons synchronized with the View menu.
- [x] Add transient platform and region quick filters without replacing the existing multi-select
  View menu filters.
- [x] Add a grid-scale slider synchronized with the existing zoom shortcuts and saved scale.
- [x] Keep fixed view controls visible while search and filter fields compress at narrow widths.

## Task 3: Preserve compatibility

- [x] Keep saved preferred-view, grid-scale, table-header, and column-visibility settings.
- [x] Keep all existing platform and country visibility config keys in the filtering path.
- [x] Record the `.pot` msgid diff and retain moved search strings.
- [x] Keep list/grid selection, sorting, context menus, keyboard launch, and game-count updates.

## Task 4: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Review light and dark list, grid, empty, filtered, and narrow-window states on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review light and dark controls, keyboard traversal, focus, and narrow-window behavior on
  Windows.

## Verification notes

- macOS built `dolphin-emu` and `qt-tests`; `qt-tests` passed all 99 tests, and `tests`
  passed 1183 tests with the same two environment-dependent shader tests skipped.
- Windows built `dolphin-emu`, `qt-tests`, and `tests`; native `qt-tests -platform windows`
  passed all 99 tests, and `tests` passed 1505 tests with the same two environment-dependent
  shader tests skipped.
- The UI extraction regression suite passed.
- Light and Dark themes were reviewed with empty, populated list, populated grid, and filtered
  libraries. `Ctrl+F`, Escape, View-menu synchronization, direct view buttons, and zoom shortcut
  synchronization were exercised. The form test also lays out the row at 360 pixels and keeps both
  32-pixel view buttons and the 80-pixel scale control intact.
- Windows light and dark list and grid states were reviewed at 1100 by 720 and 520 by 640 pixels.
  At the narrow size, both view buttons remained 32 pixels wide, the scale control remained 84
  pixels wide, and both filters and search remained visible. Right Arrow moved focus from List to
  Grid and activated it; Tab then traversed the enabled scale, platform, region, search, and grid
  controls. `Ctrl+F` focused search, and Escape cleared it and returned focus to the active view.
- Compared with the form-migration checkpoint, `Search games...` remains in extraction and
  `Close` is removed. The new control labels, accessibility names, platform names, and region names
  are extracted.
- Follow-up grid polish reserves a 160 by 224 portrait cover canvas for every fallback game card
  and scales 16-pixel card and window-edge spacing with the saved grid scale. Both changes were
  reviewed in Light and Dark themes at wide and narrow sizes on macOS and Windows; all 102 Qt tests
  continued to pass.

## Definition of done

- [x] `GameListWidget.ui` owns the game-library layout.
- [x] Search, platform, and region controls are always available.
- [x] View buttons, View menu actions, slider, and zoom shortcuts stay synchronized.
- [x] Existing saved settings and multi-select menu filters retain their meaning.
- [x] macOS and Windows builds, tests, gettext checks, and visual review pass.
