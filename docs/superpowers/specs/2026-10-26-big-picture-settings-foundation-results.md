# Big Picture slice 7 — Shared settings foundation

**Date:** 2026-10-26

**Status:** Implemented and validated on Windows; common settings pages follow in slice 8

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 7

## Shared model and operations

`Common/Settings/Descriptor.h` defines value-only descriptors, states, requests and snapshots.
Stored values retain their types, including exact 64-bit values used by existing mapped desktop
controls. Choice values and labels are separate. Numeric controls carry stored ranges, steps,
display transforms and units; mapped sliders retain exact ordered ticks, including non-monotonic
values. Defaults come directly from the typed `Config::Info`, and per-game support is explicit.

`UICommon/Settings` supplies the immutable catalog and owner-thread service. The catalog is
bootstrapped explicitly when MainWindow creates the service; no Qt settings panes are opened
to discover settings. Unsupported settings, including compound DSP controls and controller
profiles, are not inferred from the widget registry. They need dedicated migration/adapters.

The initial supported catalog contains 14 settings:

| Group | Supported settings |
|---|---|
| Interface | Start in Big Picture |
| Audio | Volume, mute on unlimited speed, available backend implementations |
| Display | Available Big Picture graphics backends, aspect ratio, VSync, integer scaling, post-processing preset path |
| Graphics | Reversed EFB-access toggle, exact texture-cache accuracy ticks |
| Advanced representatives | CPU override toggle and percentage, MEM1 size in MiB |

These advanced representatives prove float and scaled-u32 transformations against real desktop
controls; this slice does not add an advanced Big Picture page. Dynamic choices are supplied by
the host environment. Removing an option preserves its stored value and marks it unavailable,
instead of selecting or saving the first remaining option. Backend implementation discovery is
cached for the process; dynamic devices/profiles remain work for slice 8.

The service validates stored types, choices, ticks, ranges and display conversions before
writing. It rejects non-finite numbers and invalid operations. Runtime policy distinguishes
live edits, idle-only changes and settings applied at the next start. Context support,
transition state, NetPlay/Hardcore restrictions, dependencies and backend capability are checked
again when a request executes. Capability information is sampled after graphics initialization
publishes readiness, rather than reading it concurrently during initialization.
MEM1 edits require RAM override to be enabled in the selected base/effective context, matching
the desktop pane; disabled edits leave the stored size unchanged.

## Layers and lifetime

The service supports three distinct contexts:

- **Effective:** reads effective config and uses the desktop's `SetBaseOrCurrent` write policy.
- **Base:** reads/writes only persistent global values, even when runtime overrides exist.
- **Game:** owns a local-game layer and optional shipped-game fallback through shared pointers.
  Reads use local → shipped → base/default; writes never modify the base or shipped layer.

Reset writes the typed default. Clearing a per-game override removes the local key and reveals
the inherited value. Snapshots include the inherited value, override state and source layer.
Explicit save delegates to the selected layer's existing config loader; loader I/O reporting
retains its existing behavior. Runtime-only values are not silently persisted into the base.

The Qt binder delegates read/write/fallback/clear decisions to the same Qt-free layer logic.
Its existing raw layer pointers remain scoped to widget lifetimes; the service uses owned
contexts. Published snapshots retain neither layers nor widgets, callbacks or graphics objects.
Changing context, environment/options or external config invalidates outstanding requests.
Owner-thread checks reject off-thread reads and mutations. The catalog cannot be mutated by
opening or closing widgets.

## Desktop and Big Picture integration

`Config/Binder/SharedSettings` adapts canonical descriptors to existing Designer controls.
The startup checkbox, volume, aspect/VSync/integer-scaling controls, preset path, EFB/accuracy
controls and representative CPU/memory sliders consume the catalog's values or transforms.
The existing widget registry retains typed mapped values and exact ticks for parity checks;
its historical per-game merge flag does not grant service editability.

MainWindow publishes translated settings snapshots through `BigPictureHost`. Typed edit
requests travel through the existing input/host queue and are checked against both the menu
generation/input epoch and settings revision. Only an open settings screen can dispatch edits.
The renderer can read the snapshot without accessing live config or the Qt widget registry.

This is the foundation slice. Existing menu preferences remain usable; the interface/audio/
display/post-processing pages and global/per-game context selection are implemented in slice 8.

## Validation

- Windows Release `Dolphin-Settings.exe`, `tests.exe` and `qt-tests.exe` build with MSVC 19.51
  and Qt 6.8.3; optional emusync remains disabled as in preceding slices.
- **1,649 core tests passed**, with two existing external-preset fixture skips.
- **243 Qt tests passed**. New coverage includes cold catalog availability, typed mapped
  values, non-monotonic ticks, reverse booleans, display transforms and invalid numbers,
  base/effective/game contexts, shipped fallback, reset versus clearing, persistence through
  existing loaders, immutable snapshots, dynamic options, policy and stale/off-thread rejection.
- Designer parity checks compare real aspect-choice labels/values and CPU/MEM1 slider ranges
  with the catalog. Bidirectional Qt/service tests exercise effective and layered controls.
  A 64-bit mapped-value regression checks adjacent values above the exact-double integer range.
- An excluded real-app probe cold-started Big Picture, found all 14 settings and backend
  choices without constructing a SettingsWindow, edited volume through queued typed requests,
  rejected an old revision, reset the default, restored the isolated profile and exited cleanly.
- The library regression probe passed desktop-menu entry on D3D12 with embedded/fullscreen
  presentation, two game boot/stop cycles, paused disc change and insertion after resume,
  backend changes through D3D12/Vulkan/OpenGL/D3D11, desktop return/reentry and clean exit.
- New source labels, help and disabled reasons pass gettext extraction. Formatting and
  `git diff --check` pass.

Probes and isolated profiles live under ignored `build/big-picture-host/` and are excluded from
the final build. The normal profile and the preceding `Dolphin.exe` test build are preserved.
Automated app probes inject queued commands and force presentation focus; physical input,
other operating systems and device-loss behavior remain manual release checks. No new common
settings pages or controller-profile workflow is claimed by these tests.

## Trying the slice

Use `build/big-picture-host/out/Binaries/Dolphin-Settings.exe` for regression testing. Open the
existing desktop audio/graphics/advanced controls and check that volume, aspect/VSync, EFB
accuracy, CPU clock and memory-size values retain their normal behavior. For per-game graphics
settings, check inherited values and right-click clearing. Repeat Big Picture startup,
launch/stop and desktop return. New controller-operated common settings pages follow in slice 8.
