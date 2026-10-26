# Big Picture themes, icons and metadata — Windows results (2026-10-26)

Follow-up to visual refinement commit `8958524b8a`. The local PCSX2 reference remains
`E:/work/pcsx2` at `adca25d5995fbec3dbabbc061518d332a5ea935b`. Inspected its fullscreen theme
selection/palettes, library metadata, action iconography and checked choice controls in
`pcsx2/ImGui/FullscreenUI.cpp` and `ImGuiFullscreen.cpp`. Implementation and vector symbols
are independently authored; no PCSX2 code, fonts or artwork were transplanted.

## Appearance and settings

The shared settings catalog now includes `interface.big_picture_theme`, backed by the global
`General/BigPictureTheme` value in Dolphin.ini. Both the desktop Big Picture pane and fullscreen
Interface page consume the same typed choices. Changes are live; they do not rebuild the
graphics device or depend on opening a desktop settings window. The interface theme remains
global when editing per-game settings, and per-game theme writes are rejected.

The eleven choices are Dark, Light, AMOLED, Cobalt Sky, Grey Matter, Untouched Lagoon, Baby
Pastel, Pizza Brown, Scarlet Devil, Violet Angel and Dolphin Blue. The last adapts the reference's
blue family to Dolphin branding. Colors follow the reference's visual families, with independent
selection/header text colors and adjustments for readable small text. Unknown stored names
remain unchanged and unavailable in the selector; rendering falls back to Dark.

`Common/BigPicture/Theme.h` defines stable IDs and translatable labels. Renderer-owned palettes
stay in `VideoCommon/BigPicture/Style.h`; the graphics thread consumes the theme name through
the immutable host snapshot instead of reading live config. Theme changes retain the page,
selection, input capture and logical scroll. Diagnostic overlays retain their own ImGui style.

Current setting values now have checkmarks separate from navigation focus. Active category
and global/game tabs also have an underline so they remain distinguishable when a theme uses
similar header and selection colors.

## Icons, metadata and small displays

`VideoCommon/BigPicture/Icons` draws normalized symbols for menu actions, settings categories,
library controls, folders and files. Landing cards use larger symbols; rows/toolbars reserve
space for their icons and current-value markers. Symbols scale with the menu, inherit disabled
and selected colors, and introduce no font or asset dependency.

Library details now include translated platform, region, game ID, hours/minutes played, file
size, native file path and tags. Formatting happens on the library's Qt owner before publishing
an immutable snapshot. Compact row descriptions remain separate from the full preview text.
Retained snapshots keep their prior metadata when a scan or play-time update publishes new data.

At 150% menu scale with 15% safe-area margins, a normal portrait card can be taller than the
scrolling viewport. Artwork and landing symbols now shrink before the title/summary is clipped;
covers are omitted when there is too little space for a usable image,
and non-artwork cards also respect available height. A navigation regression checks that the
focused card can actually be revealed at both 720p and 4K.

## Save-alert investigation

Repeated native theme switching exposed a blocking `IOS_FS: Failed to rename temporary FST
file` alert. This initially looked like an OpenGL theme-publication stall: the setting changed
while a modal save error prevented the host from finishing and publishing its snapshot. The
existing filesystem tests also encountered the same rename alert.

The existing base config loader writes SYSCONF as part of saving settings. Windows readers
that omit delete sharing can temporarily prevent atomic file replacement. `File::Rename` now
retries only Windows sharing, lock and access-denied errors with at most 100 ms of retry delays.
Other errors return normally; permanent locks still fail without deleting either source or
destination. Dedicated Windows tests hold a reader open, exercise release during replacement,
and verify both files remain intact when the lock persists. Non-Windows rename behavior is
unchanged. This is a filesystem reliability fix, not an emulator execution/pause-policy change.

## Verification

- Release app, core tests and Qt tests build using the existing MSVC/Qt 6.8.3 dependencies.
  The final normal app contains no native probe startup hook.
- **1,662 core tests and 259 Qt tests pass**, with two existing optional preset-fixture skips.
  Core tests run through CTest in `out/Source/UnitTests`; Qt uses the Windows platform plugin.
- Core coverage includes typed theme validation, persistence/reset, unknown-value preservation,
  global-only policy, theme propagation across page builders, live focus/activation preservation,
  normal and summary text contrast, maximum-scale focus visibility and Windows lock handling.
- Qt coverage includes all desktop theme choices, bidirectional shared-service edits, fullscreen
  current-value markers and immutable translated library metadata.
- An excluded native app probe switched through all eleven themes, checked external config
  updates and unknown names, returned to desktop, reentered with the selected theme and exited
  with graphics drained. It passed on D3D12, Vulkan, D3D11 and OpenGL after the lock handling fix;
  OpenGL also passed two additional repeats. Profiles are isolated under
  `build/visual-refinement/theme-native-*`.
- The D3D11 render probe captures 236 normal-scale screens: all major page fixtures in Dark,
  Light and AMOLED at 720p, 1080p, 4K and ultrawide, plus representative 720p screens for the
  other eight themes. It also captures 34 Dark/Light fixtures at 150% scale and maximum safe area.
  Results are under `build/visual-refinement/parity-captures/` and `parity-stress-captures/`.
- Gettext extracts theme names, the shared descriptor/help, desktop Theme label and metadata
  labels. Formatting and diff whitespace checks pass.

These GPU captures are representative fixtures using production rendering and menu builders,
not equivalent screenshots of two running emulator UIs. Native side-by-side PCSX2 comparison,
user appearance review, physical controllers and other operating systems remain open gates.
Save-state thumbnails/completion hardening, richer game launch options, controller-family
prompts and expansion beyond the common settings catalog remain subsequent parity work.
