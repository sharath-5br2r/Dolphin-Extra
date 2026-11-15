# Big Picture pause view — 2026-10-27

Based on remote `origin/master` at `91cff310a0` (the repository's default branch), including
PR #23's newly merged global Big Picture settings, file picker, fonts and renderers.

The pause root follows the supplied reference: a dimmed retained game frame, game cover,
filename/ID/revision, local clock, session/total play time, achievement metadata, and a vertical
action column. It uses the current global Font Awesome/PromptFont icon mapping, typography,
neutral rounded selection treatment and theme palette. The footer stays 34 reference pixels
high regardless of overscan. At larger interface scales the action list scrolls independently.

Game Settings opens the active game's settings context. Global Settings is accessed from the
main Big Picture menu. Both use the current `SettingsView` renderer and typed settings service, including
the category icon header, section dividers, help text, right-aligned values, switches, options,
inheritance/reset behavior and game-only writes. Global-only controls stay out of game settings.
Change Disc opens the Big Picture library/file picker; pathless changes cannot invoke a desktop
dialog. Existing confirmation, Hardcore, NetPlay and pause-ownership rules remain in effect.

Save/load use a three-column landscape grid, reducing columns when scale/safe area requires it.
The header, footer, colors, icons and focus treatment follow the current global menus. Keyboard
and controller navigation skip unavailable loads, restore selection, scroll it into view, and
support F1/left shoulder to refresh. New saves capture a small game-frame preview without menu
or OSD pixels. Timestamp-matched `.preview` sidecars leave the savestate format unchanged;
missing, stale, truncated or oversized previews fall back to the shared image placeholder.
Preview loading occurs on the host; bounded image values cross to the graphics owner for upload.

RetroAchievements rich presence is copied under the existing manager lock, alongside achievement
and point totals. Full 32-bit ImGui Unicode plus installed symbol/outline emoji fallbacks fixes
supplementary characters displaying as replacement diamonds. Metadata is refreshed while the
pause menu is open and missing metadata creates no blank rows. Emoji use outline glyphs.

Open Pause Menu is configurable and defaults to Escape. Fresh default profiles leave Stop
unbound. Existing profiles gain the new binding once while preserving other mappings, subsequent
remapping and deliberate clearing. Opening capture takes priority over legacy Stop=Escape and
starts before the queued host request; held inputs require release before further navigation.
Nested Escape goes back one screen. Root Escape returns to the game.

Validation on Windows Release/MSVC with Qt 6.8.3 and optional emusync disabled:

- Dolphin, core tests and Qt tests build successfully against the updated remote base.
- Full core suite: 1,720 passed, two optional external-preset tests skipped (1,722 total).
- Full Qt suite: all 270 passed, including per-game writes/inheritance and font discovery.
- Renderer/menu/Unicode/preview regression subset: all 71 passed.
- D3D11 captures exercise the production OnScreenUI font loading, menu builders and renderers
  for pause, save/load, global/per-game settings, disc picker, Light theme, missing achievement
  metadata, and maximum scale/safe area. Emoji, variation-selector and Font Awesome glyph lookup
  checks pass. A synthetic widescreen image verifies preview upload and aspect preservation.
  Probe-only font checks run before finalization, so glyph baking precedes texture upload.
- Probe sources/captures/logs are ignored under `build/pause-view/`; the normal build configuration
  has no probe injection. Captures are GPU-rendered fixtures with synthetic metadata/backgrounds,
  not screenshots of a running game.

Live gameplay checks remain: Escape with a physical keyboard/controller, Metal Gear's live
RetroAchievements text, disc replacement during gameplay, and new-save preview capture/reload
on supported graphics backends. The optional preview path and its persistence are compiled;
GPU upload/aspect rendering and stale/corrupt sidecar handling are covered separately.

Follow-up testing feedback:

- Removed Global Settings from the pause root. Close Game retains the power symbol; Exit Dolphin
  now uses Font Awesome's exit-door/arrow icon.
- Slot cards inset preview, slot label and timestamp by 12 reference pixels on every edge.
- The state-save worker notifies listeners after the state and preview write attempt finishes.
  MainWindow queues a metadata read/publication on the Qt owner, updating the visible state
  browser without leaving it. The hook is disconnected before MainWindow teardown, and queued
  work rejects shutdown or stopped emulation.
- A running Dolphin instance locked the previous executable; the update is built as
  `Dolphin-PauseFix.exe` alongside it, without closing that session.

Follow-up validation: all 154 Big Picture core tests pass; the full core suite has 1,720
passes and two optional skips, and all 270 Qt tests pass. Production D3D11 captures at
1310 x 873 confirm the exit-door icon and padded slot contents. Live save completion
should be retested in the updated executable; the existing running process was left open.
