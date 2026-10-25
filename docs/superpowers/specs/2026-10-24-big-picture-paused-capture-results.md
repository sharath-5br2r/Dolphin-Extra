# Big Picture slice 2 — Paused presentation and capture

**Date:** 2026-10-24

**Status:** Complete on Windows/D3D11, with automated ownership/input verification

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 2

## Ownership and scheduling

Paused UI rendering uses the existing graphics owner. Single-core services bounded UI work from
the paused CPU loop, with a condition-variable wakeup that does not resume or step the guest.
Dual-core and deterministic GPU modes service it from the video loop, before the paused-state
return. A dedicated UI wakeup reaches the deterministic owner without scheduling guest FIFO work
or guest ticks. No timer thread submits into `AsyncRequests`.

`Common/BigPicture/PresentationState` carries immutable snapshots, a bounded value-event queue,
atomic capture flags, frame/input epochs and presentation leases. A visible, focused menu requests
approximately 60 Hz presentation; inactive menus stop presenting. The normal single-core paused
wait remains indefinite without UI work, and the pre-existing video idle timeout stays 100 ms.
Explicit overlay activation can request two frames to apply focus and publish its capture result.

Full graphics access suspends new leases/wakeups and drains an in-flight UI present. FIFO locking
also explicitly wakes and drains the video owner in deterministic mode: an idle loop can still
have a final accepted guest swap queued because deterministic `RunGpu()` does not wake it.
Guest FIFO execution remains disabled during that drain. `CPUThreadGuard` balances the suspension
on unlock; single-core marks its UI callback active so CPU locking and stop wait for it to finish.
Save/load/undo hold a suspension across their CPU-side work, including asynchronous operations.
Paused graphics-config refresh restores the previous running state instead of enabling guest GPU
execution unconditionally. Surfaces continue to resize on the graphics owner.

## Input, capture and pause state

- Qt copies keyboard/mouse events into the generation-tagged queue. Input forwarding no longer
  dereferences `g_presenter` during graphics initialization/teardown or accesses live `ImGuiIO`.
  The render owner drains events before `NewFrame` and publishes capture after rendering.
- Full Qt key codes are preserved, preventing aliases such as Shift/Space. Releases are forwarded;
  Qt auto-repeat forwards UTF-8 text independently of key transitions, preserving repeated chat
  characters without repeatedly closing a menu. Escape requests one Back action while modal.
- Queue overflow, session changes, focus changes and disconnect reset pressed state. A frame epoch
  rejects capture published by an old frame after a reset. Physical input has a separate epoch so
  ordinary desktop initialization/focus changes do not impose menu-release behavior.
- Modal capture blocks guest input and conflicting hotkeys even with background input enabled.
  A dedicated menu input channel remains available. Buttons/directional axes held across transitions must
  return to neutral before reuse, including the opening chord in the menu channel. Expressions
  inspect physical inputs without evaluating stateful functions during suppression; constants and
  absolute inputs remain usable. Full-axis surfaces have no universal release position and are
  excluded from zero-neutral release tests; action deadzone/repeat handling follows in slice 3.
  Relative-input channels discard accumulated motion on transitions.
- Native GameCube-adapter input receives equivalent capture/release filtering, preserving its
  origin/connection flags. This path bypasses `ControlReference` and needed its own filter.
- Opening the in-game menu requests a policy-checked pause and records its session and exact CPU
  pause revision. Closing only reverses that pause. An already-paused game, NetPlay, an external
  pause/resume, frame step, load-state request/completion, focus loss or session change cannot
  acquire or retain the menu's pause ownership. Intervening changes during opening are checked too.
  Explicit Resume continues to use the existing host operation. Cursor capture changes only on
  modal transitions; ordinary configuration updates preserve the desktop cursor behavior.
- NetPlay chat/golf yield interactive focus to the menu. Each render holds a shared overlay
  reference across concurrent stop/reset. Chat messages and activation are queued to the render
  owner, and sending returns to Qt's thread. No Qt callback queries ImGui focus.

## Validation

The normal application and both test binaries build under MSVC 19.51 with pinned Qt 6.8.3. The
optional emusync integration remains disabled in this validation build, as in slice 1.

- CTest passed: **1,578 core tests passed**, with the same two environment-dependent real-preset
  tests skipped. All **202 Qt tests passed** using the Windows platform plugin. The bundle has no
  offscreen plugin; the test setup selects `Binaries/QtPlugins/platforms` explicitly.
- This slice adds **19 core tests and seven Qt tests**. They cover bounded pacing/wakes, draining
  exclusion under concurrent input, stale capture, queue overflow, release retention, background
  input, the opening chord, constant/stateful expressions, absolute surfaces/combined modifiers,
  relative motion, native-adapter origin
  handling, owner-side chat focus, and pause ownership including intervening pause/focus events.
  Review regression tests use Qt key events to verify repeated UTF-8 text, suppression of
  auto-repeat release/press transitions, and full key codes. The application and both test suites
  were rebuilt and rerun after the review fix.
- Final runtime results used two-second idle samples at 1280 × 720; these are feasibility checks
  on hidden windows, not sustained performance or power benchmarks:

| Execution mode | UI frames | Updates/second | Guest ticks, PC and present count unchanged | Lock excludes UI under input | Config/resize/save/load |
|---|---:|---:|---|---|---|
| Single-core | 115 | 57.42 | Yes | Yes | Pass |
| Normal dual-core | 115 | 57.44 | Yes | Yes | Pass |
| Deterministic dual-core | 113 | 56.44 | Yes | Yes | Pass |

All three runs retained key down/up, preserved full Qt-style key codes without the Shift/Space
alias, stopped presenting while inactive, resumed UI presentation on focus regain, stopped the
core normally, cleared capture at teardown, and reported no Dolphin alerts. Resize changed the
actual backbuffer to 1024 × 576. State restore left the saved ticks/PC and paused state intact.

The deterministic experiment exposed a final accepted guest swap surviving an idle-loop wait.
The completed FIFO lock now drains that swap before the baseline/graphics access is established;
the idle interval then keeps the guest present count unchanged. The harness also distinguishes
state loading's explicit restored-frame presentation from idle guest advancement.

Formatting with clang-format 22.1.3 and `git diff --check` passed. The application and both normal
test targets were built after removing the excluded probe from the CMake configuration.

The excluded harness uses the production owner loops, real game boot/pause/stop, a hidden D3D11
surface, muted audio and separate profiles under ignored `build/big-picture-host/`. It draws an
interactive ImGui control through `host_ui_frame_event` and measures idle UI presentation without
submitting per-frame CPU jobs. It checks full-lock exclusion while another thread continuously
queues input/wakeups, key down/up, focus throttling/recovery, paused VSync changes, native resize,
state save/load and teardown capture reset. State restoration legitimately presents a loaded
frame; its checks separately verify restored ticks/PC remain paused, rather than counting that
explicit operation as an idle guest frame.

Logs and the excluded harness remain in that ignored build directory. Temporary dependency links
are removed after validation, and the normal CMake configuration does not retain the probe hook.
No forms, translatable product strings, fonts or distribution assets changed.

## Remaining scope

There is still no advertised Big Picture entry point or complete menu renderer. The next slice is
3: input bindings and the navigation toolkit; screen actions follow in slice 4. The production
handoff is available through the existing host bridge and the owner-side UI frame hook.

Runtime checks here cover Windows/D3D11. Linux/macOS, the remaining graphics backends, native
NetPlay sessions and physical controller/adapter/real-Wii-Remote hardware checks remain platform
and release gates. Native real-Wii-Remote/Bluetooth-passthrough report filtering is outside this
emulated-input bridge; it must be handled or reported unavailable before advertising a menu for
those hardware configurations. Full controller navigation/disconnect testing follows slice 3.
