# Declarative UI Slice 25: Controller Mapping

**Status:** Complete

**Goal:** Move the controller-mapping, control-expression, adapter, and alternate-input shells to
Qt Designer forms without changing input detection, binding, profile, device, or controller-group
behavior.

This slice covers the mapping window, its controller and hotkey pages, the advanced input/output
expression dialog, GameCube adapter settings, and DSU alternate-input configuration.

## Task 1: Preserve the runtime boundary

- [x] Keep controller-group discovery, binding buttons, numeric settings, indicators, calibration,
      and advanced mapping dialogs generated in C++.
- [x] Keep device enumeration, profile discovery and persistence, input detection, output testing,
      expression parsing, and controller updates in C++.
- [x] Keep Wii extension selection, extension-dependent visibility, and runtime attachment controls
      in C++.
- [x] Keep conditional controller-interface backends and DSU server data operations in C++.
- [x] Move permanent widgets, properties, focus order, and placement layouts to `.ui` files.

## Task 2: Migrate the mapping shells

- [x] Add forms for `MappingWindow`, `IOWindow`, and `GCPadWiiUConfigDialog`.
- [x] Add forms for the GameCube, Game Boy Advance, Wii Remote, extension, Free Look, and hotkey
      mapping pages.
- [x] Retain named empty layouts in the forms as insertion targets for runtime-generated
      controller groups.
- [x] Remove C++ construction of each permanent mapping-page and dialog layout.

## Task 3: Migrate alternate input sources

- [x] Add forms for `ControllerInterfaceWindow`, `DualShockUDPClientWidget`, and
      `DualShockUDPClientEditServerDialog`.
- [x] Preserve compile-time backend availability, server validation, storage, editing, and refresh
      behavior.
- [x] Remove C++ construction of each permanent alternate-input layout.

## Task 4: Cover and verify

- [x] Add Qt form tests for shell hierarchy, insertion layouts, defaults, visibility, sizing, text
      interaction, and button roles.
- [x] Run XML, `uic`, formatting, translation extraction, and targeted string-parity checks.
- [x] Build the application and both test binaries and run both suites on macOS and Windows.
- [x] Review representative controller, hotkey, expression, adapter, and alternate-input forms with
      native Qt rendering on both platforms.
- [x] Remove temporary profiles, scripts, captures, harnesses, and transferred changes.

## Definition of done

- [x] All permanent controller-mapping and alternate-input shells are form-owned.
- [x] Runtime-generated controls, data, decisions, side effects, strings, and translator comments
      remain intact.
- [x] macOS and Windows builds, tests, extraction, and visual review pass.

## Verification

- Implementation checkpoint: `cd72c10a62` (`DolphinQt: migrate controller mapping UI to forms`).
- XML validation, clang-format, `git diff --check`, and
  `Languages/tests/test-ui-extraction.sh` passed. The old and migrated targeted catalogs were
  identical at 188 messages, excluding the POT header.
- macOS built `dolphin-emu` and `qt-tests`; all 174 Qt tests passed. The core suite passed 1,185 of
  1,187 tests, with the two expected real-preset tests skipped because `SLANG_PRESET` was unset.
- Windows built `Dolphin.exe`, `qt-tests.exe`, and `tests.exe`; all 174 Qt tests passed. The core
  suite passed 1,507 of 1,509 tests, with the same two expected preset-dependent skips.
- Native Qt review covered the mapping window, expression editor, keyboard controller, Wii
  extension, motion input, adapter settings, alternate input window, DSU editor, and Free Look
  rotation on both platforms. The Windows review also exercised the forms at the host's constrained
  1,028-pixel desktop width.
- The Windows checkout was restored to `21ba25ee0e596a62b5709e5347890b14becc8739`; transferred
  patches, scripts, logs, harnesses, captures, and related processes were removed. Local slice
  scratch was also removed.
