# Fullscreen OSD / Big Picture UI — Follow-Up Project Scoping

**Date:** 2026-09-18
**Status:** historical scope; implementation tracked in the approved plan
**Depends on:** `2026-09-18-declarative-desktop-ui-design.md` slice 0 (the setting registry)
**Reference implementation:** PCSX2 (`~/work/pcsx2`), `pcsx2/ImGui/`

**2026-10-24 follow-up:** See the [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md)
for current repository findings, PCSX2/GooseStation comparisons, rendering-lifetime work and
settings-registry gaps. Implementation has started; see the
[slice 0 results](2026-10-24-big-picture-lifecycle-spike-results.md). The historical scope below
is retained.

This is a scoping document, not an implementation plan. Its purpose is to size the work, record
which dependency gaps are real, and identify the one decision in the desktop migration that
materially changes this project's cost. A plan is written when this project is approved to start.

## 1. Goal

Give Dolphin a gamepad-navigable fullscreen interface — game list, pause menu, save states and
settings — rendered over the emulated frame, comparable to PCSX2's FullscreenUI.

## 2. Starting position: better than a line-count comparison suggests

Dolphin already has the renderer-side infrastructure this project would otherwise have to build.

- **ImGui 1.92.2b is already vendored** at `Externals/imgui/imgui`.
- **`VideoCommon/OnScreenUI.cpp` (600 LOC) already initialises ImGui, owns input key mapping, and
  renders it into the emulated frame.** This is Dolphin's counterpart to PCSX2's
  `ImGuiManager.cpp` (1,497 LOC).
- **`VideoCommon/OnScreenDisplay.cpp` (208 LOC)** provides messages and toasts.
- `PerformanceMetrics`, `Statistics`, `NetPlayChatUI` and `NetPlayGolfUI` already draw ImGui
  overlays.
- `DolphinQt/Settings/OnScreenDisplayPane.cpp` (253 LOC) already exposes OSD settings.

## 3. Dependency gaps — most of them are not real

`OnScreenUI` is already on ImGui 1.92's **new dynamic font system**: it sets `style.FontSizeBase`
and rebuilds the font texture when the size changes, and loads a user-overridable `OSD_Font.ttf`
from the user or Sys resources directory.

That retires most of the assumed dependency gap. PCSX2 needs **freetype ≥2.10 + plutosvg +
plutovg** because it pre-bakes glyphs at fixed sizes; Dolphin does not, because 1.92 scales fonts
dynamically. None of those three externals are present in `Externals/`, and none appear to be
needed.

The one genuinely new asset is a **controller-glyph font** for button prompts. PCSX2 uses
promptfont (OFL-licensed). Whether Dolphin needs it, and whether ImGui 1.92's dynamic fonts hold
up at TV viewing distances, is the subject of the phase 1 spike rather than an assumption in this
document.

## 4. Gap decomposition

PCSX2's `pcsx2/ImGui/` is 19,244 LOC. Against Dolphin's existing code:

| PCSX2 file | LOC | Dolphin equivalent |
|---|---|---|
| `ImGuiManager.cpp` | 1,497 | **exists** — `OnScreenUI.cpp` (600) |
| `ImGuiOverlays.cpp` | 1,886 | **mostly exists** — `PerformanceMetrics` + `Statistics` + `OnScreenDisplay` |
| `ImGuiFullscreen.cpp` | 3,560 | **missing** — the nav-focus widget toolkit |
| `FullscreenUI.cpp` | 4,181 | **missing** — the screens |
| `FullscreenUI_Settings.cpp` | 6,879 | **missing** — the settings browser |
| `FullscreenUI_Internal.h`, `ImGuiFullscreen.h`, `ImGuiAnimated.h`, others | ~1,241 | mixed |

So the real work is three missing pieces, not nineteen thousand lines of it.

## 5. The cost driver is duplication, not rendering

6,879 of PCSX2's lines exist because FullscreenUI re-renders, in ImGui, the same settings its Qt
panes render. Built naively, Dolphin would end up with **three** settings UIs: the Qt `.ui` panes,
FullscreenUI, and the existing Android one.

This is why the desktop migration's slice 0 records every `Bind()` call into a registry keyed by
`Config::Location`, capturing the config location, type, range or choice list, layer, label and
help text — all of which already pass through `Bind()` as arguments. Phase 4 below drives the
settings browser from that registry instead of hand-authoring each setting.

That registry is roughly 50 LOC in slice 0. Adding it after the fact would mean revisiting all 194
bind sites once they have been written across 27 slices. It is the single decision in the desktop
project that changes this project's cost, which is why it is called out there rather than here.

## 6. Phases

1. **Spike.** Confirm the controller-glyph-font requirement and that ImGui 1.92's dynamic fonts
   are legible at TV distances. Cheapest probe that answers both; output is an answer, not code
   that is kept.
2. **Nav toolkit.** The `ImGuiFullscreen` equivalent: focus model, large-format lists, choice
   dialogs, and gamepad navigation driven through Dolphin's existing expression-based
   `ControlReference` rather than a new input path.
3. **Screens.** Game list, pause menu and save states, built on the phase 2 toolkit.
4. **Settings browser.** Driven by the slice-0 registry. This is what keeps the phase from being
   a 6,879-LOC re-authoring.

Phases 2–4 are each large enough to warrant their own spec and plan when this project starts.

## 7. Licensing boundary

Identical to the desktop project: PCSX2 is `GPL-3.0-or-later` and Dolphin is
`GPL-2.0-or-later`, so PCSX2 provides behavioural reference only. No PCSX2 source, asset or font
is copied. A controller-glyph font, if needed, is sourced independently under a licence compatible
with Dolphin's distribution.

## 8. Out of scope for this project

- The desktop Qt migration itself.
- Replacing or restyling the existing OSD message and performance overlays; they keep working as
  they do today.
- The Android UI, which has its own fullscreen interface.
