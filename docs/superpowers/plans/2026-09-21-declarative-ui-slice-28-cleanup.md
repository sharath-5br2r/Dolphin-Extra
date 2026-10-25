# Declarative UI Slice 28: Cleanup

**Status:** Complete

**Goal:** Remove the widget-subclass compatibility layer used before bind-after-construction,
retain the shared settings-help balloon through the binder module, and make the form/C++ layout
boundary enforceable for future changes.

## Task 1: Migrate the remaining compatibility-control users

- [x] Replace Free Look's `ConfigBool` and `ConfigChoice` instances with stock `QCheckBox` and
      `QComboBox` controls bound through `ConfigWidget`.
- [x] Replace Logitech microphone `ConfigBool` instances with stock bound checkboxes.
- [x] Replace SDL hint `ToolTipCheckBox` instances with stock checkboxes and binder-owned help
      metadata while preserving its string-valued SDL settings.
- [x] Preserve labels, option order, setting values, enablement, descriptions, signals, and
      emulation-state restrictions.

## Task 2: Remove the legacy control directories

- [x] Move `BalloonTip` and `ToolTipStyle`, which remain active binder infrastructure, into
      `Config/Binder`.
- [x] Update application and test includes, comments, and CMake source lists to use the binder
      location.
- [x] Remove the obsolete `ConfigControls` and tooltip widget-subclass sources.
- [x] Remove the legacy-only settings-help test while retaining coverage for help metadata on stock
      controls.

## Task 3: Enforce the declarative boundary

- [x] Add a configure-time check that rejects `new Q*Layout` construction in a controller's C++
      file when a same-name `.ui` form owns its permanent shell.
- [x] Document explicit exceptions for runtime-generated Riivolution option groups, the Triforce
      IP-redirection dialog, and TAS input groups.
- [x] Make newly added same-name form/controller pairs participate in the check automatically.

## Task 4: Verify and clean

- [x] Confirm no application or test source references `ConfigControls` or `ToolTipControls`.
- [x] Run formatting, diff checks, translation extraction, and targeted catalog parity.
- [x] Configure and build the application and both test binaries and run both suites on macOS and
      Windows.
- [x] Review Free Look, Logitech microphone, SDL hints, and settings-help behavior with native Qt
      rendering where available.
- [x] Remove temporary scripts, captures, profiles, catalogs, patches, and transferred changes.

## Definition of done

- [x] The legacy control directories are gone and the remaining users rely on stock Qt widgets.
- [x] Balloon help remains functional through `ConfigWidget::SetDescription`.
- [x] Form-backed controllers cannot silently reintroduce permanent C++ layouts.
- [x] macOS and Windows builds, tests, extraction, and representative visual review pass.

## Verification

- Implementation: `2ddfa98123 DolphinQt: remove legacy UI controls`.
- Source audit: no application or test source references the removed control directories or wrapper
  classes.
- Declarative boundary: a fresh macOS configure accepted the source tree; a same-name `.ui`/`.cpp`
  fixture containing `new QVBoxLayout` was rejected with the expected diagnostic.
- Formatting and source checks: `clang-format`, `git diff --check`, and
  `Languages/tests/test-ui-extraction.sh` passed.
- Translation parity: all 36 affected Free Look, SDL hints, and Logitech microphone source messages
  and translator comments remained present.
- macOS: the application built; Qt tests passed 189/189; core tests passed 1,185/1,187 with the two
  environment-dependent shader preset tests skipped.
- Windows: configure and the application, Qt test, and core test targets built under MSVC; Qt tests
  passed 189/189; core tests passed 1,507/1,509 with the same two skips.
- Native review: Free Look enablement and stock controls, the Logitech microphone manager, the SDL
  main and advanced tabs, the controller page at its default size, and the settings help panel
  rendered correctly on macOS.
- Cleanup: the local fixtures, catalog, profile, log, and patch were removed. The transferred patch
  was reversed on Windows, its scratch file was removed, no Dolphin process remained, and the
  Windows checkout was clean at `21ba25ee0e596a62b5709e5347890b14becc8739`.
