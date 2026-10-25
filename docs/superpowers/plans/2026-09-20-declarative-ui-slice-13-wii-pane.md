# Declarative UI Slice 13: Wii Settings

**Status:** Complete

**Goal:** Move the Wii settings pane into a Qt Designer form while preserving SYSCONF choices,
SD-card paths and conversion actions, USB passthrough whitelisting, Wii Remote settings, and
emulation-state restrictions.

## Task 1: Define the complete form

- [x] Add `WiiPane.ui` with Misc Settings, SD Card Settings, Whitelisted USB Passthrough Devices,
  and Wii Remote Settings groups.
- [x] Author the static aspect-ratio, system-language, sound-mode, SD-size, and sensor-position
  choices in the form.
- [x] Use stock checkboxes, combo boxes, sliders, line edits, and buttons plus the existing
  minimum-size list wrapper, with buddies, non-default action buttons, and an explicit tab order.
- [x] Keep the complete pane usable at the settings pane's narrow width without horizontal
  clipping.

## Task 2: Bind settings and descriptions

- [x] Bind the four Misc checkboxes and the three mapped or indexed choices to their existing
  typed settings.
- [x] Bind both SD paths through `BindUserPath`, the three SD checkboxes, and the mapped SD-size
  choice.
- [x] Bind the Wii Remote rumble checkbox, mapped sensor position, IR sensitivity, and speaker
  volume.
- [x] Preserve all existing setting descriptions and label font mirroring.

## Task 3: Preserve SD-card and USB behavior

- [x] Preserve SD image and sync-folder browse starting locations, filters, native separators,
  effective user paths, and immediate config updates.
- [x] Preserve the pack and unpack confirmation text, asynchronous conversion, cancellation,
  progress dialog, and failure reporting.
- [x] Preserve USB whitelist population, display strings, selection-dependent Remove state,
  duplicate rejection, and Add and Remove writes.
- [x] Keep every browse, conversion, Add, and Remove button non-default.

## Task 4: Preserve runtime restrictions

- [x] Keep the same SYSCONF, WiiLink, SD conversion, and Wii Remote controls disabled while
  emulation is running.
- [x] Keep Insert SD Card, Allow Writes, folder sync, SD paths, SD size, USB whitelist, and USB
  keyboard behavior unchanged while emulation is running.
- [x] Preserve config-change refreshes and initial state setup.

## Task 5: Preserve compatibility

- [x] Keep the existing 70 translated msgids and 17 explicit config symbols.
- [x] Preserve all 18 concrete config locations plus the USB whitelist read and write paths.
- [x] Add a Qt form test for section order, authored choices, row placement, buddies, ranges,
  initial button state, tab order, and narrow layout fit.
- [x] Verify `WiiPane.cpp` constructs no permanent widget or layout.

## Task 6: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and targeted gettext/config comparisons.
- [x] Review Wii settings in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review wide and narrow layouts, scrolling, USB selection state, tab traversal, and setting
  dependencies on Windows.

## Definition of done

- [x] The Designer form owns the complete Wii pane layout and permanent controls.
- [x] Existing bindings, browsing, conversion, whitelist behavior, help, and runtime restrictions
  remain intact.
- [x] macOS and Windows builds, tests, audits, and visual review pass.

## Implementation checkpoint

- Exact implementation commit:
  `97bdcac88ffe8e6b457796485c960196213b8536` (`DolphinQt: define Wii settings in a form`).

## Verification

- macOS built `dolphin-emu`, `qt-tests`, and `tests` from the exact implementation commit.
  `qt-tests` passed 119/119; core tests passed 1183/1185 with only the two existing
  environment-dependent shader skips.
- The UI extraction suite passed. The targeted Wii catalog retained all 70 msgids, and the
  config audit retained 17 explicit symbols at 18 concrete locations plus the existing USB
  whitelist reads and writes. `WiiPane.cpp` creates no permanent widget or layout.
- Native macOS review passed in light and dark themes. All four groups, effective SD paths,
  conversion actions, compact USB list, mapped choices, slider values, and narrow scrolling were
  readable and aligned.
- Windows built the Release app and both test binaries from the exact implementation commit.
  `qt-tests` passed 119/119; core tests passed 1505/1507 with the same two shader skips.
- Native Windows review passed in light and dark themes at 980×760 and 700×520. The exact
  `[HEAD]` build title was present; defaults and all five mapped choices were verified; USB
  Remove changed from disabled to enabled after selecting a preloaded device; the complete tab
  order passed; and top-to-bottom scrolling showed no horizontal clipping.
- The Windows scheduled task and Dolphin process were removed after review, and the remote
  checkout remained clean at the exact implementation commit.
