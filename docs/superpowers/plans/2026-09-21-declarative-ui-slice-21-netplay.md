# Declarative UI Slice 21: NetPlay

**Status:** Complete

**Goal:** Move the NetPlay setup, browser, room, mapping, selection, digest, and transfer dialogs
to Qt Designer forms without changing networking behavior.

The approved design groups NetPlay into the standalone-dialog phase. Achievements used slice 20 on
this branch, so NetPlay continues as slice 21.

## Task 1: Preserve the behavior boundary

- [x] Keep connection, hosting, refresh-thread, room, chat, player, and synchronization behavior in
  C++.
- [x] Keep game, player, region, session, progress, and table contents dynamically populated.
- [x] Keep existing feature-flag, configuration, geometry, splitter, menu, and confirmation
  behavior.
- [x] Move only static structure, widget properties, actions, menus, and layout positioning to
  `.ui` files.

## Task 2: Migrate the static layouts

- [x] Add Designer forms for the setup dialog, session browser, room dialog, game selector,
  controller mapping, digest progress, and chunked-transfer progress.
- [x] Promote `ClickBlurLabel` and express the existing non-default button behavior as stock
  `QPushButton` properties in the forms.
- [x] Keep dynamic progress rows and table contents in C++ while placing their containers in the
  forms.
- [x] Remove all C++ layout construction from `DolphinQt/NetPlay`.

## Task 3: Cover the form structure

- [x] Add Qt tests for each dialog shell, the setup tabs, filter controls, room menus and panes,
  mapping rows, dynamic progress containers, and keyboard order.
- [x] Add the NetPlay forms to the Qt test AUTOUIC search path.
- [x] Run translation extraction and preserve existing strings and translator comments.

## Task 4: Verify and clean up

- [x] Build DolphinQt and both test binaries on macOS and run both test suites.
- [x] Review the NetPlay setup, browser, and room surfaces on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review the NetPlay setup, browser, and room surfaces on Windows.
- [x] Remove temporary profiles, scripts, captures, and transferred source changes from both
  hosts.

## Definition of done

- [x] No `new Q*Layout` construction remains in `DolphinQt/NetPlay`.
- [x] NetPlay retains its existing behavior, strings, dynamic population, and feature flags.
- [x] Every new form opens as stock or explicitly promoted Qt widgets in Designer.
- [x] macOS and Windows builds, tests, extraction, and native review pass.

## Verification

- All seven forms pass XML validation and Qt `uic` generation. UI translation extraction passes.
- macOS builds `dolphin-emu`, `qt-tests`, and `tests`. Qt passes 150/150 tests; core passes
  1,185/1,187 tests with the two expected preset-dependent skips.
- Native macOS review covers the connection and hosting tabs, session browser, room controls,
  player table, chat pane, and room menus.
- Windows builds `dolphin-emu`, `qt-tests`, and `tests` with MSVC. Qt passes 150/150 tests; core
  passes 1,507/1,509 tests with the same two expected skips.
- Native Windows review covers the setup, browser, and room forms with the repository's Qt 6.8.3
  runtime. It exposed an expanding room menu bar; the form now fixes its vertical size policy and
  the Qt test asserts that constraint. The corrected room recapture is compact and unclipped.
- The Windows checkout is reset to its original clean commit. Temporary scheduled tasks,
  processes, patches, scripts, harnesses, profiles, and captures were removed from both hosts.
