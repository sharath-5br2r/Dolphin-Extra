# Big Picture slice 1 — State, commands and host bridge

**Date:** 2026-10-24

**Status:** Complete; Windows build and automated verification passed

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 1

## Implementation

- `Common/BigPicture/Contracts.h` defines value-only commands, results, modes, screens,
  lifecycle state, availability reasons and snapshots. These contracts have no Qt or GPU
  dependencies. Commands carry a nonzero monotonically increasing request ID and the generation
  of the snapshot they were created from. Results retain the original command for correlation.
- `UICommon/BigPicture/Controller` owns the screen stack and command state on the host thread.
  Published `shared_ptr<const Snapshot>` values remain valid across updates. Back pops one screen
  before leaving the menu. Stop and reset use modal confirmations with Cancel as their default;
  cancelling never invokes the emulator operation. Confirm must identify the displayed request,
  so an old dialog cannot confirm a newer action. Errors retain their message until dismissed
  with Back, even if later commands are rejected.
- Session boundaries invalidate queued commands and confirmations. Boot acceptance invalidates
  the previous generation immediately, including on immediate failure. External desktop boots
  explicitly invalidate the session. Observed boot/stop transitions invalidate snapshots too.
  Graceful stop stays pending until actual stop completion. Boot remains pending until the
  core runs, pauses, fails or is stopped externally.
- `DolphinQt/BigPictureHost` queues value commands onto Qt's owning thread and publishes snapshots
  behind a short mutex. Reading a snapshot never queries live core or ImGui state. Host destruction
  discards queued callbacks. No continuous timer, input poll or presentation work is added.
- `MainWindow` supplies live lifecycle/policy state and adapts its existing boot (including
  second-disc scanning), resume, stop, reset and ten-slot save/load operations. Availability is
  refreshed before dispatch and rechecked by the host. Load restrictions match the existing
  NetPlay/hardcore policy; reset/resume cannot alter a NetPlay session locally. Booting, stopping
  and an active desktop stop confirmation block conflicting operations.
- Desktop `RequestStop()` keeps its existing confirmation behavior. The menu's confirmed stop
  uses the same shutdown path without repeating the ordinary stop question. Mandatory
  RAIntegration modification warnings, graceful Wii power events, recording cleanup and the
  shutdown timeout still use the existing path.

Save/load APIs currently return `void` and queue work internally. Their result is deliberately
`Dispatched`, not `Succeeded`; accurate asynchronous state I/O completion is slice 10. Existing
boot warnings/errors and mandatory achievement warnings still use Qt dialogs until the in-game
presentation work. Opening/closing this logical menu does not itself change guest pause state;
pause ownership, rendering and input capture remain slice 2. There is no user-visible Big Picture
entry point in this slice.

## Validation

- Release builds of `dolphin-emu`, `tests` and `qt-tests` passed with MSVC 19.51 and the pinned
  Qt 6.8.3 dependencies. The optional unrelated emusync integration was disabled for this build.
- CTest from `build/big-picture-host/out/Source/UnitTests` passed: **1,559 tests passed**, including
  **19 new controller tests**. The same two environment-dependent real-preset tests were skipped
  (`SlangCompile.RealPresetCompilesAllPasses` and `LibrashaderParameters.EnumerateRealPresetFromEnv`).
- All **195 Qt tests passed**, including **three new bridge tests** covering worker-thread command
  submission, immutable snapshots, execution-time policy/session checks, and dropping queued
  callbacks when the host is destroyed.
- The Windows Qt bundle has no offscreen plugin. The Qt run uses `-platform windows`, the build's
  `Binaries` directory on `PATH`, `QT_PLUGIN_PATH=Binaries/QtPlugins`, and
  `QT_QPA_PLATFORM_PLUGIN_PATH=Binaries/QtPlugins/platforms` (absolute paths). The initial attempt
  pointing the platform-plugin variable at the parent directory failed before test output;
  correcting the harness paths resolved it without a product change.
- `clang-format` 22.1.3 dry-run and `git diff --check` passed. No form, translation source message
  or translator comment changed; the adapter reuses MainWindow's existing boot-error message.
- Fake-host tests cover nested Back, opening on an already-paused game, explicit Resume, stale
  and duplicate commands, confirmation cancellation/identity, reentrant execution, invalid slots,
  policy changes, immediate/asynchronous boot failure, external stop during boot, and graceful
  stop completion. Opening/closing the logical menu leaves the guest state alone.

Validation used temporary links to the previously populated pinned dependencies, removed after
the build/tests. Logs and binaries remain under ignored `build/big-picture-host/`. The bundled
Qt deployment tool warns about absent Qt translation catalogs; this is unrelated to the new
bridge. Linux/macOS builds and controller/graphics runtime checks remain later platform gates.

## Next slice

Slice 2 adds paced presentation on the existing graphics owner, full graphics-lock exclusion,
render/input queues, capture snapshots and guest/hotkey gating. It must preserve the session and
command boundary introduced here and the ownership rules established by slice 0.
