# Big Picture slice 3 — Input bindings and navigation toolkit

**Date:** 2026-10-24

**Status:** Complete on Windows/D3D11, with automated controller/keyboard and ownership verification

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 3

## Input and frontend integration

`InputCommon/BigPictureInput` owns a separate `InputConfig` in `BigPicture.ini`, with four
independent binding sets and normal device selection, expression mapping and profiles. The
existing host-input owner polls Open Menu before guest-affecting hotkeys and FreeLook; navigation
continues while paused and the input configuration exists before boot. It reuses the existing
backend sample rather than adding a device update. Outside a menu, only configured Open Menu
references are evaluated. Keyboard entry remains unbound by default while the in-game menu is
unfinished. XInput's Default mapping supplies Back + Start, D-pad/left-stick directions,
A/B and shoulders; other devices use the existing mapping UI without assuming a Guide button.

Config → Big Picture provides Configure buttons for each of four binding sets; Options → Big
Picture Input remains a shortcut to the same Qt mapping pages. The general Big Picture category
also owns the shared confirm/back swap, interface scale and safe-area controls. These values
round-trip through the existing input configuration as the in-game menu's preference controls;
their storage is unchanged, so previously saved preferences carry over.

Each binding set has independent threshold hysteresis and release retention. Disconnect,
reconnect, focus changes and capture epochs clear old presses and require release before reuse.
Holds from multiple binding sets are combined so one controller's release does not cancel
another's input. ImGui receives named gamepad events on the graphics owner; it owns direction
repeat. Confirm and Back do not repeat actions. Native Qt arrows, Enter/keypad Enter, Escape,
mouse interaction and Page Up/Down remain available without controller bindings.

Opening captures the initiating chord immediately, including the interval before Qt delivers the
request. Pending and visible capture are published together, avoiding a guest/hotkey gap at the
handoff. A rejected request, focus loss, overflow, input reset, session change or renderer teardown
clears pending capture. The render owner does not display a speculative menu before the host
accepts it. Render requests carry both session generation and input epoch; Qt rechecks the epoch
at execution, so an old activation cannot execute after focus loss and regain.

Requests cross a bounded value queue; no renderer callback touches Qt, input configuration files
or emulator operations. Capture and menu input remain separate from ordinary guest gating.

## Toolkit and the available hierarchy

`VideoCommon/BigPicture/Navigation` provides large selectable rows/cards, stable page/item IDs,
focus and scroll restoration, disabled controls, shoulder page movement and dialog layouts.
Dialog callers supply Cancel as the default item. Removed or newly disabled selections fall
back to a usable control. Focus restoration also preserves the previous list scroll position
while ImGui settles its focus request. The navigation cursor is visible as a menu opens, so the
first confirmation press activates the selected control. Scroll offsets use logical units;
resizing restores the selected item and reveals it if changed wrapping would move it out of view.

The initial production hierarchy contains Menu Preferences and Close Menu. Preferences change
confirm/back assignment, interface scale (75–150%) and safe area (0–15%). Close and nested Back
use the existing host bridge and preserve its pause-ownership rules. The complete in-game actions
and their screen wiring remain slice 4; this is not the standalone library or a new startup path.

Layout uses the native viewport, fits 16:9 and ultrawide surfaces, and applies its own text sizes
without multiplying the diagnostic OSD/DPI font scale again. Labels wrap within their control;
the list scrolls independently of the heading and prompts. Prompt text follows the last keyboard
or bound navigation input. Short binding expressions identify remapped controls; unknown or
mixed-device bindings use neutral action text rather than guessing a controller glyph.

## Text, fonts and translation

Product strings are translated on Qt and published as values. Input group/control labels use the
existing `_trans` mechanism. The mapping page uses the existing Designer-generated mapping shell.

Menu font resources are created lazily on the graphics owner. A user `Load/OSD_Font.ttf` override
is preferred; otherwise an installed text font is used, with the bundled OSD font as fallback.
Qt discovers at most one installed Latin and one CJK fallback from its font directories using a
bounded scan. Missing families leave the normal fallback intact. Menu glyphs use ImGui's pinned
dynamic font/texture API and are independent of diagnostic OSD text size. No font bytes, Qt font
objects or GPU handles cross the host boundary.

This slice uses text binding prompts instead of bundling PromptFont. Arbitrary expression
bindings do not provide a reliable fixed A/B or platform-glyph mapping; neutral prompts remain
accurate. Installed system fonts are read in place and no third-party assets or licenses are
added to the distribution. Additional script coverage and shaping remain release checks.

## Validation

The normal Windows Release application and both test binaries build with MSVC 19.51 and pinned
Qt 6.8.3. Optional emusync is disabled, as in the preceding slices.

- Retained tests cover hysteresis, opening-chord release, focus/disconnect/reconnect, finite
  preferences, bounded/stale requests, pending-capture cancellation, input config/profile
  sections, and queued host actions invalidated by focus reset.
- ImGui behavior tests drive keyboard and named gamepad events through a representative
  hierarchy, including first confirmation, repeat delay, Cancel defaults, swapped confirm/back,
  disabled/removed controls, page movement, remembered focus/scroll and long UTF-8 labels.
- Layout tests cover 720p, 1080p, 4K and ultrawide surfaces. A geometry comparison checks that
  diagnostic font and DPI scaling do not change menu text positions or sizes.
- Qt tests check installed-font discovery, duplicate roots, missing families and bounded source
  selection without depending on a particular machine's installed fonts.

All **1,602 core tests passed**, with the same two environment-dependent shader-preset tests
skipped; all **207 Qt tests passed**. This slice adds 24 core tests and five Qt tests. Source-string
extraction with the repository's gettext keywords passed, including the new mapping/menu labels.
The new Configuration category uses a Designer form; no translation catalog was changed.
Formatting and `git diff --check` passed.

The virtual-controller D3D11 probe passed at **1280 × 720, 1920 × 1080 and 3840 × 2160**. Each run
confirmed that the opening chord fired once, preferences opened, confirm/back swapping applied,
a second controller's reconnect while held caused no activation, Back restored the parent, and
the menu closed. Japanese and Cyrillic glyph availability was checked against the actual menu
font. Native backbuffer captures were inspected at 720p and 4K; the larger runs also applied an
artificial 2× diagnostic DPI scale. All final runs reported zero alerts. One first-run profile
initialization reported a temporary FST rename warning during an overlapping suite run; sequential
reruns at all three sizes completed without warnings. No filesystem behavior was changed.

The real-game paused-owner probe was rerun with the production menu and font fallbacks active:

| Execution mode | UI updates/second | Guest ticks/PC and idle present count unchanged | Lock/config/resize/save/load |
|---|---:|---|---|
| Single-core | 56.94 | Yes | Pass |
| Normal dual-core | 56.97 | Yes | Pass |
| Deterministic dual-core | 56.97 | Yes | Pass |

These are short feasibility samples, not sustained performance benchmarks. All runs preserved
full Qt key codes and releases, stopped presenting while inactive, resumed UI after focus regain,
stopped the core normally, cleared capture at teardown and reported zero alerts. State loading's
explicit restored-frame presentation is measured separately from idle guest advancement.

The excluded probes, logs, native backbuffer captures and isolated user profiles live under
ignored `build/big-picture-host/`. The virtual-controller probe uses the real `InputConfig`,
expression references, hotplug callbacks, capture queue and production `OnScreenUI` renderer.
It does not claim physical-controller validation. The real-game probe uses the existing local
test game and separate muted profiles; it does not modify the game or the user's Dolphin profile.
Temporary dependency links and the CMake probe hook are removed after validation.

## Review follow-up, 2026-10-25

Inactive navigation and preference bindings now release their own hotkey suppressions. Cleanup
walks the complete expression tree without evaluating functions or assignments, and preserves
suppressions owned by other bindings. Disabled polling also clears the Open Menu chord's
suppression and uses cached preference values. This prevents a held navigation chord from
leaving a gameplay key suppressed after closing or losing focus.

The existing FreeLook backend sample now also publishes accumulated relative motion to the
Big Picture channel before menu references are read. The companion channel is scoped to that
update; other readers retain their own accumulated motion. DInput's relative-mouse state uses
this explicit companion channel, without an additional hardware poll.

Eight review regression tests cover chord release on exit, nested/stateful expressions, shared
suppression ownership, disabled polling and preferences, a real menu reference bound to a
relative axis, independent reader accumulation, duplicate-channel consumption and capture-epoch
resets. The application and both full suites were rebuilt and passed after these fixes.

The excluded 720p D3D11 probe also passed through the production controller-interface update:
the menu binding and FreeLook both received the injected relative motion, exactly one device
sample was taken, and the companion channel was cleared afterward. The navigation, swap,
reconnect and close checks still passed with zero alerts. This uses a virtual relative axis;
physical DInput mouse testing remains part of the hardware matrix. The owner timing samples
above were recorded on 2026-10-24.

The general Configuration window now includes a Big Picture category. Its four Configure buttons
open the same cached binding/profile editors as the Options shortcut. Shared Menu Preferences
controls now live directly in this category, and the Controller 1 editor contains only navigation
bindings. The pane reloads preferences when shown or when Configuration regains focus, including
changes made through the in-game menu. Qt regression tests verify editor routing, existing-value
loading, persistence without changing bindings, and refresh on show/activation. The Windows
application was rebuilt and all 207 Qt tests passed after these UI changes.

## Trying the slice

1. In Config → Big Picture, select Configure for Controller 1, choose a device and map Open Menu.
   Map directions, Confirm, Back and page controls for a controller. Native keyboard navigation
   needs only the Open Menu binding; it uses arrows, Enter/keypad Enter, Escape and Page Up/Down
   afterward. Check that all four Configure buttons open the corresponding editor and that
   Options → Big Picture Input still opens the same editors.
2. Start a game, release the opening chord after opening the menu, and navigate between Menu
   Preferences and the root. Change each preference and check it in Config → Big Picture. Change
   the values there, reopen Configuration, and verify persistence and the in-game values. Check
   that Controller 1 no longer contains a Menu Preferences group.
3. Close while originally running and while originally paused. The implicit close preserves the
   host's existing pause-ownership rules. Check focus loss and controller reconnect while held.

## Remaining scope

Slice 4 wires the full in-game actions and save-state entry points. Slice 5 supplies the
standalone graphics owner, library entry and startup/CLI behavior. No default keyboard Open Menu binding
or standalone entry is advertised by this slice.

Linux/macOS, other graphics backends, physical device families, monitor switching, live NetPlay,
TV-distance readability and accessibility review remain platform/release checks. Native
GameCube-adapter navigation and real-Wii-Remote/Bluetooth-passthrough input are not provided by
the expression-backed menu bindings; those configurations still require explicit support or an
unavailable-feature indication before advertising controller-only menu operation.
