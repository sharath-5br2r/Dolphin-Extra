# Big Picture settings — 2026-10-27

The settings view follows the supplied PCSX2 reference: a fullscreen Back/title/category
header, section rules, full-width rows with separate help and right-aligned values or
switches, neutral focus frames, scrolling content and fixed control hints. Titles align
with the Back glyph. The header contains only Back and category tabs; the extra
settings-options gear is removed.
Switching to Controllers or pages without editable rows retains the category positions.

Global Settings opens Interface. Game Settings opens Display and is available from the
pause menu of a running or paused game. The global view has no game selector and never
uses the idle library selection as an edit target. The game view omits global-only
categories and settings, including physical controller-device assignment. Its controller
profiles remain game-specific references. The pause menu also offers Global Settings.
Forged context switches and per-game requests outside a running-game session are rejected.

## Native coverage

This Windows build exposes **188 typed settings**, expanded from 16, across Interface,
General, Display, Enhancements, Graphics Hacks, Advanced Graphics, Post-processing, OSD,
Audio, GameCube, Wii, Folders, Memory Card Sync, Achievements and Advanced, plus the
existing Controllers workflow. Categories with no settings are omitted. The catalog is
available before constructing desktop dialogs or initializing the graphics renderer.

`ApplicationSettings.inc` records each desktop control used as its metadata source;
`ApplicationSettings.cpp` handles combined controls and dynamic options. This includes
CPU engine/speed and memory overrides, graphics enhancements/hacks/dumping/stereo,
OSD, audio engines/latency/surround, console preferences, resource paths, sync preferences,
and RetroAchievements preferences. Backend adapters, anti-aliasing samples, CPU engines,
languages and WASAPI devices use available typed choices. Stored values that are currently
unavailable remain intact. CPU, console and other boot-sensitive settings retain idle
restrictions; graphics/audio capabilities and dependent controls are checked by the service.
Resource-directory and sync configuration changes explicitly require a Dolphin restart.
Interface edits issue the same refresh/cursor/window notifications as desktop controls.

Number editors provide bounded step changes, exact slider ticks and typed display-unit
entry. Text/path editors support physical keyboard Unicode input and a controller keyboard,
with clear, erase, done and cancel. Drafts do not modify settings until Done. Invalid values
stay in the editor with an explanation. Long descriptions, values and draft displays are
bounded so they cannot consume the entire viewport. Full stored values are preserved.
Settings content ends 16 reference pixels above the footer, plus 16 pixels of scrollable
trailing space provided by the scroll container. Focus reveal includes this padding, so
controller navigation cannot clip the final row. Detail explanations begin 16 reference
pixels below the category bar.
These distances scale with the interface.
Defaults and per-game inheritance remain accessible through F2, controller D-pad Right or
right-clicking the row. Category changes use keyboard Left/Right or controller shoulders.

Full application workflow parity is not yet complete. Controller remapping/calibration and
controller-type setup, hotkey assignment, shader parameters, credentials/login, memory-card
sync authorization, SD creation/conversion, custom RTC date/time and USB whitelist management
still need dedicated native pages. The desktop settings action remains available for these.
Desktop-only Qt style/theme choices remain in the desktop interface. No emulator behavior
was reimplemented or changed.

## Icons and state

Menus use the exact `fa-solid-900.ttf` from the local PCSX2 fork together with PromptFont's
system/controller symbols. Font Awesome's SIL OFL notice is bundled and attribution is in
`Data/Sys/Resources/BigPicture/README.md`. The matching font SHA-256 is
`8a2db53f7927abee48e52262090d99ced56a22d46752fba13c8f8782ce632d73`.
Each category has a distinct symbol, including an application window for Interface, a monitor
for Display, a cube for GameCube, a remote silhouette for Wii, a card for Memory Card Sync,
and separate wrench, sliders and warning symbols for graphics hacks, advanced graphics and
advanced settings. Glyphs are rasterized at the requested size and centered using their ink bounds. Reserved
codepoints keep ordinary fallback fonts from replacing them. No PCSX2 implementation code
was copied.

The Qt owner publishes translated values and requests; the renderer never owns config layers
or controller objects. Focus survives pending edits and graphics-owner replacements. A bounded
PresentationState cache retains only page/item IDs and logical scroll, clears when Settings
closes, and rejects late writes after closure. Typed toggles carry only the setting request,
avoiding the previous ambiguous settings-navigation request.

## Validation

- Release/native probe, core tests, Qt tests and the production-layout visual fixture build.
- Catalog expansion baseline: 1,693 core tests passed, with two optional external-preset
  tests skipped; 266 Qt tests passed. The subsequent icon/spacing refinements passed 54
  focused layout, category and navigation tests, including mouse options and scroll bounds.
- Regression checks cover scopes, active-game layers and inherited values, catalog defaults,
  static typed choices, backend/parent dependencies, number units/ranges, Unicode drafts,
  queued characters, cancel/commit, long help/values, fixed tab geometry and graphics-handoff
  focus retention.
- Native isolated-profile input flows passed with clean exits on D3D11, D3D12, Vulkan and
  OpenGL. They check themes, direct controller toggles, replacement
  graphics-owner focus, options/defaults, shoulder navigation, volume, generic numeric entry,
  queued physical text, path editing, Controllers and return Home. No game is booted.
- Final refinement evidence includes 240 renders of native menu data, including end-of-list
  Interface views, using current production icon assignments, layout and fonts at 720p,
  1080p, 1634 x 977,
  4K and ultrawide in Dark, Light and AMOLED.
  Fixtures and native hooks live only in ignored `build/visual-refinement`; normal builds
  contain no test hook.

Physical controller hardware and settings changes during actual emulation were not exercised
by the native probes; active-game layer behavior is covered by Qt tests.
