# Declarative UI Slice 17: Settings Layout Polish

**Status:** Complete

**Goal:** Make the shared settings shell and its widest pages fit predictably, keep the padded help
panel aligned with the active page, and use a prominent horizontal volume control.

## Task 1: Rework the shared settings shell

- [x] Keep the navigation and active page in the top row.
- [x] Keep the navigation sidebar full height and place the help panel below the active page.
- [x] Give the help panel balanced left and right gutters, with the footer below it.
- [x] Update the shell form test for the new grid structure, spans, and margins.

## Task 2: Fit controller settings at the default width

- [x] Open the global settings window at a screen-bounded preferred width that accommodates the
  controller rows.
- [x] Prevent dynamic controller and adapter names from increasing combo-box minimum widths.
- [x] Extend controller form tests to cover compact combo sizing and narrow row fit.

## Task 3: Make volume consistent with other sliders

- [x] Place the Volume group at the top of the main Audio column.
- [x] Change the volume slider to horizontal and keep its value label aligned beside it.
- [x] Update the Audio form test for the revised structure, focus order, and narrow layout.

## Task 4: Verify and clean up

- [x] Build the app and both test binaries on macOS and run both test suites.
- [x] Run the UI extraction regression test.
- [x] Review Controllers, Audio, and the content-column help panel in light and dark themes.
- [x] Build and run the app and both test binaries on Windows and review the revised layouts.
- [x] Remove validation scratch and leave both checkouts clean.

## Definition of done

- [x] Controllers are fully visible at the default settings width.
- [x] Volume is the first Audio control and uses the same horizontal interaction pattern as other
  sliders.
- [x] The help panel remains aligned with the active page and has balanced edge padding.
- [x] macOS and Windows builds, tests, and visual review pass.

## Verification

- Initial implementation: `96005327810f627f371fd9867f20f6c55cead8fc`
- Follow-up implementation: `21ba25ee0e596a62b5709e5347890b14becc8739`
- macOS: DolphinQt and both test targets built; 130 Qt tests passed; 1,185 core tests
  passed and 2 preset-dependent tests skipped; UI extraction passed.
- Windows: DolphinQt and both test targets built; 130 Qt tests passed; 1,507 core tests
  passed and 2 preset-dependent tests skipped.
- Follow-up validation rebuilt DolphinQt on macOS and Windows, passed all 130 Qt tests on each
  platform, and passed UI extraction.
- Native macOS and Windows renders confirmed Volume is first, the padded help panel stays in the
  content column, and the navigation sidebar remains full height. Windows light and dark renders
  both passed.
- Temporary users, bundles, scripts, captures, and the Windows-only render hook were removed.
