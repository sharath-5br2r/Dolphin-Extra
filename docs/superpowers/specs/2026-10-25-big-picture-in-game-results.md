# Big Picture slice 4 — In-game menu and ten-slot browser

**Date:** 2026-10-25

**Status:** Complete on Windows/D3D11; platform and physical-device checks remain release gates

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 4

## Available actions

The production menu now provides Resume, Save State, Load State, Settings, Change Disc
(Desktop), Reset, Screenshot, Stop Emulation, Exit Dolphin, and Close Menu. It shows the
running title over a dimmed retained game frame. Existing native desktop behavior remains
the fallback when stopping; the standalone Big Picture library is slice 5.

Resume explicitly resumes an already-paused game when policy permits. Close Menu and nested
Back keep the existing pause-ownership rules: a menu can only undo a pause it still owns.
Loading a state invalidates that ownership, so closing afterward leaves the game paused;
Resume remains available. NetPlay's local menu does not acquire a pause.

Reset, Stop and Exit show in-menu confirmations that initially focus Cancel. Occupied save
slots also require an overwrite confirmation. A confirmation carries its original command ID
through the render/input/Qt queues; an old activation cannot confirm a newer request. Dialog
focus revisions reset to Cancel without accumulating one focus-memory entry per command.

Exit uses the existing stop path and waits for graceful shutdown. Stop completion returns the
Qt host's logical mode to Desktop, preventing the next native game launch from unexpectedly
opening and pausing in Big Picture. Achievement modification workflows that still require a
desktop review disable Stop/Exit with an explicit reason instead of bypassing that review.

Menu Preferences remain available in the in-game Settings page and Config → Big Picture.
Advanced Settings and disc selection have explicit desktop handoffs: leave fullscreen, show
the desktop interface, release menu capture, and preserve a local game's pause. Disc selection
uses the existing picker and DVD operation; generation, core state and NetPlay policy are
checked again after the dialog. The controller-only file picker belongs to slice 6 and the
common settings service/pages to slices 7–8.

Screenshot uses Dolphin's existing capture request. It finishes on the next game frame;
requesting it from a paused menu does not advance the guest solely to take a screenshot.

## State slots and result reporting

Save/Load use a simple ten-slot list with one-based slot numbers, saved dates, Empty and
unreadable-header indications, Refresh Slots, and Back. Qt reads metadata on entry or explicit
refresh and publishes value-only snapshots. Rendering never reads state files or core objects.
The metadata read uses the existing state-save lock and rejects non-finite/out-of-range dates
before converting them to a platform timestamp. It does not validate state payloads or versions.

Empty/unreadable loads are disabled. Execution checks the selected slot again; a newly occupied
save requires a refresh and confirmation, while a missing/unreadable load becomes an in-menu
error. Policies are advisory in snapshots and checked again at dispatch. NetPlay/hardcore load
restrictions, disc availability and busy lifecycle states appear as disabled reasons.

State I/O still uses the existing void core APIs. The menu honestly reports that a request was
sent, rather than claiming completion; Dolphin's normal messages report subsequent results.
Asynchronous metadata, complete save/load completion/error routing, corrupt/incompatible
payload handling and optional previews remain slice 10. Metadata refresh can wait for a pending
save to finish; no background state-metadata worker is introduced in this slice.

## Validation

Windows Release Dolphin and both test binaries build with MSVC 19.51 and pinned Qt 6.8.3.
Optional emusync remains disabled, as in preceding slices.

- **1,610 core tests passed**, with the same two environment-dependent shader-preset tests skipped.
- **211 Qt tests passed**. This slice adds eight core and four Qt tests.
- Tests cover root/slot action routing, all ten slot numbers, disabled loads and policy changes,
  Cancel-first production dialogs, overwrite/exit confirmations, graceful exit acknowledgment,
  queued slot/confirmation payloads, pause ownership after loading, and stop followed by boot.
- The real Qt application passed a real-game menu → save → load → close → explicit Resume flow
  using named gamepad events, including overwrite/reset cancellation and confirmed stop.
  Single-core, normal dual-core and deterministic dual-core configurations were exercised.
- A separate D3D11 renderer probe captured all seven production page types at 720p, 1080p and
  4K with zero alerts. Native captures of the root, slot list and confirmation dialogs were
  inspected, including Japanese/Cyrillic text and independent menu sizing.
- Formatting, `git diff --check`, documentation links and gettext source-string extraction passed.

The application probe uses an isolated muted profile and an existing local game; it does not
modify the game or the user's normal profile. The automated desktop cannot reliably supply
Windows foreground focus, so this probe injects named gamepad navigation events after the
existing binding layer and opens the menu through the queued host API. It verifies production
renderer → request queue → scheduler → Qt host → core behavior, not physical controller input or
native focus handling. The retained slice 3 tests still cover bindings and controller input.

Excluded probes, logs, captures and test profiles live under ignored `build/big-picture-host/`.
The normal application was rebuilt without the temporary CMake hook; dependency links were
removed after validation.

## Trying the slice

1. In Config → Big Picture, configure a controller's Open Menu and navigation bindings. Shared
   Menu Preferences are directly in the general Big Picture category.
2. Start a game, open the menu, select Save State and an empty slot. Return to the root, open
   Load State and select that slot. Use Refresh Slots if needed. After loading, Close Menu
   should leave the game paused; reopen and select Resume to continue playing.
3. Try opening while already paused. Compare Resume with Close Menu. Check nested Back, opening
   chord release, focus loss, and reconnect while held using the existing input regression matrix.
4. Select an occupied save slot, Reset, Stop and Exit; the first confirmation activation should
   cancel. Move to Confirm to execute. After stopping, launch another game from the native list
   and verify that it plays without automatically opening the menu.
5. Request Screenshot, then Resume and verify the PNG in Dolphin's normal screenshot directory.
   Test Settings → Advanced Settings (Desktop), and Change Disc (Desktop) for a disc game.

Physical controllers, live NetPlay, hardcore/achievement modification workflows, native pickers,
fullscreen/window-system transitions, other platforms/backends and release accessibility checks
remain manual validation. This slice does not claim the standalone startup/library milestone.
