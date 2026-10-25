# Declarative UI Slice 4: Settings Shell

**Status:** Complete

**Goal:** Move the shared settings-window shell into a Qt Designer form and add compact category
icons plus persistent setting help while preserving pane order, navigation, theme handling,
window sizing, and the game-properties use of the same shell.

## Task 1: Migrate the existing shell

- [x] Add `SettingsWindow.ui` for the navigation list, pane stack, help area, and footer.
- [x] Convert `StackedSettingsWindow` into the form's controller.
- [x] Preserve settings and game-properties navigation, close behavior, theme updates, and sizing.
- [x] Add a Qt form test and verify the behavior-preserving migration.

## Task 2: Add settings-shell parity

- [x] Add category icons from Dolphin-owned resources and native Qt standard icons.
- [x] Show category guidance in a persistent help pane.
- [x] Show control descriptions on pointer hover and keyboard focus.
- [x] Keep existing balloon tooltips available while exposing the same metadata to future
  binder-authored panes.

## Task 3: Preserve compatibility

- [x] Keep the existing pane order and `SettingsWindowPaneIndex` values.
- [x] Keep `PropertiesDialog` on the shared shell without global-settings category guidance.
- [x] Record the `.pot` msgid diff and retain all moved shell strings.
- [x] Keep every existing settings binding and pane implementation unchanged.

## Task 4: Verify the slice

- [x] Build `dolphin-emu`, `qt-tests`, and `tests` on macOS.
- [x] Review settings and game-properties shells in light and dark themes on macOS.
- [x] Build and run both test binaries on Windows.
- [x] Review icons, help updates, focus, keyboard traversal, and narrow-window behavior on Windows.

## Verification notes

- macOS built `dolphin-emu`, `qt-tests`, and `tests`; the final crash guard was followed by a
  `dolphin-emu` and `qt-tests` rebuild. `qt-tests` passed all 102 tests, and `tests` passed 1183
  tests with the same two environment-dependent shader tests skipped.
- Windows built `dolphin-emu`, `qt-tests`, and `tests` from exact commit `460bcee1021`. Native
  `qt-tests -platform windows` passed all 102 tests, and `tests` passed 1505 tests with the same two
  environment-dependent shader tests skipped.
- The UI extraction regression suite passed. Compared with the form-migration checkpoint, the
  gettext catalog has 13 added msgids and no removed msgids: two accessibility names and eleven
  category-help strings.
- Global Settings and game properties were reviewed in Light and Dark themes on macOS. Settings
  showed the category icons and persistent General guidance. Game properties retained the shared
  navigation shell and Close footer without global-settings icons or guidance.
- Windows Light and Dark themes were reviewed at 980 by 760 and 700 by 650 pixels. All eleven
  categories, the help pane, and Close remained visible. Selecting Graphics and Interface updated
  category help; hovering Dual Core and Theme showed persistent control help while the legacy
  balloon tooltip remained available. Focus moved to Dual Core, and Tab advanced to Enable Cheats.
- The first Windows visual run found a deterministic startup crash when Windows delivered a style
  change event while `SettingsWindow.ui` was still being constructed. Dump analysis located the
  dereference in `UpdateNavigationListStyle`; guarding the not-yet-created navigation list fixed
  it. The final audit opened Settings through the normal Config toolbar button in both themes and
  completed without the temporary audit hook.
- Follow-up shell polish added 12-pixel horizontal gutters around the category cards while keeping
  the sidebar background flush with the window edge. The form assertion, UI extraction check,
  macOS visual review, and all 103 Qt tests passed; Windows rebuilt the app and both test binaries
  at exact commit `3ee5981fb990ff70453b0a121ddcaa350b451bfd`, passing all 103 Qt tests and
  1,505 of 1,507 core tests with the same two environment-dependent skips.

## Definition of done

- [x] `SettingsWindow.ui` owns the shared settings-window layout.
- [x] Settings categories have clear Dolphin-owned or native icons.
- [x] Category and control help remain visible without replacing existing balloon tooltips.
- [x] Global settings and game properties retain their existing pane behavior.
- [x] macOS and Windows builds, tests, gettext checks, and visual review pass.
