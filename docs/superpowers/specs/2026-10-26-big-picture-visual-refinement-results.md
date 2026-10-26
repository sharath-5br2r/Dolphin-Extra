# Big Picture visual refinement — Windows results (2026-10-26)

Implemented on top of the completed common-settings slice, `b83d6ebf34`, on
`codex/big-picture-visual-refinement`. Slice 9's appearance review and release matrix remain
open; this document records implementation and verification, not final visual-parity signoff.

## Local reference analysis

Inspected `E:/work/pcsx2` at `adca25d5995fbec3dbabbc061518d332a5ea935b`:

- `pcsx2/ImGui/FullscreenUI.cpp`: landing header/horizontal menu, library grid and list/details
  columns, pause title/cover and narrow action column, state cards and settings navigation.
- `pcsx2/ImGui/ImGuiFullscreen.h`: 1280 × 720 design coordinates, 22/14 px title/summary fonts,
  48 px navigation heading, 34 px prompt footer, row padding and rounded frames.
- `pcsx2/ImGui/ImGuiFullscreen.cpp`: Dark palette, selection/hover/focus distinction, fullscreen
  window composition and contextual footer behavior.

These are visual/behavioral references. No PCSX2 implementation, fonts, icons or assets were
transplanted. Dolphin retains GPL-2.0-or-later source headers and its existing font fallback
and cover-loading paths. No additional dependencies or font assets were introduced.

## Implementation

`VideoCommon/BigPicture/Style.h` owns the reference-space palette, font hierarchy, spacing,
row dimensions and rounding. `NavigationToolkit` applies scoped styles, so diagnostic overlays
retain their own fonts, DPI/color settings and ImGui style. Title text uses 22 reference pixels;
summaries and prompts use 14. Display scaling still comes from the existing menu-scale and
safe-area preferences, with a 1280 × 720 baseline and horizontal expansion on ultrawide displays.

All pages share the heading, scrolling content and fixed prompt strip. Prompts wrap within
the safe area, reserving their actual height. Descriptions and long help/status text scroll
with content instead of pushing the footer off screen. Toolbar labels wrap within equal-width
buttons. Grid cells have equal heights; two-line titles cannot cover summaries. Custom geometry
is still skipped for offscreen entries. Disabled controls remain excluded from activation.

- Library grid: portrait covers, neutral disc placeholders and separated rounded cards.
  A direct view toggle joins the existing settings/search/filter/setup actions.
- Library list: thumbnail rows with a selected-game cover/title/details column. That column
  scrolls long metadata and cannot take keyboard/controller navigation focus. Narrow layouts
  omit the extra column to preserve readable rows.
- Pause: a dimmed game frame, running-game title/cover header and a 500-reference-pixel action
  column. The host publishes the running disc path separately from the idle launch selection.
  Read-only cover requests are accepted only for that path on the pause screen; library
  mutations and cover requests during confirmations remain blocked.
- Settings/controllers: category navigation remains available on subpages, with separate active
  indicators for global/game context and category. Existing typed writes, inheritance,
  disabled reasons, apply policy and controller-profile operations are preserved.
- State browser: ten slot cards in four columns, with Refresh/Back in the toolbar. Empty and
  unreadable loads remain disabled, slot numbers stay one-based, and overwrite confirmations
  retain their Cancel-first behavior.
- Landing, pickers, keyboard and confirmation/error pages inherit the same controls. Dialogs
  use a shorter centered panel; long content remains scrollable. Pause-header text is clipped
  to its reserved band instead of painting over the action list.

Stable page/item IDs retain selection and logical scroll. Resize restoration waits for ImGui
to recalculate its content bounds before deciding whether the selected control needs revealing;
using the first resized frame's old bounds caused a four-reference-pixel scroll drift.

## Verification

Windows Release builds use MSVC 19.51 and the existing Qt 6.8.3 dependencies, with emusync
disabled as in preceding slices. The build is isolated under ignored `build/visual-refinement/`.

- `dolphin-emu`, `tests` and `qt-tests` compile. The normal app is rebuilt without native probe
  sources. Capture/native probes are temporary, excluded verification tools.
- All **1,655 core tests and 256 Qt tests pass**. Logs are `all-core.log` and `all-qt.log`;
  two optional external preset fixture tests retain their existing skips.
  CTest's core entry is under `out/Source/UnitTests`; the root build directory has no test
  entries. Qt tests run directly with the Windows platform plugin because the bundled Qt
  does not provide the offscreen plugin used by its CTest command on other platforms.
- Regression coverage includes keyboard/gamepad hierarchy and grid/list navigation, paging,
  Cancel-first dialogs, removed/disabled selections, DPI/OSD font independence, resize/scroll
  restoration, 5,000-entry geometry bounds, details-pane focus exclusion, full ImGui-style
  restoration, category/context switching and running-game cover request policy.
- The native app probe cold-started a real UI-only session, presented the library and settings
  pages, switched directly between categories/controllers, returned to the library and desktop,
  reentered through `MainWindow::EnterBigPicture`, and exited with graphics drained. It passed
  separately on D3D12, D3D11, Vulkan and OpenGL with isolated profiles. It forces presentation
  focus and queues host requests; it does not establish physical-controller or game-boot coverage.
- The D3D11 render probe produced 64 PNGs at 1280 × 720, 1920 × 1080, 3840 × 2160 and
  3440 × 1440. It calls the production toolkit and menu builders with representative value
  snapshots, using Segoe UI plus the existing Meiryo fallback. Captures cover landing, pause,
  save/load slots, confirmation/error, grid/list/empty library, settings/long translated help,
  controller profiles, picker/busy/error and keyboard. Theme is Dark, menu scale 100%, safe area 5%. Captures are under
  `build/visual-refinement/captures/`; these are actual GPU-rendered fixtures, not app screenshots.
- Formatting and `git diff --check` pass. New visible controls reuse existing translated strings.

## Review and remaining parity work

Review `library-grid-720.png`, `library-list-720.png`, `root-720.png`, `settings-720.png`,
`save-states-720.png`, `confirmation-720.png` and their higher-resolution counterparts.
The render fixtures include missing artwork, Japanese/Cyrillic text and disabled settings.
They supplement behavior tests; they do not demonstrate real-game artwork, controller-family
prompts or a pixel-for-pixel comparison with PCSX2.

Equivalent native PCSX2/Dolphin comparison captures and user appearance approval remain required
by the plan. The local reference build was inspected in source; an attempted hidden launch did
not provide a targetable comparison window. Do not count that as a completed visual comparison.

This initial pass implements the Dark reference palette. The
[appearance follow-up](2026-10-26-big-picture-appearance-parity-results.md) adds the other theme
families, menu icons and richer game metadata. Save-state thumbnails and the full reference
settings catalog remain parity work; they are not claimed to be blocked by Dolphin's core. State metadata/completion
hardening and optional thumbnail sidecars remain slice 10 work. PlayStation-specific actions
and settings are adapted to Dolphin's existing GameCube/Wii capabilities.

Physical input, real-game pause/boot/stop cycles, device loss, monitor/DPI changes, 150% menu
scale/maximum safe area visual review, and Linux/macOS execution remain manual release checks.
No emulator execution, pause-ownership, graphics-lifetime or input-capture design was changed.
