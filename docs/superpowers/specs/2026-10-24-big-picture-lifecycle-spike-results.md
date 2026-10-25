# Big Picture slice 0 — Lifecycle and readability results

**Date:** 2026-10-24

**Status:** Complete on Windows/Direct3D 11

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 0

## Outcome

Dolphin can render a standalone ImGui interface without booting a dummy game or initializing
guest hardware. Its existing paused CPU/GPU dispatch paths can also redraw an actual game's
retained output without advancing guest execution. Preserve those graphics owners; implement
an explicit paced UI scheduler and graphics-access exclusion in slice 2. Deterministic GPU mode
needs an independent wakeup: its existing idle poll limits paused redraws to approximately 10 Hz.

Keep the pinned ImGui and its dynamic font system. The bundled OSD font and an independently
sourced controller-glyph font rendered successfully at 720p, 1080p and 4K. No new FreeType or SVG
dependency is needed for the demonstrated controls.

Two reusable changes survive the experiment:

- `VideoCommon::OnScreenUI::Finalize(false)` completes UI-only frames and services dynamic font
  textures without consulting performance, movie or achievement state. The default argument is
  `true`; existing presenters keep their overlay behavior and draw order.
- Direct3D 11 invalidates its cached framebuffer binding after presentation. A flip-model
  present unbinds the backbuffer from the device; the next frame must rebind it even when the
  framebuffer pointer is unchanged. Ordinary game presentation previously masked this by
  restoring the EFB framebuffer after each present.

The prototype screens and scheduling code are not installed into Dolphin. There is no Big Picture
entry point yet; the next open slice is **1: state, commands and host bridge**.

## Experiment and controls

Populated the repository's pinned submodules. Confirmed `IMGUI_VERSION` is **1.92.2b**, commit
`45acd5e0e82f4c954432533ae9985ff0e1aad6d5`; used its existing dynamic font texture API.

Built temporary, excluded-from-normal-build probe executables against the real `core`,
`uicommon`, `imgui` and Direct3D implementation. Used hidden native Windows surfaces, isolated
profiles below the ignored `build/big-picture-spike/` directory, muted game audio, and the normal
boot/stop path. The probes do not edit the user's profile or game files.

The standalone path creates the device, swapchain, vertex manager, a presenter for surface
metadata, and `OnScreenUI`. It does **not** call `BootCore`, backend `InitializeShared`, hardware
initialization, FIFO initialization, or the normal presenter's post-processor initialization.
It draws directly to the backbuffer and finalizes with emulation overlays disabled. It also
avoids `EndUtilityDrawing()`, whose normal implementation assumes an EFB framebuffer exists.

The game path performs a UI-only create/draw/destroy cycle, boots an existing local GameCube
title through `BootManager::BootCore`, waits for actual guest frames, pauses, and sends redraw
work through `Core::RunOnCPUThread` and `AsyncRequests`. Single-core passthrough runs on the
CPU/GPU owner; dual-core dispatch runs on the video owner. Each request completes before the
next is submitted. After stopping and joining emulation, another UI-only session is created.

Execution modes were verified from `System::IsDualCoreMode()` and
`FifoManager::UseDeterministicGPUThread()`. The title's compatibility INI overrides the base
CPU setting, so the final runs use explicit **CurrentRun** overrides. Dolphin's command-line
layer has lower priority than game INIs and was insufficient. Earlier mislabeled runs were
discarded. The final window style also avoids Windows' tracking-size cap, and captured texture
dimensions confirm that the 4K surface is actually 3840 × 2160.

## Measured results

Each timed interval is approximately two seconds, paced to one request every 16,667 microseconds.
Standalone captures and font warmup happen before timing. These are short feasibility samples
on hidden windows, not benchmarks of visible-window latency, power use or sustained performance.

| UI-only surface | UI frames / seconds | Updates per second | Core stayed uninitialized | Guest ticks / PC unchanged |
|---|---|---|---|---|
| 1280 × 720 | 121 / 2.0071 | 60.29 | Yes | Yes |
| 1920 × 1080 | 121 / 2.0148 | 60.06 | Yes | Yes |
| 3840 × 2160 | 121 / 2.0116 | 60.15 | Yes | Yes |

| Paused game mode, 1280 × 720 | Actual dual / deterministic flags | UI frames / seconds | Updates per second | Maximum dispatch + draw time |
|---|---|---|---|---|
| Single-core | false / false | 120 / 2.0013 | 59.96 | 5.818 ms |
| Normal dual-core | true / false | 120 / 2.0017 | 59.95 | 5.714 ms |
| Deterministic dual-core | true / true | 21 / 2.0725 | 10.13 | 102.715 ms |

All three game runs preserved guest ticks, the program counter, the guest presentation-event
count and the paused state. All returned to a recreated UI-only renderer after normal stop.
After requests drained, a 150 ms `CPUThreadGuard` interval produced no further UI frames.
That quiescent check does not prove exclusion against a future continuously running UI producer.
The final runs reported no Dolphin alerts. The standalone D3D debug queue reported no messages
after the framebuffer invalidation fix.

Before the D3D fix, vertex submission still occurred but subsequent presents discarded UI pixels.
The debug layer identified an unbound render target following flip-model presentation. Corrected
captures show the actual text and controls; a frame counter alone was not sufficient validation.

## Graphics ownership and access rules for implementation

### Paused game

- **Single-core:** The emulation thread owns the graphics context. `CPUManager::Run()` services
  pending jobs while stepping, so host work can draw without executing a guest instruction.
  Do not move the context to Qt or add a second presentation thread.
- **Dual-core, including deterministic:** The video thread owns the context. `RunGpuLoop()`
  drains `AsyncRequests` before its paused-state early return. The existing SPSC producer must
  remain the CPU owner, not arbitrary Qt or timer threads.
- **Wakeup:** Ordinary `RunGpu()` wakes the non-deterministic worker. Deterministic mode relies
  on the 100 ms loop timeout when paused. Slice 2 needs a dedicated host-UI wakeup that does not
  enable guest FIFO processing or schedule guest ticks. Retain bounded pacing rather than a
  busy loop; suspend UI redraws when hidden or inactive as appropriate.

The probe uses `Core::RunOnCPUThread()`'s existing pause/lock/enqueue/unlock sequence to wake the
paused CPU owner. `AddCPUThreadJob()` alone does not notify its condition variable. The production
scheduler needs an explicit paused-job wakeup as well as the video wakeup, without repeatedly
resetting guest controls or confusing a UI update with a resume request.

### Full graphics access lock

Normal pause is not a graphics access lock. A continuous UI scheduler would invalidate the
current assumption that pausing the guest is enough to make all graphics work stop.

Slice 2 must implement these operations in order:

1. Suspend acceptance/scheduling of UI presentation work, including pending wakeups. Associate
   work with a session generation and limit outstanding requests so a slow renderer cannot
   accumulate a backlog.
2. Pause the CPU and drain accepted CPU-to-video UI work on the owning threads. Await an
   acknowledgment that neither queued nor in-flight presentation can touch graphics resources.
3. Grant graphics access to the existing guarded operation. Do not wait for the render owner
   while holding a mutex that its in-flight callback needs to finish.
4. Release exclusive access, check session validity, and restore UI scheduling independently of
   whether guest execution resumes. Already-paused games must remain paused.

Integrate this with `Core::CPUThreadGuard`, the direct FIFO lock users, config callbacks,
surface changes, state operations and teardown. `CPUManager::ExecutePendingJobs()` executes
callbacks with its state mutex released; its instruction-run-loop activity flag is not an
acknowledgment that a UI job has drained. `BlockingLoop::Wait()` can also fail to finish if a
new producer keeps waking it. Neither is a substitute for explicitly suspending that producer.
The temporary synchronous probe is deliberately not the production scheduler.

### Standalone session and game handoff

Give the UI-only session a dedicated graphics owner while the core is uninitialized. Destroy
its ImGui, surface and device resources on that owner before letting the game's existing owner
initialize graphics. On stop, join the old owner and recreate the UI-only session. Keep logical
menu state in host snapshots across these boundaries; reject results from old generations.

Backend extraction work for slice 5:

- Separate utility device/surface/vertex setup from guest command-processor, FIFO, pixel-engine,
  EFB, texture-cache, shader-manager, custom-resource and graphics-mod initialization.
- Separate UI presentation from the normal presenter's VI aspect-ratio, auto-EFB-scale,
  post-processing, frame-dumping, suggested-window-size and EFB-restoration work. The probe uses
  a fixed stretch layout and fixed scale for surface metadata; production must have an explicit
  guest-independent path rather than inheriting these assumptions.
- Keep device-library acquisition paired with destruction. D3D capability discovery temporarily
  loads and unloads compiler libraries, so it must happen before the active device is created;
  calling it afterward invalidated the probe's compiler function pointers.
- Preserve D3D11's binding invalidation fix. Audit D3D12 command-list/resource-state handling,
  Vulkan state tracking/render-pass submission, OpenGL context/utility-state restoration and
  Metal drawable/encoder ownership on their respective platforms. Only D3D11 was executed here.
- Treat a live UI-only device as an active renderer for configuration capability queries:
  `PopulateBackendInfo()` currently uses core initialization as its guard. Serialize those
  queries with the UI session and rebuild that session for a backend change.

## Font and glyph decision

Inspected rendered captures using the bundled `OSD_Font.ttf`, dynamic text sizes of 24/28/36
pixels in a 720p reference layout, and 32-pixel controller prompts, scaled by surface height.
Text, selection rows and independently loaded glyphs are visible at all three actual resolutions.
Use the ImGui 1.92 per-font-size API for menu text instead of modifying the diagnostic OSD size.

The glyph experiment used [PromptFont v1.10 from its author's release](https://github.com/Shinmera/promptfont/releases/tag/v1.10),
downloaded independently of either emulator. Its release includes the SIL Open Font License 1.1
and requests attribution to Yukari "Shinmera" Hafner. The tested TTF's SHA-256 is
`742889883800bac0b4540113b42e65df72057dcddda204a6400d86f3b7745ed2`.
No third-party font or glyph header is added to Dolphin by this slice.

Use a separate glyph font and explicit prompt mapping, with readable text fallback. PromptFont
assigns glyphs to ordinary Unicode positions such as arrows; unrestricted merging could replace
normal text characters. Bundle a pinned font and its license/attribution in slice 3 after choosing
the required device families. Non-Latin coverage, fallback fonts, long translations, DPI changes,
accessibility sizing and review at physical TV viewing distances remain navigation/release checks;
native pixel captures alone do not establish them.

## Validation and remaining scope

- Pinned submodule initialization completed; no gitlink updates are part of this change.
- Real standalone rendering, dynamic font upload and correctly decoded RGB10_A2 texture readback
  were exercised at all three resolutions.
- Real game boot/pause/redraw/stop/UI recreation passed in the three verified execution modes.
- Normal Release builds of `dolphin-emu`, `tests` and `qt-tests` passed under MSVC 19.51.
- CTest from `build/big-picture-spike/out/Source/UnitTests` passed: 1,540 core tests passed and
  two environment-dependent real-preset tests skipped (`SlangCompile.RealPresetCompilesAllPasses`
  and `LibrashaderParameters.EnumerateRealPresetFromEnv`). The build root has no CTest registration.
- All 192 Qt tests passed using `-platform windows`, with the build's Qt DLLs on `PATH` and the
  bundled plugin directory selected. The Windows Qt bundle has no offscreen plugin; the initial
  offscreen launch could not run and is not counted as a successful check.
- The available clang-format 22.1.3 dry-run and `git diff --check` passed for the retained C++
  changes; documentation local links were checked. No translatable product strings changed.

Local experiment sources/captures are under the ignored `build/big-picture-spike/` tree. No probe
include remains in `Source/CMakeLists.txt`. The local `inject.cmake` can attach the excluded
targets using `CMAKE_PROJECT_dolphin-emu_INCLUDE` for another experiment without editing product
CMake files. They are not shipped, and no reference-emulator implementation was copied.

This establishes feasibility and the ownership design. Production asynchronous presentation,
capture and pause ownership remain slice 2; backend-wide standalone startup remains slice 5.
Cross-platform runtime verification remains required before release.
