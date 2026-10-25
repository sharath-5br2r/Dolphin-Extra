# Declarative UI Slice 6: Graphics Settings

**Status:** Complete

**Goal:** Move the Graphics settings tab shell and its four permanent pages into Qt Designer
forms, replace their config-widget subclasses with stock widgets and bind-after-construction
controls, and preserve backend-dependent availability, per-game overrides, descriptions, and
translated strings.

The shader preset picker and shader-parameter editor remain dynamic standalone dialogs. Their
container migrations belong with the later standalone-dialog slices; the controls generated from
shader metadata must remain in C++.

## Task 1: Migrate the tab shell

- [x] Add `GraphicsPane.ui` for the General, Enhancements, Hacks, and Advanced tab structure.
- [x] Keep construction of the four page controllers and their scroll wrappers in C++.
- [x] Preserve backend propagation and the shared per-game config layer.
- [x] Add a Qt form test for the permanent tab structure.

## Task 2: Migrate General graphics settings

- [x] Add `GeneralWidget.ui` for Basic, Other, and Shader Compilation.
- [x] Bind backend, aspect ratio, custom dimensions, checkboxes, and shader modes through
  `ConfigWidget`.
- [x] Preserve adapter population, backend warnings, custom-aspect visibility, runtime
  restrictions, and backend-change signals.
- [x] Move every permanent description to `ConfigWidget::SetDescription`.

Checkpoint: the tab shell and General page build in `dolphin-emu` and `qt-tests`; all 105 Qt tests
pass on macOS, including the two new form-structure tests.

## Task 3: Migrate Enhancements

- [x] Add `EnhancementsWidget.ui` for enhancement, post-processing, and stereoscopy controls.
- [x] Replace simple, mapped, complex, text, and float-slider config subclasses with binder
  equivalents.
- [x] Preserve backend capability gating, shader preset browsing and parameters, anti-aliasing
  population, stereoscopy behavior, value labels, and inter-control dependencies.

## Task 4: Migrate Hacks

- [x] Add `HacksWidget.ui` for EFB, texture-cache, XFB, and other controls.
- [x] Replace every config subclass with stock widgets and binder calls, including reversed
  boolean bindings and the mapped accuracy slider.
- [x] Preserve backend capability descriptions and all dependent enabled states.

## Task 5: Migrate Advanced

- [x] Add `AdvancedWidget.ui` for debugging, utility, texture dumping, frame dumping, crop, misc,
  and experimental controls.
- [x] Replace every config subclass with stock widgets and binder calls.
- [x] Preserve global-only controls, runtime restrictions, backend-specific availability,
  dependent enabled states, and label font mirroring.

## Task 6: Preserve compatibility

- [x] Record an exact before/after binding-key comparison for all four pages.
- [x] Keep every existing config key, reversed value, range, step, and default.
- [x] Keep global-versus-per-game layer choices unchanged.
- [x] Keep tab order, labels, group titles, descriptions, and all emitted signals.
- [x] Record the targeted gettext msgid comparison.

## Task 7: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Run the UI extraction regression suite.
- [x] Review every tab in light and dark themes on macOS, with narrow geometry covered by the
  native Windows pass.
- [x] Build and run both test binaries on Windows.
- [x] Review every tab, persistent help, focus traversal, disabled controls, and narrow sizing on
  Windows.

## Definition of done

- [x] Five Designer forms own the Graphics pane's permanent tab and page layouts.
- [x] No Graphics page constructs a permanent control or layout in C++.
- [x] Every former Graphics-page config subclass uses the binder.
- [x] Existing backend, emulation-state, per-game override, shader, and dependency behavior remains
  intact.
- [x] macOS and Windows builds, tests, gettext checks, binding audit, and visual review pass.

## Verification notes

- macOS builds `dolphin-emu`, `qt-tests`, and `tests`. All 109 Qt tests pass. The core suite runs
  1,185 tests, with 1,183 passing and the two environment-dependent shader tests skipped.
- The UI extraction regression suite passes. The targeted Graphics catalog contains 244 msgids
  before and after the migration, with no additions or removals; the Advanced vertex-shader
  translator comment is preserved in the generated form header.
- The source binding audit retains the exact unique config-key sets: General 12, Enhancements 24,
  Hacks 16, and Advanced 30, with no additions or removals. Reversed bindings, slider ranges and
  steps, and global-only dump and timing controls remain explicit in the binder calls and form
  tests.
- The exact current macOS bundle was reviewed at 768 pixels wide in both light and dark themes.
  All four tabs fit cleanly, permanent help remains visible, and backend-dependent controls show
  the expected disabled state.
- Windows Release builds and runs `dolphin-emu`, `qt-tests`, and `tests` from the exact Graphics
  code commit `a2f64f4adb2757c550cb1a36d6b0d76ceb4cac84`. All 109 Qt tests pass. The core
  suite runs 1,507 tests, with 1,505 passing and the same two environment-dependent shader tests
  skipped.
- Native Windows captures cover all four tabs in light and dark themes at 980x760 and 700x650.
  The pass verifies persistent help, disabled states, and successive keyboard-focus targets. It
  exposed horizontal scrolling on the narrow Enhancements page; moving the post-processing
  actions to their own form-owned row removed that overflow, and the corrected captures retain
  every label and control without horizontal scrolling.
