# Declarative UI Slice 23: Root Utility Dialogs

**Status:** Complete

**Goal:** Move the small root-level About, Discord join request, memory-card creation, NAND repair,
and NKit warning dialogs to Qt Designer forms without changing their decisions or side effects.

The approved design groups the remaining root-level dialogs into the standalone-dialog phase.
The larger conversion and manager surfaces have substantially more dynamic content, so this slice
finishes the compact utility dialogs first and leaves those managers for the next root-level batch.

## Task 1: Preserve the behavior boundary

- [x] Keep version formatting, external links, avatar download, card creation, NAND title lookup,
  warning decisions, and configuration writes in C++.
- [x] Keep optional avatar and NAND-removal content controlled dynamically.
- [x] Preserve window titles, icons, button roles, defaults, text wrapping, fixed sizing, and
  accept/reject behavior.
- [x] Move only permanent widgets, properties, focus order, and layout positioning to `.ui` files.

## Task 2: Migrate the dialogs

- [x] Add forms for About, Discord join request, memory-card creation, NAND repair, and NKit
  warning.
- [x] Replace static widget members with generated form ownership where needed.
- [x] Remove all C++ layout construction from the five dialog controllers.

## Task 3: Cover and verify

- [x] Add Qt form tests for permanent structure, defaults, button boxes, optional-content
  placeholders, and keyboard order.
- [x] Run XML, `uic`, formatting, translation extraction, and targeted string-parity checks.
- [x] Build the app and both test binaries and run both suites on macOS and Windows.
- [x] Review all five dialogs with native Qt rendering on both platforms.
- [x] Remove temporary profiles, scripts, captures, harnesses, and transferred changes.

## Definition of done

- [x] The five forms own every permanent widget and layout.
- [x] The existing decisions, side effects, strings, and translator comments remain intact.
- [x] macOS and Windows builds, tests, extraction, and visual review pass.

## Verification

- All five forms pass XML validation and Qt `uic` generation. The extraction regression passes,
  and the targeted catalog retains all 45 msgids and the complete Japanese-encoding translator
  note, with XML normalizing that note onto one line.
- macOS builds `dolphin-emu`, `qt-tests`, and `tests`. Qt passes 157/157 tests; core passes
  1,185/1,187 tests with the two expected preset-dependent skips.
- Native macOS renders confirm the fixed About layout, join-request buttons, memory-card defaults,
  optional NAND removal section, and complete NKit warning all fit without clipping.
- Windows builds the app and both test binaries with MSVC. Qt passes 157/157 tests; core passes
  1,507/1,509 tests with the same two expected skips.
- Bundled Qt 6.8.3 Windows renders confirm native button ordering, label wrapping, and optional
  content sizing across all five forms.
- The Windows checkout is reset to its original clean commit. Temporary patches, logs, scripts,
  harnesses, and captures were removed from both hosts.
