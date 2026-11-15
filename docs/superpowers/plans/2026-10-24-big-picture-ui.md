# Dolphin Big Picture UI — Implementation Plan

**Date:** 2026-10-24

**Status:** Implementation in progress; slices 0–9 implemented, plus the PCSX2 gap follow-ups
(#34–#41) including controller mapping; slice 10 hardening, appearance review and
platform/hardware gates pending

**Dolphin baseline:** `dd2cae1601216a4e254424d37d57ba47da1c2221`

**Scope:** Desktop DolphinQt on Windows, Linux and macOS

Add a controller-operated fullscreen library and an in-game menu using Dolphin's existing
ImGui renderer. Users should be able to start Dolphin, pick a game, play, open the menu, manage
save states, change common settings, stop back to the library, and exit using a controller.
Ship a useful in-game menu first, then complete the standalone Big Picture experience.

This plan develops the earlier [fullscreen OSD scoping document](../specs/2026-09-18-fullscreen-osd-followup-design.md).
Its assumptions about rendering lifetime and the settings registry need the additional work below.
The completed Designer-based desktop UI remains the desktop frontend.

## 1. Reference implementations and applicable patterns

Inspected the supplied forks at these revisions, rather than assuming they match upstream:

| Reference | Revision | What to adopt as behavior |
|---|---|---|
| PCSX2 | `32950b968d7eb2f828d7ea2de4224073b2727b82` | Large list and grid controls, explicit focus restoration, contextual controller prompts, global/per-game settings, pause-state preservation, startup and return-to-desktop flows |
| GooseStation | `1f6726c409800d761d9d3f6584ec45c03f6a134c` | Landing/library/pause screen routing, asynchronous cover loading, idle presentation while paused, controller-friendly dialogs and save-state selection |

Primary source entry points:

- PCSX2 [screen lifecycle and pause menu](https://github.com/ilyalissoboi/pcsx2/blob/32950b968d7eb2f828d7ea2de4224073b2727b82/pcsx2/ImGui/FullscreenUI.cpp),
  [fullscreen widgets](https://github.com/ilyalissoboi/pcsx2/blob/32950b968d7eb2f828d7ea2de4224073b2727b82/pcsx2/ImGui/ImGuiFullscreen.h),
  [input routing](https://github.com/ilyalissoboi/pcsx2/blob/32950b968d7eb2f828d7ea2de4224073b2727b82/pcsx2/ImGui/ImGuiManager.cpp),
  and [Qt host startup](https://github.com/ilyalissoboi/pcsx2/blob/32950b968d7eb2f828d7ea2de4224073b2727b82/pcsx2-qt/QtHost.cpp).
- GooseStation [screens and settings](https://github.com/ilyalissoboi/goosestation/blob/1f6726c409800d761d9d3f6584ec45c03f6a134c/src/core/fullscreen_ui.cpp),
  [fullscreen widgets](https://github.com/ilyalissoboi/goosestation/blob/1f6726c409800d761d9d3f6584ec45c03f6a134c/src/util/imgui_fullscreen.h),
  and [Qt host idle loop](https://github.com/ilyalissoboi/goosestation/blob/1f6726c409800d761d9d3f6584ec45c03f6a134c/src/duckstation-qt/qthost.cpp).

Both separate screen state, reusable fullscreen widgets, host actions and input translation.
PCSX2 explicitly enables idle graphics work; GooseStation continues presentation outside system
execution. Both remember whether opening the menu caused the pause. These are architectural
requirements for Dolphin, not just visual styling examples.

For visual unification (slice 9), the layout and style target is the local
[`ilyalissoboi/pcsx2` fork](https://github.com/ilyalissoboi/pcsx2), inspected at
`adca25d5995fbec3dbabbc061518d332a5ea935b` in `E:/work/pcsx2` on 2026-10-25.
Its [screen composition](https://github.com/ilyalissoboi/pcsx2/blob/adca25d5995fbec3dbabbc061518d332a5ea935b/pcsx2/ImGui/FullscreenUI.cpp),
[layout metrics and controls](https://github.com/ilyalissoboi/pcsx2/blob/adca25d5995fbec3dbabbc061518d332a5ea935b/pcsx2/ImGui/ImGuiFullscreen.h),
and [themes, focus styling and footer](https://github.com/ilyalissoboi/pcsx2/blob/adca25d5995fbec3dbabbc061518d332a5ea935b/pcsx2/ImGui/ImGuiFullscreen.cpp)
define the reference. Preserve the earlier architectural inspection above; record the actual
reference revision and comparison captures when implementing the visual slice.

Implement these behaviors with Dolphin's APIs and independently authored code and assets.
Preserve Dolphin's existing GPL-2.0-or-later contribution baseline. The inspected PCSX2 files
declare GPL-3.0+; GooseStation files have GPL-3.0 or alternative-license headers. Treat the forks
as behavioral and visual references, without transplanting their implementation. Source any glyph fonts
independently and include their license and attribution.

## 2. What already exists, and what is actually missing

Paths below are relative to the repository root.

| Existing code | Reuse | Gap to resolve |
|---|---|---|
| `Source/Core/VideoCommon/OnScreenUI.{h,cpp}` | ImGui context, dynamic font texture updates, backend-independent drawing, keyboard/mouse entry points | Fullscreen controls, gamepad events, capture snapshots and screen routing |
| `VideoCommon/Present.cpp` | Composite UI over the current XFB; can present without an XFB once initialized | Presentation scheduling while paused; initialization without an emulation session |
| `VideoCommon/Fifo.cpp` | GPU execution and synchronization | `RunGpuLoop()` returns while paused; `PauseAndLock()` must still guarantee exclusive access |
| `VideoCommon/VideoBackendBase.cpp` | Existing backend/device implementations | `InitializeShared()` initializes guest graphics state, FIFO and caches; it is not a UI-only device constructor |
| `DolphinQt/RenderWidget.cpp`, `Host.cpp` | Native surface, fullscreen, focus, cursor and event integration | Event forwarding requires a running core; Escape also triggers existing host behavior; capture reads live ImGui state |
| `DolphinQt/HotkeyScheduler.cpp`, `InputCommon/ControlReference/` | Host input polling and expression-based bindings | Independent menu controls available before boot and while paused; prevent simultaneous game/hotkey activation |
| `DolphinQt/GameList/GameListModel`, `GameTracker`, `UICommon/GameFileCache`, `UICommon/GameFile` | Library discovery, metadata, covers, tags and play time | Immutable menu-facing snapshots and a bounded GPU cover cache |
| `DolphinQt/Config/Binder/ConfigSettingRegistry` | Location, kind, labels, help, choice labels and numeric ranges | Startup completeness, typed choice values, defaults, read/write/reset operations, dependencies and runtime policy |
| `Config/Binder/ConfigBindingLogic.h` | Existing layer selection and inheritance semantics | A reusable settings service independent of widget instances and raw layer lifetimes |
| `Core/State.h` and `DolphinQt/MainWindow.cpp` | Ten state slots, timestamps, save/load actions and normal boot/stop flows | Visual browser, command completion/error reporting and optional thumbnail sidecars |

Two pitfalls must be addressed explicitly:

1. **Drawing a pause menu once does not make it interactive while paused.** Input polling and
   presentation must continue without advancing guest execution. Single-core and deterministic
   modes need their own ownership treatment; changing only the normal dual-core FIFO loop is
   insufficient. Existing `Host_UIBlocksControllerState()` even has a paused-state exception
   pending asynchronous presentation.
2. **The settings registry is a seed, not a complete settings engine.** Entries are recorded when
   widgets bind. `BindMapped()` records display labels without its underlying typed values;
   mapped slider ticks also lose information. Entries use `QString`, are not synchronized, and
   returned spans/pointers are invalidated by subsequent registration. Do not construct hidden
   settings dialogs to populate it, or read it directly from the graphics thread.

At the planning baseline, the ImGui submodule pinned to
`45acd5e0e82f4c954432533ae9985ff0e1aad6d5` was not populated in this checkout. Slice 0 populated
the pinned dependencies and confirmed ImGui 1.92.2b and its dynamic font API.
Do not add FreeType/SVG dependencies or upgrade ImGui merely to imitate a reference frontend.

## 3. User experience and initial scope

**Entry:** A desktop menu action, a configurable Open Menu hotkey, a startup preference, and a
new GUI-only `--big-picture` option. Big Picture presentation and native window fullscreen are
separate state: the menu can also be exercised in a window. Existing fullscreen and batch
semantics remain explicit; reject `--big-picture --batch` with a useful argument error.

**Home/library:** Games, Settings, Return to Desktop, and Exit Dolphin. Default to the library
when games exist; show Add Game Folder and Refresh for an empty library. Offer cover grid/list,
search, platform/region filters, sorting and existing tags. Remember selection by game path,
not row index. Use the existing GameCube/Wii title metadata and launch path, including second
disc handling. Include GameCube IPL and Wii System Menu actions when available.

**In-game menu:** A dimmed game frame, title/cover, Resume, Save State, Load State, Settings,
Change Disc where applicable, Reset, Screenshot, Stop to Library, and Exit. Use an opaque
background if no usable frame exists. Confirmation dialogs default to Cancel. Nested Back
returns one level; closing the menu preserves the pause state that existed before opening it.
The explicit Resume action resumes a previously paused game when permitted.

**Navigation:** D-pad/left stick, confirm, back, shoulder page/tab movement and menu toggle,
with keyboard/mouse equivalents. Show prompts for the active device and allow confirm/back
swapping. Use neutral text prompts if device identity is unknown. Opening/closing consumes the
triggering button or chord until released. Supply an on-screen keyboard and file/folder picker
for the common controller-only flows, including folder setup, search and disc selection.

**Settings:** Start with interface/menu, volume/audio, resolution/aspect ratio, VSync and
post-processing controls; then controller device/profile selection and per-game overrides.
Show disabled reasons and whether a change is immediate, requires stopping, or requires restart.
Reuse the current post-processing configuration and capabilities; do not rebuild either shader
engine. Keep existing Qt configuration paths available for advanced tools.

Initial release excludes a new Android frontend, a new no-GUI frontend, debugger/TAS UI,
NetPlay session management, achievement account management, a replacement memory-card manager,
and complete expression/motion controller mapping. Those can remain desktop actions with a clear
transition. Existing NetPlay, movies and achievements still constrain which menu actions work.
Save-state preview thumbnails are optional polish, not a dependency of the state browser.

## 4. Architecture and ownership

Use the same backend-independent ImGui drawing path for library and in-game screens. Keep Qt out
of `VideoCommon` and keep emulator operations out of drawing callbacks.

Proposed modules, with names finalized during implementation:

- `Common/BigPicture/`: lightweight, Qt-free snapshot/command contracts shared with the renderer.
  Do not add a renderer dependency on the full `uicommon` library, which already depends on core.
- `UICommon/BigPicture/`: plain C++ state/actions, immutable library/settings/state-slot
  snapshots, command IDs and results. Keep renderer resource handles out of these models.
- `VideoCommon/BigPicture/`: focusable widgets, screen rendering and GPU texture cache.
  Called by `OnScreenUI::Finalize()` before `ImGui::Render()`. Retain existing OSD/performance
  overlays and arbitrate interactive NetPlay chat/golf focus. Menu visibility is independent of
  `MAIN_OSD_MESSAGES`; menu text size is independent of the diagnostic OSD font size.
- `InputCommon/BigPictureInput`: menu bindings built on `InputConfig`/`ControlReference` and
  existing controller backends. Poll through the existing host input owner, including before
  boot and while paused. Feed semantic events to the UI; enable ImGui keyboard/gamepad navigation
  and backend gamepad capability only when appropriate.
- `DolphinQt/BigPictureHost`: owns mode transitions, host requests, library/settings adapters,
  pause ownership, result publication and the idle render session. Use existing MainWindow
  operations through queued host commands; send CPU work through `Core::RunOnCPUThread` where
  required. Keep all QWidget, model and config-edit operations on their existing owning thread.

### Rendering lifetime

Introduce an explicit UI-only rendering session using the current backend abstractions.
Extract the minimum device/surface/utility-rendering initialization needed from guest graphics
initialization. Reuse `OnScreenUI` rendering; gate guest-dependent overlays and post-processing
work in the UI-only path. Do not boot a dummy title or initialize emulated hardware for a library.

For the first version, **tear down and recreate graphics resources at library/game ownership
boundaries**, preserving logical UI state in host-owned snapshots. This avoids making device
reuse across guest initialization a prerequisite. Exactly one owner may hold the native surface
and graphics globals. Cancel/drain jobs and release textures before teardown; stale events must
carry a session generation and be ignored after a transition. Backend changes while idle rebuild
the UI session and recover to desktop on failure.

Preserve the Qt presentation widget and its native window across those handoffs. Graphics APIs
use a native child retained while compatible and recreated after the old owner drains when
backend/stereo requirements change; input stays attached to the persistent widget.
Boot/stop must not hide or reparent that widget, select the desktop
game list, or leave fullscreen. The PCSX2 transition comparison is recorded in the slice 6
results; Dolphin retains its explicit device-resource teardown at this stage.

When a game is paused, schedule UI-only presents on the graphics owner, redrawing the retained
XFB without ticking the guest. In single-core mode this owner must be able to service UI work
while CPU execution is paused. Distinguish a guest pause from a full graphics access lock:
save/load, debugging, surface changes and shutdown must be able to suspend and drain all presents.
`AsyncRequests` is a CPU-to-video SPSC queue with passthrough behavior, not a general UI-thread
dispatcher; add the necessary ownership-aware dispatch rather than pushing host jobs into it.
Use bounded pacing while visible, throttle when inactive, and sleep when there is no UI work.

### Input and pause ownership

Publish capture flags atomically from the render owner; other threads must not query live
`ImGuiIO`. A modal menu explicitly captures emulated input even with background input enabled:
`Core::UpdateInputGate()` currently permits that configuration to bypass ordinary focus blocking.
Keep menu navigation and the close-menu shortcut available while blocking guest input and
conflicting hotkeys. Do not let the menu's own capture flag disable its input polling.

Track pause ownership per emulation session, with invalidation when another action changes
state. Closing a menu only undoes its own pause. Focus regain, another pause hotkey, debugger
breaks, load-state completion, boot failure and stop must not accidentally resume a game. In
NetPlay, opening a local menu must not locally pause the session; use existing session policy
and block unsafe actions. Handle device disconnect, focus loss and mode transitions by clearing
pressed state; require release before restoring affected controls to gameplay.

### Settings shared with desktop

Extract a Qt-free descriptor/value model and settings operations into `UICommon`, retaining a
Qt adapter for translated text and widgets. Register the supported catalog explicitly at startup,
independent of opening panes. Keep one source of truth for each migrated setting's choices,
constraints and operations; let desktop binders and fullscreen controls consume it incrementally.

A descriptor needs typed values/default, stable ID/location, category/order, label/help source,
choice value/label pairs, exact mapped ticks, units/transforms, supported edit contexts, capability
and dependency checks, and apply/restart policy. Model complex controls with dedicated adapters.
Never infer per-game editability solely from the registry's historical `per_game` merge flag.
Reuse `ConfigBindingLogic` semantics for effective value, shipped game INI fallback, local
override writes and clearing an override. Snapshot translated strings and current values for
rendering; mutations go through the host settings service and publish fresh snapshots.

## 5. Delivery sequence

Implement as small reviewable changes. Each slice below includes a behavior gate, not just a
new set of files. Slices 1–4 produce the first usable in-game release; 5–10 complete core Big Picture.

**Progress, 2026-10-26:** Slices 0–8 are complete; see the
[lifecycle/readability results](../specs/2026-10-24-big-picture-lifecycle-spike-results.md) for
runtime measurements, retained fixes and the ownership design, and the
[state/host-bridge results](../specs/2026-10-24-big-picture-host-bridge-results.md) for the command,
snapshot, confirmation and lifecycle tests, and the
[paused presentation/capture results](../specs/2026-10-24-big-picture-paused-capture-results.md)
for owner scheduling, lock exclusion, capture and pause ownership, and the
[navigation results](../specs/2026-10-24-big-picture-navigation-results.md) for configurable menu
input, controller/keyboard navigation, focus/scroll restoration, text/fonts and native captures,
and the [in-game menu results](../specs/2026-10-25-big-picture-in-game-results.md) for action wiring,
Cancel-first confirmations, the ten-slot browser, desktop handoffs and real save/load flows,
and the [standalone runtime results](../specs/2026-10-25-big-picture-standalone-results.md) for
cold startup, graphics ownership handoffs, desktop return and backend recovery,
and the [library/setup results](../specs/2026-10-25-big-picture-library-results.md) for the shared
catalog, grid/list, search/filters, bounded covers, controller setup and paused disc picker.
The [shared settings foundation results](../specs/2026-10-26-big-picture-settings-foundation-results.md)
record the explicit typed catalog, shared layer operations, desktop adapters and cold-start
settings snapshots. The [common settings/profile results](../specs/2026-10-26-big-picture-common-settings-results.md)
record controller-operated settings pages, persistent global/per-game edits, preset discovery and
controller device/profile selection. The [visual refinement results](../specs/2026-10-26-big-picture-visual-refinement-results.md)
record slice 9's shared dark styling, screen composition, navigation and Windows verification.
The [appearance follow-up](../specs/2026-10-26-big-picture-appearance-parity-results.md) adds live,
persistent theme selection, menu icons, checked setting values, richer library details and
card sizing for large text with maximum safe-area margins.
Reference comparison captures, user appearance review and platform/hardware gates remain open;
final state-browser/release hardening follows in slice 10.
Platform/hardware release checks remain required.

**Progress, 2026-10-30:** A comparison with the local PCSX2 fork's FullscreenUI found feature,
code-quality and performance gaps. The following PRs closed them:
- Per-frame menu rebuilds, per-refresh settings snapshots and cover cache churn
  ([#34](https://github.com/ilyalissoboi/dolphin/pull/34)); library snapshot lists are now shared
  copy-on-write ([#41](https://github.com/ilyalissoboi/dolphin/pull/41)).
- Per-game library options: start, load a game's states without booting, Game Properties
  without a running game, and play-time reset
  ([#35](https://github.com/ilyalissoboi/dolphin/pull/35)).
- Folder/file browsing for path settings and Left/Right number stepping
  ([#36](https://github.com/ilyalissoboi/dolphin/pull/36)); game folder management
  ([#37](https://github.com/ilyalissoboi/dolphin/pull/37)).
- Shared view focus tracking, styles and layout constants
  ([#38](https://github.com/ilyalissoboi/dolphin/pull/38)).
- Controller mapping inside Big Picture: press-to-bind capture with menu input suspended, Clear
  and Default Mapping ([#39](https://github.com/ilyalissoboi/dolphin/pull/39)); Wii Remote
  extensions, tuning settings, profile saving and the menu's own bindings
  ([#40](https://github.com/ilyalissoboi/dolphin/pull/40)).

Section 3's exclusion of controller mapping now covers only expression editing, motion-sensor
passthrough, calibration and controller types, which remain desktop workflows. None of this
work has yet run on controller hardware; it counts towards the platform/hardware gates below.
Optional PCSX2 extras remain unscheduled: a speed toggle, frame dump and leaderboards in the
pause menu; favourites and boot variants in the library; and custom backgrounds,
transitions, notifications and a cover downloader.

| Slice | Work and primary touchpoints | Completion gate |
|---|---|---|
| 0. Lifecycle and readability spike | Populate pinned dependencies; trace `Core`/backend/FIFO lock ownership; prototype paused UI presents in dual-core, single-core and deterministic modes; prototype a UI-only surface; test dynamic fonts and independently sourced controller glyphs at 720p/1080p/4K | Record the ownership design, required backend changes, measured idle pacing and font decision. Demonstrate pause without guest advancement and boot/stop without a dummy game. Keep only reusable production changes |
| 1. State, commands and host bridge | Add mode/screen stack, immutable snapshots, session generation, command/result dispatch and shared action availability. Adapt boot/stop/reset/save/load operations and confirmations from `MainWindow`; update CMake targets | Fake-host tests cover stale commands, nested Back, errors, cancel and boot/stop races. Desktop behavior remains equivalent |
| 2. Paused rendering and capture | Add idle presentation and full-lock exclusion to `Fifo`/core owner paths; render-owner event queue and capture snapshots; update `Host`, `RenderWidget`, hotkey routing and emulated-input gating | Menu updates while paused in all three execution modes; no guest advancement, graphics race, stuck capture, hotkey leakage or lost key-up |
| 3. Navigation toolkit | Add menu `InputConfig`, configurable Open Menu binding and Qt binding page; large rows/cards, focus/scroll restoration, dialogs, prompts, scale/safe-area handling and text/font support | Controller and keyboard can navigate a representative hierarchy without mouse use; disconnect/reconnect and confirm/back swap work |
| 4. In-game menu | Wire Resume, reset/stop/exit confirmation, screenshot, disc action and the state/settings entry points. Use a simple ten-slot browser initially | Play → menu → save/load → return to play works with a controller; already-paused and external-resume cases behave correctly. Core policies are rechecked at execution |
| 5. Standalone runtime and entry/exit | Extract UI-only initialization in backend implementations; add `BigPictureHost` idle owner and explicit graphics handoff; desktop action/startup preference/GUI `--big-picture`; update `Main.cpp`, command parsing, `MainWindow` and `MenuBar` | Cold-start with no game; boot; stop back into UI; exit to desktop; enter again; recover from failed boot/device creation. Embedded/separate surfaces remain usable |
| 6. Library and controller-only setup | Adapt existing game model/tracker; publish path-keyed snapshots; grid/list, filters, tags, sorting and selection restoration; async decoded covers with GPU upload/eviction; empty state, folder browser, text keyboard, refresh and disc picker | Add folder → scan → select → boot → stop → select next game entirely by controller. Removing a selected game or completing a scan does not move focus unpredictably |
| 7. Shared settings foundation | Extend/extract registry and typed operations; explicit supported catalog bootstrap; Qt adapters and descriptor parity checks; preserve layer/fallback semantics | Settings work on cold Big Picture startup without opening Qt settings. Tests prove mapped values, transformed ranges, overrides, reset and dynamic options |
| 8. Common settings and controller profiles | Interface/audio/display/post-processing pages, controller device/profile selection; global and per-game context; inherited indicators, disabled reasons and restart handling | Changes round-trip through desktop and Big Picture using the same stored values. Per-game edits never modify global values; incompatible changes remain blocked |
| 9. Visual appearance and menu unification | Match the local PCSX2 fork's layout and style across Big Picture through shared screen composition, themes, typography, spacing, rows/cards, focus states, dialogs and controller-prompt footer; update `VideoCommon/BigPicture` and `OnScreenUI` | Side-by-side reference review covers landing/library, pause, settings, state slots, dialogs and setup pickers at 720p/1080p/4K and ultrawide. Menus share the same visual rules; controller focus, capture and pause behavior remain correct |
| 10. State browser and release hardening | Async state metadata refresh/results; empty/corrupt/incompatible slots and overwrite confirmation; optional PNG sidecars captured from game output; translations, assets, packaging and platform verification | All release scenarios below pass. Failed saves/loads report accurately, pause ownership remains correct, and missing sidecars never prevent loading. New state-browser surfaces use slice 9's shared layout and style |

Dependencies: `0 → 1 → 2 → 3 → 4`; `5 → 6` follows the ownership model proven in 0–2;
`1 → 7 → 8`; `6, 8 → 9 → 10`. A pause-menu milestone may ship before 5, but it is not completion
of the standalone Big Picture request. Do not block that milestone on exhaustive settings parity.

### Slice 9: PCSX2 visual alignment

Place this slice after library/controller-only setup and common settings, so the visual pass
can unify all major menu types, and before final hardening, so release testing exercises the
finished appearance. Screens introduced in slices 6 and 8 should use the shared toolkit and
the PCSX2 reference from the outset; slice 9 resolves accumulated inconsistencies and completes
the cross-screen visual review. Slice 10's state-browser additions inherit the same primitives.

- Match the reference's screen structure: landing header and menu composition, library grid/list
  with navigation header and game details, pause overlay with title/cover information, settings
  category navigation, and a consistent bottom strip for contextual controller/keyboard prompts.
  Adapt the content and actions to Dolphin's supported features and GameCube/Wii metadata.
- Centralize the theme palette, font hierarchy, spacing, padding, row/card dimensions, borders,
  rounding, overlay opacity and focus/hover/disabled states. Use the reference's 1280×720 design
  coordinate system as the comparison baseline, scaled through Dolphin's existing menu scale,
  DPI and safe-area preferences. Keep diagnostic OSD font sizing independent.
- Apply those rules to the landing screen, library and empty state, pause menu, global/per-game
  settings, controller profiles, save/load slots, confirmation/error dialogs, file/folder/disc
  pickers and on-screen keyboard. Shared components own the visual rules so individual pages
  do not accumulate separate styles.
- Produce equivalent PCSX2 and Dolphin captures and review them side by side at 720p, 1080p,
  4K and an ultrawide aspect ratio. Record the reference theme, revision, display scale and
  intentional adaptations. Check long/non-Latin text, missing covers, empty/busy/error states,
  contrast, clipping, scrolling, focus visibility and device-specific prompts.
- Verify keyboard/controller navigation, confirm/back swapping, focus/selection restoration,
  modal capture, pre-existing pause ownership and repeated library → game → menu → stop flows
  after the visual changes. Completion requires user review of the appearance and behavior;
  screenshot comparison supplements the existing state/input regression checks.

## 6. Validation and release criteria

Add tests around state, ownership and data correctness, using existing `Source/UnitTests` and
`Source/QtTests` infrastructure. Avoid screenshot-only tests for behavior.

- State/host tests: opening while running or already paused; nested dialogs; explicit Resume;
  external pause/resume; cancel stop; asynchronous graceful stop; boot failure; stale callbacks;
  loading a state or closing the app while a menu is open.
- Input tests: capture with background input enabled; menu chord release; axis deadzone/repeat;
  focus loss; reconnect; multiple controllers; menu/hotkey/guest exclusivity. Cover actual input
  backends with hardware checks rather than assuming all devices have a Guide button.
- Settings tests: typed choice mappings rather than list indexes; numeric ticks/scaling/reverse
  bools; dynamic options; cold-start catalog completeness; per-game fallback/clear; save/reload;
  frontend round trips; runtime restrictions and capability-dependent visibility.
- Library/resource tests: concurrent scan updates through snapshots; selected path removal;
  cancellation and resource release at teardown; bounded cover-cache memory; state metadata errors.
- Integration: build `dolphin-emu`, `tests`, and `qt-tests` with tests enabled; run CTest including
  the offscreen Qt suite. Run `git diff --check`, relevant formatting checks, and translation
  extraction/parity tests. Add all new source files/resources to CMake and platform packaging.

Manual matrix:

| Dimension | Required coverage |
|---|---|
| Platform/backend | Windows D3D11/D3D12/Vulkan/OpenGL; Linux Vulkan/OpenGL on X11 and Wayland; macOS Metal and supported OpenGL paths. Software/Null/headless report unavailable UI or return to desktop cleanly |
| Execution | Dual-core, single-core and deterministic GPU; guest paused; frame stepping; no game; booting/stopping; disc change; backend change while idle |
| Display | Windowed/fullscreen; embedded/separate render window; DPI/monitor switch; resize/minimize; 720p/1080p/4K; ultrawide; HDR/stereo where supported; non-Latin text and long translations |
| Input | Xbox-style, PlayStation-style, generic/SDL controller, keyboard/mouse, GC adapter, configured emulated Wii Remote controls; disconnect/reconnect and confirm/back swap |
| Policy | NetPlay local menu, recording/playback, achievements hardcore, pause-on-focus-loss, background input, graceful stop, direct CLI boot, first-run setup and batch-option conflict |

Use the existing core policy for every operation; snapshot availability is advisory, so recheck
at execution time. Critical errors and confirmations in the advertised controller-only flow
must render in the menu. Refactor host confirmation presentation without bypassing safety checks
or creating a blocking dialog/render-lock deadlock. Unsupported advanced workflows should offer
an explicit return to desktop.

Before release, complete the full controller-only path on each supported platform, exercise
repeated library → game → pause → stop cycles, measure UI responsiveness and idle CPU/GPU use,
and verify the disabled feature does not add continuous presentation or input work. Keep normal
desktop launch as the default until these gates pass. Update the feature documentation and
`docs/superpowers/release-notes-next.md` when implementation ships.

## 7. Principal risks and decisions

| Risk | Planned response |
|---|---|
| Idle rendering violates GPU locks or single-core ownership | Resolve in slice 0; add a real graphics-exclusive suspension state and owner dispatch. Do not solve it with a Qt timer calling `Presenter::Present()` directly |
| UI-only device creation expands into a backend rewrite | Extract only utility-rendering prerequisites; recreate devices across game boundaries initially. Record backend-specific work before committing later slices |
| Registry reuse silently writes wrong values or misses unopened panes | Explicit startup catalog, typed mappings and value operations; migrate supported groups incrementally with desktop parity tests |
| Input leaks into gameplay or cannot close the menu | Separate menu polling from guest gating; publish capture, suppress conflicting hotkeys, preserve release events and test background input |
| Pause/stop behavior differs from desktop | Shared host actions and policy; pause ownership tied to session; asynchronous confirmations/results and graceful-stop handling |
| Covers or UI jobs outlive a graphics session | Bounded cache, cancellation/drain and generation checks; GPU resource destruction on the owner |
| Full reference feature parity makes the first release unbounded | Deliver pause menu first, then complete library/common-settings workflow; keep advanced tools accessible through desktop |

The largest uncertainty is graphics lifetime, followed by settings metadata and input arbitration.
Estimate implementation effort after slice 0 establishes the backend changes; existing line
counts are not a reliable effort estimate. Per-slice results record implementation evidence.
