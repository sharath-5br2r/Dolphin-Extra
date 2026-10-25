# Big Picture slice 5 — Standalone runtime and entry/exit

**Date:** 2026-10-25

**Status:** Complete on Windows; other platforms and physical-device checks remain release gates

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 5

## Entry points and behavior

View → Big Picture enters without a running game. Config → Big Picture includes a persisted
“Start Dolphin in Big Picture mode” checkbox, disabled by default. The GUI also accepts
`--big-picture`, including alongside a game argument. It rejects combination with `--batch`;
the non-GUI frontend does not advertise or accept the option. Batch launch ignores the saved
startup preference. First-run setup still runs before the main window is created.

The initial screen offers Play for the selected native-list game or configured default ISO,
Open Game (Desktop), Settings, Toggle Fullscreen, Return to Desktop, and Exit Dolphin.
Settings includes the existing shared Menu Preferences and Advanced Settings (Desktop).
The full scanned library, controller file/folder picker and covers belong to slice 6;
this landing screen deliberately uses existing selection/default-game data.

Boot releases the idle graphics owner before starting normal emulation. Successful boot starts
gameplay with the menu closed. Stop returns to standalone Big Picture when it was entered
through these entry points; ordinary desktop play/stop retains its native game-list behavior.
Return to Desktop releases graphics and input capture. View → Big Picture can enter again.
Exit uses the Cancel-first confirmation and drains the graphics owner before application exit.
Open Game and Advanced Settings release the idle renderer before opening native UI; cancelling
the game picker returns to Big Picture. A disappeared selected file reports an in-menu error.
Failed backend initialization or repeated backbuffer-bind failure returns to the desktop with
an explanatory warning.

## Graphics ownership

`StandaloneSession` owns a dedicated graphics thread only while Core is uninitialized. It
initializes, presents, handles coalesced surface/DPI/fullscreen changes, and destroys graphics
on that thread. Qt polls completion rather than joining a driver while blocking native messages.
The active-owner registry remains occupied until teardown is acknowledged and the thread joined.
Boot is staged until that acknowledgment; return from a game joins the old core owner before
recreating standalone graphics. Presentation generations invalidate commands from old surfaces.
Renderer recreation preserves an existing inline boot error.

The backend adapters share device/surface and utility-buffer setup with normal initialization,
then branch before guest resources. The idle path creates no EFB, texture cache, guest shader
cache, post processor, frame dumper, bounding box, performance-query object or guest FIFO.
It draws through `OnScreenUI` and presents directly, bypassing normal XFB/VI/frame events and
the guest-state restoration at the end of normal utility drawing. D3D12 command submission
allows the absence of guest queries. OpenGL keeps its context alive through shader/sampler
destruction. Metal wraps owner initialization/presentation in autorelease pools.

The owner uses the shared presentation scheduler with a 60 Hz cap and sleeps while inactive.
Surface notifications occupy bounded, replaceable slots. Repeated bind failures report failure
and drain the device. Hardware backend configuration changes coalesce into an idle renderer
restart; capability discovery cannot unload backend libraries while the idle device owns them.
Fullscreen respects Dolphin's borderless/exclusive policy and forwards exclusive focus changes
to the graphics owner. Null, Software and headless surfaces cannot start the standalone UI.

## Validation

Windows Release Dolphin and both test binaries build with MSVC 19.51 and pinned Qt 6.8.3;
optional emusync remains disabled as in preceding slices.

- **1,620 core tests passed**, with two existing environment-dependent preset tests skipped.
- **215 Qt tests passed**, using the bundled Windows platform plugin. This Qt distribution
  has no offscreen platform plugin, so an offscreen launch is unavailable here.
- The user tested the supplied Windows build and confirmed that all tested behavior worked
  as expected.
- New coverage includes sole-owner enforcement, stop during creation, failed creation,
  persistent surface failure, bounded surface/DPI requests, focus wake, command-line policy,
  landing action/path routing, pending Boot preservation, native boot without pause ownership,
  renderer recreation with an inline error, startup persistence and the Designer View action.
- The real Qt app exercised cold startup, keyboard Back to desktop, View re-entry, keyboard
  Play, actual game boot, menu pause/confirmed stop, return to standalone, missing-file failure,
  failed core boot after graphics handoff, idle backend changes, fullscreen,
  unsupported-backend recovery and confirmed idle Exit.
  Separate-window startup used the CLI option; embedded startup used the saved preference.
  Both flows checked exclusive ownership during boot and release before application exit.
- Windows D3D11, D3D12, Vulkan and OpenGL all resumed idle presentation after backend changes.
  Null returned to desktop and subsequent D3D11 entry recovered.
- An excluded D3D11 probe captured the standalone landing screen at 720p, 1080p and 4K.
  All three captures were inspected. It observed no guest framebuffer, zero guest presents,
  zero alerts, and complete graphics release after draining.

Integration probes and isolated muted profiles live under ignored `build/big-picture-host/`.
The automated desktop cannot reliably provide Windows foreground focus: the app probe forces
the shared presentation focus flag and injects Qt keyboard events/queued host commands. These
checks verify production rendering, navigation routing and lifecycle behavior, not physical
controller input or native focus correctness. Linux X11/Wayland, macOS Metal/OpenGL, real
controller reconnect, monitor/DPI changes and device-loss hardware cases remain release checks.
Core-specific boot alerts still use existing error handling; complete controller-facing error
routing and release hardening remain slice 10.

The final test build excludes the temporary probe hook. Dependency junctions used to access the
existing pinned dependency checkout are removed after verification.

## Trying the slice

1. Open View → Big Picture without starting a game. Navigate with arrows/Enter/Escape or the
   bindings configured in Config → Big Picture. Try Settings, fullscreen and Return to Desktop.
2. Select a game in the native list (or configure a default ISO), enter Big Picture and Play.
   Open the in-game menu and confirm Stop; the standalone screen should return.
3. Enter again after returning to desktop. Try the startup checkbox and GUI `--big-picture`.
4. Compare embedded and separate render windows. Change the idle graphics backend, resize,
   enter/leave fullscreen, and check focus loss and capture with a real controller.
5. Select an unsupported backend and verify the desktop fallback; choose a supported backend
   and enter again. Confirm idle Exit and verify that Dolphin closes cleanly.
