# Declarative UI Slice 15: Advanced Settings

**Status:** Complete

**Goal:** Move the Advanced settings pane into a Qt Designer form while preserving its dynamic
CPU-core choices, config binding, runtime value labels, emulation-state restrictions, and reset
workflow.

## Task 1: Define the complete pane form

- [x] Add `AdvancedPane.ui` with CPU Options, Timing, Clock Override, VBI Frequency Override,
  Memory Override, Custom RTC Options, and Reset Dolphin Settings groups plus the trailing stretch.
- [x] Author the CPU-engine label and combo, all checkboxes, both float sliders, both scaled memory
  sliders, their value labels, the UTC date-time editor, and the reset button as stock widgets.
- [x] Preserve the group and control order, slider ranges, non-default reset button behavior,
  explicit tab order, and narrow-pane fit.

## Task 2: Bind the declarative controls

- [x] Populate the platform-dependent CPU engine choices from `PowerPC::AvailableCPUCores()` and
  bind their enum values with the runtime mapped-combo overload.
- [x] Bind the boolean settings, 1–500% clock sliders, and 24–64 MiB and 64–128 MiB memory sliders
  with the stock-widget config binder.
- [x] Preserve the existing descriptions and translator context for the timing controls.

## Task 3: Preserve dynamic behavior

- [x] Keep the CPU and VBI percentage labels synchronized with their sliders and current emulated
  clock or refresh rate.
- [x] Keep CPU-engine, MMU, and panic controls locked while emulation runs; preserve the existing
  write-back-cache behavior.
- [x] Enable clock and VBI sliders only with their respective overrides, and enable memory sliders,
  custom RTC editing, and reset only when their existing state rules allow it.
- [x] Preserve the custom RTC display format, four-digit year, UTC interpretation, 2000–2099 range,
  and epoch-seconds storage.

## Task 4: Preserve reset and compatibility behavior

- [x] Keep the reset confirmation text, Yes/No buttons, default No choice, window modality, settings
  reset, user-directory refresh, config-change notification, and optional analytics prompt.
- [x] Retain all 38 translated msgids and the existing 16 config keys.
- [x] Add a Qt form test for group order, control placement, slider ranges, label buddies, button
  defaults, tab order, and narrow layout fit.
- [x] Verify `AdvancedPane.cpp` constructs no permanent widget or layout.

## Task 5: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config comparisons.
- [x] Review the full pane, dependent enabled states, and reset confirmation in light and dark
  themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, scrolling, tab traversal, dependent enabled states, RTC
  editing, and reset cancellation on Windows.

## Definition of done

- [x] The Designer form owns the complete Advanced settings pane layout and permanent controls.
- [x] Dynamic labels, config writes, state restrictions, RTC handling, and reset behavior match the
  prior pane.
- [x] macOS and Windows builds, tests, audits, and native review pass.

## Implementation checkpoint

- Exact implementation commit:
  `19f8701decdf6af6cbad7ab68e554d31b487574c`
  (`DolphinQt: define Advanced settings in a form`).

## Verification

- macOS built `dolphin-emu`, `qt-tests`, and `tests` from the exact implementation commit.
  `qt-tests` passed 121/121; core tests passed 1183/1185 with only the two existing
  environment-dependent shader skips.
- The UI extraction suite passed. The targeted Advanced catalog retained all 38 msgids and their
  translator comments, the same 16 config keys remained in use, and `AdvancedPane.cpp` constructs
  no permanent widget or layout.
- Native macOS review passed in light and dark themes. The platform-dependent ARM64 CPU engine fit,
  dependent controls enabled only with their overrides, the clock and memory labels tracked test
  values, and the reset confirmation focused No by default and canceled without changing values.
  The global Qt configuration and Advanced settings were restored afterward.
- Windows built the Release app and both test binaries from the exact implementation commit.
  `qt-tests` passed 121/121; core tests passed 1505/1507 with the same two shader skips.
- Native Windows review passed in light and dark themes at 980×760 and 700×520. The exact `[HEAD]`
  build title was present, initial disabled states and override dependencies matched, all authored
  tab transitions from the CPU engine through Custom RTC passed, and the value labels showed
  111% CPU, 111% VBI, 35 MiB MEM1, and 75 MiB MEM2.
- The Windows review edited the Custom RTC to a valid 2032 value, confirmed reset cancellation
  preserved it and the other test values, and visually confirmed that the narrow pane scrolls far
  enough to expose the complete Reset Dolphin Settings group. The scheduled task and Dolphin
  processes were removed afterward, and the remote checkout remained clean at the exact
  implementation commit.
