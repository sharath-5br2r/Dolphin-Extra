# Big Picture slice 8 — Common settings and controller profiles

**Date:** 2026-10-26

**Status:** Implemented and validated on Windows; physical-input/platform checks remain manual

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 8

## User-facing workflow

Open Settings directly from the top-level Big Picture toolbar or the in-game menu. Library
Options does not contain global settings. The settings home page selects Global
settings or Game settings for the selected library title/running game. A title without a usable
game ID cannot create a game INI; the page explains how to select a supported game.

The controller-operated pages use the existing navigation toolkit and shared settings service:

- **Interface and menu:** startup Big Picture preference, confirm/back swap, menu scale and safe area.
- **Audio:** volume (exact integer percentages), mute on unlimited speed and available audio backends.
- **Display:** supported graphics backends, internal resolution, aspect ratio, VSync and integer scaling.
- **Post-processing:** existing preset configuration, asynchronously discovered folder/preset rows,
  refresh and Off. Both post-processing engines keep their existing configuration/loading paths.
- **Controllers:** physical input device and existing user/stock profile selection for the first four
  emulated GameCube controllers and Wii Remotes. Full mapping and controller-type setup stay on desktop.

Setting editors show the stored value, context/title, inherited value, override state, help,
disabled reason and apply timing. Reset to Default writes a typed default; Use Inherited Value
deletes the local override. Unavailable choices/devices/presets remain stored until an explicit
edit. Idle backend changes rebuild the graphics owner while preserving the logical settings
page. Advanced Settings (Desktop) remains an explicit handoff.

The catalog now has 15 supported settings: slice 7's 14 plus typed internal-resolution choices.
Advanced CPU/memory representatives remain foundation/parity coverage, rather than new menu pages.

## Contexts, persistence and policies

Global pages edit Base explicitly. Game/session/command-line layers can still take precedence;
the editor identifies an active higher-priority layer. Game pages own their local and shipped
fallback layers. While emulating they use the active game's existing layers, so supported live
edits reach the running configuration. Before boot they use the existing game-config loaders.
Writes and clears save only the selected layer. Opening settings again reloads an inactive
game's layers, allowing desktop edits to be reflected without stale cached INI data.

Controller changes require stopped emulation and are blocked during transitions, NetPlay and
movie recording/playback. Global profile selection loads the existing Profile section, updates
references, saves the controller config and regenerates controller textures using existing APIs.
Physical device selection preserves mappings. Game profiles store the existing Controls/PadProfileN
or Controls/WiimoteProfileN user-profile reference; they never load a mapping into the global
controller object. Per-game device selection comes from that profile. Stock profiles remain a
global load action because Dolphin resolves game references against the user profile directory.
Clearing a game profile reveals shipped/global behavior. Missing/invalid profiles do not replace
the mapping; malformed profiles report an in-menu error.

Settings navigation and typed edits both cross the existing presentation/input/host queues.
Commands validate menu generation/input epoch, menu revision or service revision, supported
context and execution-time policies. New menus consume immutable translated value rows, with
no Qt widgets, controller objects, config layers or graphics resources on the renderer.
Device hotplug refreshes the host snapshot; old selections are rejected without changing the
stored disconnected device. Preset scanning runs off the UI thread and drains at shutdown.
Keyboard focus is assigned to the persistent render widget after the desktop entry menu closes.

## Validation

- Windows Release application, core tests and Qt tests build with MSVC 19.51 / Qt 6.8.3.
- 1,652 core tests passed; two existing external-preset fixture tests skipped.
- 255 Qt tests passed using the Windows platform plugin. The bundled Qt package contains no
  offscreen platform plugin, so the offscreen launch cannot run in this environment.
- Added coverage exercises cold pages, queued typed edits, desktop/menu round trips, base/game
  isolation, game INI persistence and clear/reload, active-game layers and shipped fallback,
  typed resolution values, unavailable options, stale/wrong-screen commands, policy changes,
  global profile loading, game profile references, missing files, device persistence/disconnect,
  async preset discovery, capability guards, and separate menu preferences.
- An excluded real-app probe cold-started Big Picture and exercised the settings categories,
  input-queue volume edits, stale rejection, defaults, preferences, preset selection/clearing,
  D3D12 → Vulkan → D3D12 idle backend rebuilds, device/profile selection, per-game overrides and
  clearing, game profile references, real game boot, paused live edits, runtime backend blocking,
  stop to library and clean exit. It uses an isolated user directory and injected value commands.
- Native Windows captures were inspected for the settings home and per-game VSync editor,
  including the title/context, current/inherited values, apply timing and reset/clear actions.
- The final executable without probe hooks was cold-started in Big Picture and closed through
  the native window; the process exited with code 0.
- The library regression probe covers desktop-menu entry, embedded/fullscreen D3D12 presentation,
  repeated boot/stop, disc insertion after resume, backend handoffs, desktop return/reentry and exit.
- Formatting, whitespace checks and translation extraction pass. Probe sources and profiles stay
  under ignored build directories and are excluded from the final executable.

These checks do not substitute for physical-controller navigation, device disconnect/reconnect,
other platforms, device-loss handling or the slice 9 PCSX2 visual comparison. The existing menu
toolkit supplies this slice's layout; full menu appearance unification remains slice 9.

## Manual testing

Build: `build/big-picture-host/out/Binaries/Dolphin-CommonSettings.exe`.
The preceding executables and normal user profile are preserved.

1. Enter Big Picture, open Settings directly from the top toolbar, and check Interface and menu preferences.
   Change a preference, return to desktop Config → Big Picture, and verify the same value.
2. Change audio volume, aspect ratio, internal resolution, VSync and integer scaling. Check their
   desktop controls, change one on desktop, then return to Big Picture and check the round trip.
   Restart Dolphin and verify persistent settings.
3. Choose an installed post-processing preset, navigate its folders, return and select Off.
   Verify the desktop preset field and an actual game image. Advanced parameters remain desktop.
4. With emulation stopped, change the graphics backend. The settings page should survive the
   rebuild. Repeat D3D12/Vulkan/D3D11/OpenGL where supported, then launch and stop a game.
5. Focus a game in the library, open Settings and select Game settings. Verify the title, edit
   VSync/aspect/resolution, and check the override and inherited value. Compare the desktop game
   properties and global values. Check Reset to Default versus Use Inherited Value and restart.
6. Under Controllers choose a device and load an existing profile. Check desktop mapping/device
   values. Disconnect a selected device and check that it is preserved. For a game context,
   select a user profile, boot that game and verify the mapping; check that the global mapping
   remains unchanged after stop. Clear the game profile and boot again.
7. Open in-game Settings while running and while already paused. Live visual/audio changes should
   work where supported; backend and controller changes should explain why they are disabled.
   Back through the pages and verify pause ownership and normal Resume/stop behavior.
