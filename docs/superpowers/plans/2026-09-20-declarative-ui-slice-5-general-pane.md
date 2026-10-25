# Declarative UI Slice 5: General Settings

**Status:** Complete

**Goal:** Move the General settings pane into a Qt Designer form, replace its plain config-widget
subclasses with bind-after-construction controls, and use a compact checkbox grid while preserving
every setting, runtime restriction, side effect, description, and translated string.

## Task 1: Migrate the existing pane

- [x] Add `GeneralPane.ui` for the four settings groups and their controls.
- [x] Convert `GeneralPane` into the form's controller and remove its C++ layout construction.
- [x] Keep unsupported auto-update, Discord, and analytics controls absent from the visible layout.
- [x] Preserve control order, label buddies, keyboard traversal, and scroll sizing.
- [x] Add a Qt form test for the pane's permanent structure.

## Task 2: Move plain config controls to the binder

- [x] Bind Dual Core, Cheats, Load Whole Game Into Memory, Mismatched Region Settings, and
  Automatic Disc Changes through `ConfigWidget::Bind`.
- [x] Move their help text to `ConfigWidget::SetDescription` so balloon and persistent help use
  the same metadata.
- [x] Keep speed-limit rounding in the controller because arbitrary existing float values are
  rounded to the nearest displayed tenth.
- [x] Keep update-track, fallback-region, analytics, and Discord handling in the controller because
  they use `Settings` storage, enum/string mappings, or runtime side effects.
- [x] Record the binding-coverage and `.pot` msgid diffs.

## Task 3: Add General-pane layout parity

- [x] Arrange the Basic Settings checkboxes in a compact two-column grid, following the useful
  grouping pattern in PCSX2's Designer-authored emulation pane without copying its source or text.
- [x] Keep Speed Limit as a labeled full-width row below the checkbox grid.
- [x] Preserve the logical tab order independently of visual row placement.
- [x] Verify the grid collapses cleanly inside the existing scroll wrapper at narrow widths.

## Task 4: Preserve compatibility

- [x] Keep the existing config keys and default values.
- [x] Keep the `EnableCheatsChanged`, update-track, fallback-region, analytics, Discord, and
  identity-generation behavior.
- [x] Keep all emulation-state restrictions and Hardcore Mode speed-limit guidance.
- [x] Keep the current group titles, control labels, combo choices, and descriptions.

## Task 5: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite and compare the source catalog.
- [x] Review General settings in light and dark themes at wide and narrow sizes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review layout, help, focus order, disabled-while-running behavior, and narrow sizing on
  Windows.

## Verification notes

- macOS built `dolphin-emu`, `qt-tests`, and `tests`. All 103 Qt tests passed. The core suite ran
  1,185 tests, with 1,183 passing and the two environment-dependent shader preset tests skipped.
- Windows checked out exact commit `adee06925df3f31f4ca389824cb5cdc6bb56624b`, built all three
  targets, passed all 103 Qt tests, and ran 1,507 core tests with 1,505 passing and the same two
  environment-dependent tests skipped.
- The UI extraction regression suite passed. A targeted gettext comparison between the former
  C++ pane and the new C++ plus generated form found 41 msgids before and after, with no additions
  or removals.
- Light and dark theme reviews on macOS confirmed the final two-column layout. The 560-pixel form
  test verified that permanent controls neither overlap nor clip at a narrow width.
- Native Windows reviews at 980 by 760 and 700 by 650 confirmed the light and dark layouts,
  persistent help text, and keyboard traversal from Dual Core to Cheats. Direct review of the
  controller path confirmed that the existing emulation-state restrictions and custom side
  effects remain connected.

## Definition of done

- [x] `GeneralPane.ui` owns all General-pane layout and permanent controls.
- [x] Every former `ConfigBool` construction site uses the stock-widget binder.
- [x] The Basic Settings grid is compact, readable, and keyboard accessible.
- [x] Existing config semantics, side effects, help, and translations are retained.
- [x] macOS and Windows builds, tests, gettext checks, and visual review pass.
