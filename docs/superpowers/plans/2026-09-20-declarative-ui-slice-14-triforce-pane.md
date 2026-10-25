# Declarative UI Slice 14: Triforce Settings

**Status:** Complete

**Goal:** Move the Triforce settings pane into a Qt Designer form while preserving controller
mapping dispatch and the existing IP-redirection editor and config behavior.

## Task 1: Define the complete pane form

- [x] Add `TriforcePane.ui` with Controllers and IP Address Redirections groups plus the trailing
  stretch.
- [x] Use stock Configure buttons with non-default behavior and an explicit tab order.
- [x] Keep both groups aligned and usable at the settings pane's narrow width.

## Task 2: Preserve action dispatch

- [x] Keep the controller action opening the AM Baseboard mapping window for device 0.
- [x] Preserve delete-on-close, window-modal behavior, and non-blocking display for the mapping
  window.
- [x] Keep the IP Address Redirections action opening its editor with delete-on-close behavior.

## Task 3: Preserve the IP-redirection editor

- [x] Leave the existing dialog layout and table behavior unchanged for the later standalone
  dialog migration.
- [x] Preserve the Emulated, Real, and Description columns, stretch sizing, edit triggers, and
  final empty row.
- [x] Preserve automatic row insertion and removal, whitespace handling, incomplete-row skipping,
  descriptions, and comma-separated serialization.
- [x] Preserve Default's immediate config reset, Clear's empty-row state, and OK-only saving.

## Task 4: Preserve compatibility

- [x] Keep the existing eight translated msgids.
- [x] Preserve `MAIN_TRIFORCE_IP_REDIRECTIONS` at its three existing read, save, and reset sites.
- [x] Add a Qt form test for group order, button placement, button defaults, tab order, and narrow
  layout fit.
- [x] Verify the `TriforcePane` constructor constructs no permanent widget or layout; the nested
  dialog remains outside this slice's form boundary.

## Task 5: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config comparisons.
- [x] Review the pane and IP-redirection editor in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow pane layouts, tab traversal, mapping-window dispatch, and
  IP-redirection dialog actions on Windows.

## Definition of done

- [x] The Designer form owns the complete Triforce settings pane layout and permanent controls.
- [x] Both Configure actions and the IP-redirection editor retain their existing behavior.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Implementation checkpoint

- Exact implementation commit:
  `9f645407fba4d79b5bb7fc3f57ac9805be014afc`
  (`DolphinQt: define Triforce settings in a form`).

## Verification

- macOS built `dolphin-emu`, `qt-tests`, and `tests` from the exact implementation commit.
  `qt-tests` passed 120/120; core tests passed 1183/1185 with only the two existing
  environment-dependent shader skips.
- The UI extraction suite passed. The targeted Triforce catalog retained all eight msgids, and
  `MAIN_TRIFORCE_IP_REDIRECTIONS` remained at the same read, save, and reset sites. The
  `TriforcePane` constructor creates no permanent widget or layout.
- Native macOS review passed in light and dark themes. Both groups and Configure buttons were
  aligned, the IP editor showed its three columns and default 12 populated rows plus the final
  empty row, and controller dispatch opened `Triforce Baseboard at Port 1`.
- Windows built the Release app and both test binaries from the exact implementation commit.
  `qt-tests` passed 120/120; core tests passed 1505/1507 with the same two shader skips.
- Native Windows review passed in light and dark themes at 980×760 and 700×520. The exact
  `[HEAD]` build title was present, both buttons remained visible without clipping, tab traversal
  moved from controller configuration to IP configuration, and controller dispatch opened the
  expected mapping window.
- The isolated IP editor audit loaded two custom rows, confirmed Clear followed by Cancel restored
  them, confirmed Default followed by Cancel retained the immediate default reset, and confirmed
  Clear followed by OK saved an empty table. The scheduled tasks and Dolphin processes were
  removed afterward, and the remote checkout remained clean at the exact implementation commit.
