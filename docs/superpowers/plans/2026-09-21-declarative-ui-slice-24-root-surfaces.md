# Declarative UI Slice 24: Remaining Root-Level Surfaces

**Status:** Complete

**Goal:** Move the remaining root-level cheat, conversion, status, memory-card, resource-pack,
Riivolution, and updater surfaces to Qt Designer forms without changing their runtime behavior.

This completes the standalone root-level phase before the controller-mapping, TAS, debugger, and
final cleanup slices.

## Task 1: Preserve the behavior boundary

- [x] Keep cheat-search sessions, enum item data, results, generated tabs, and game state in C++.
- [x] Keep conversion choices, compression rules, progress, file operations, and validation in C++.
- [x] Keep emulation metric formatting and visibility decisions in C++.
- [x] Keep memory-card slot data, table contents, menus, and file operations in C++.
- [x] Keep resource-pack discovery, selection, installation, priority, and table contents in C++.
- [x] Keep generated Riivolution disc, section, and option rows in C++.
- [x] Keep updater version text, release notes, button roles, settings writes, and update flow in C++.
- [x] Move permanent widgets, properties, focus order, and shell layout positioning to `.ui` files.

## Task 2: Migrate the surfaces

- [x] Add forms for the cheat-search factory, cheat-search session, and cheats manager.
- [x] Add forms for conversion, emulation status, memory-card manager, and resource-pack manager.
- [x] Add a form for the permanent Riivolution boot shell while retaining generated patch layouts.
- [x] Add a form for the update-available dialog.
- [x] Replace static widget construction with generated form ownership.
- [x] Remove C++ construction of every permanent root-level layout covered by this slice.

## Task 3: Cover and verify

- [x] Add Qt form tests for permanent structure, defaults, sizing, placeholders, and button order.
- [x] Run XML, `uic`, formatting, translation extraction, and targeted string-parity checks.
- [x] Build the app and both test binaries and run both suites on macOS and Windows.
- [x] Review representative forms with native Qt rendering on both platforms.
- [x] Remove temporary profiles, scripts, captures, harnesses, and transferred changes.

## Definition of done

- [x] The nine controllers use forms for all permanent widgets and shell layouts.
- [x] Runtime-generated controls, data, decisions, side effects, strings, and translator comments
      remain intact.
- [x] macOS and Windows builds, tests, extraction, and visual review pass.

## Verification

- All nine forms passed XML validation. Generated `uic` sources compiled on macOS and Windows, and
  formatting plus `git diff --check` passed.
- Translation extraction passed. A targeted before/after catalog comparison retained all 243
  message IDs and the existing translator context.
- macOS built the application, Qt test binary, and core test binary. All 166 Qt tests passed;
  1,185 of 1,187 core tests passed, with the two preset-dependent tests skipped as expected.
- Windows built the application and both test binaries with MSVC and Qt 6.8.3. All 166 Qt tests
  passed; 1,507 of 1,509 core tests passed, with the same two expected skips.
- Native Qt renders of all nine forms were reviewed on macOS and Windows. Controls, wrapping,
  spacing, sizing, disabled states, and platform button ordering rendered correctly.
- The Windows validation checkout was restored to its original clean commit. Local and remote
  patches, logs, scripts, render harnesses, captures, and temporary translation catalogs were
  removed.
