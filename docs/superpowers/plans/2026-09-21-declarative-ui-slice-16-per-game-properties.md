# Declarative UI Slice 16: Per-Game Properties

**Status:** Complete

**Goal:** Move the per-game Game Config page and INI editor into Qt Designer forms, keep the
existing local and shipped game-INI behavior, and make inherited values explicit in layered
controls.

## Task 1: Record the current contract

- [x] Identify the ten bespoke General-tab config keys and the existing local-over-system-over-base
  display priority.
- [x] Confirm that the Graphics tab already reuses the four declarative graphics panes with a
  per-game `Config::Layer`.
- [x] Confirm the existing editor refresh, external-editor, completion, highlighting, save, and
  tab-switch behavior.

## Task 2: Define the per-game forms

- [x] Add `GameConfigWidget.ui` with the warning row, General groups, Graphics host, Editor groups,
  tab order, slider ranges, label buddies, and narrow-window layout.
- [x] Add `GameConfigEdit.ui` with the refresh and external-editor buttons plus the plain-text
  editor.
- [x] Keep dynamically discovered INI tabs and the existing graphics pane as runtime children of
  layouts authored in the forms.

## Task 3: Migrate and preserve behavior

- [x] Bind the General controls through `ConfigWidgetBinder`, including reverse disc-speed
  semantics, deterministic-dual-core values, float mappings, descriptions, value labels, and font
  mirrors.
- [x] Preserve shipped game settings as the inherited value and italic marker, local overrides as
  bold, and right-click clearing.
- [x] Preserve editor synchronization when entering or leaving the raw INI tab, active-game layer
  reload, and empty-local-INI cleanup.
- [x] Add form tests for structure, authored properties, focus order, and narrow layout fit.

## Task 4: Expose inherited values

- [x] Add layered binding support for an optional inherited game layer before falling back to the
  base setting.
- [x] Show an inherited checkbox as partially checked and a layered combo box as
  `Use Global Setting [X]`; selecting an explicit value creates an override.
- [x] Show inherited slider and spin-box values in place, create an override on editing, and keep
  right-click clearing.
- [x] Cover inherited-state display, edits, clearing, reverse booleans, mapped choices, and
  no-write refresh behavior in binder tests.

## Task 5: Verify the slice

- [x] Retain the existing Game Config and editor translated msgids, apart from the intentional
  inherited-setting strings.
- [x] Retain the same bespoke General-tab config keys and preserve the Graphics binding registry.
- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS and run both test binaries.
- [x] Review the page and inherited/overridden states in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows, then review wide, narrow, light, and dark states.
- [x] Remove local and Windows validation scratch while leaving both source checkouts clean.

## Definition of done

- [x] The forms own every permanent Game Config and INI editor widget and layout.
- [x] Per-game controls clearly distinguish inherited, shipped game, and user values.
- [x] Existing per-game settings, graphics behavior, raw editing, and cleanup workflows remain
  intact.
- [x] macOS and Windows builds, tests, audits, and native review pass.

## Implementation checkpoints

- Form migration:
  `980315577682adff74b0d6f510be5ee1c8639c37`
  (`DolphinQt: define per-game config in forms`).
- Inherited-value parity:
  `c36c63fbf0908089e129a6f65ff49f85cc90ca1e`
  (`DolphinQt: expose inherited per-game settings`).

## Verification

- macOS built `dolphin-emu`, `qt-tests`, and `tests` from the parity commit. All 130 Qt tests
  passed. Core tests passed 1185/1187 with only the two existing environment-dependent shader
  preset skips.
- The UI extraction regression suite passed. A targeted comparison retained 46 Game Config and
  editor msgids, with only the planned inherited-setting help rewrite. The same ten bespoke
  General-tab config symbols remained in use.
- Native macOS review passed in the built-in light and dark themes. Inherited checkboxes displayed
  their partial state, combo boxes identified the effective global value, and the shipped
  `StereoConvergence = 64` value appeared in italics. Creating and right-click-clearing a local
  checkbox override updated immediately. Graphics dependencies and the syntax-colored default and
  user INI editors remained correct. The saved theme and temporary game path were restored
  afterward.
- Windows built the Release app and both test binaries from the exact parity commit. All 130 Qt
  tests passed. Core tests passed 1507/1509 with the same two shader preset skips.
- Native Windows Qt renders covered General, Graphics General, Graphics Enhancements, and Editor in
  light and dark themes at 980 and 700 pixels wide. Inherited states, bracketed global values,
  shipped convergence, disabled backend-dependent controls, syntax colors, and buttons remained
  readable without horizontal clipping. The Properties shell retained its existing 1096-pixel
  natural height while exercising the narrow width.
- A temporary uncommitted capture hook rendered the real Windows Properties dialog with the
  `qwindows` platform plugin. It was removed from both source trees, and the normal macOS and
  Windows apps were rebuilt from the parity commit before cleanup.
