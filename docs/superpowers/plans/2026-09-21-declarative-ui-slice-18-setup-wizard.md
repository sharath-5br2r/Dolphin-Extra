# Declarative UI Slice 18: Setup Wizard

**Status:** Complete

**Goal:** Give genuinely new Dolphin profiles a native first-run setup flow, authored in a `.ui`
form and adapted from the PCSX2 setup sequence without copying PCSX2 source or assets.

The approved design called this slice 17. The branch used slice 17 for the intervening settings
layout polish requested during implementation, so this plan continues with slice 18.

## Task 1: Define the first-run contract

- [x] Detect whether `Dolphin.ini` existed before configuration initialization.
- [x] Store an incomplete marker before showing the wizard so canceling or an interrupted launch
  presents it again.
- [x] Never present the wizard to an established profile solely because the marker is absent.
- [x] Skip modal setup in batch mode while retaining the incomplete marker for the next GUI launch.
- [x] Cover the decision table with a pure policy test.

## Task 2: Author the setup form

- [x] Add a native dialog form with Appearance, Game Folders, Controllers, Privacy, and Complete
  pages.
- [x] Keep page navigation, margins, focus order, accessible names, and minimum sizing usable at a
  compact desktop window size.
- [x] Use Dolphin's own logo, strings, settings, and implementation.
- [x] Add a form test for page order, controls, layout ownership, and keyboard traversal.

## Task 3: Connect Dolphin settings

- [x] Share the existing Interface language choices rather than duplicating them.
- [x] Save language and built-in style choices, applying style changes to the wizard immediately.
- [x] Add and remove game folders and bind recursive scanning.
- [x] Record the usage-statistics choice and suppress the separate analytics prompt after setup.
- [x] Offer to open Controller Settings after the main window initializes.
- [x] Save configuration and clear the incomplete marker only when setup finishes.

## Task 4: Integrate startup

- [x] Run setup before constructing the main window so it cannot flash behind the wizard.
- [x] Preserve command-line boot, updater, analytics, shutdown, and established-profile behavior.
- [x] Exit cleanly when setup is canceled.

## Task 5: Verify and clean up

- [x] Build DolphinQt and both test binaries on macOS and run both test suites.
- [x] Run the UI extraction regression test.
- [x] Review every wizard page in native light and dark styles on macOS.
- [x] Build and test on Windows and review the wizard in native light and dark styles.
- [x] Exercise new-profile completion, cancel-and-resume, established-profile, and batch-mode paths.
- [x] Remove temporary users, scripts, captures, and validation artifacts from both hosts.

## Definition of done

- [x] A new GUI profile sees the setup wizard before the main window.
- [x] Existing profiles and batch launches are not interrupted.
- [x] Canceling causes setup to return on the next GUI launch.
- [x] Appearance, paths, analytics, and controller follow-up choices take effect.
- [x] macOS and Windows builds, tests, extraction, and visual review pass.

## Verification

- Implementation: `945906460b DolphinQt: add first-run setup wizard`
- macOS: DolphinQt and both test targets built; 136 Qt tests passed; 1,185 core tests passed
  with 2 preset-dependent skips.
- Windows: DolphinQt and Qt tests built; 136 Qt tests passed; 1,507 core tests passed with
  2 preset-dependent skips.
- UI extraction, `git diff --check`, form validation, and clang-format checks passed.
- Native review covered all five pages in light and dark styles on macOS and Windows.
- Native workflow review covered first-run completion, controller-settings handoff,
  cancel-and-resume, and established-profile startup. The policy suite covered batch startup.
- Temporary profiles, scripts, captures, scheduled tasks, archives, and transferred source changes
  were removed from both validation hosts.
