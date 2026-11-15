# Start Game layout — 2026-10-27

The Start Game page follows the supplied PCSX2 reference: large white artwork, centered
launch tiles, neutral rounded focus frames, a fullscreen header and a fixed hint footer.
Back is a small control at the left of the header, matching Dolphin's Game Grid page; it
is not a launch tile. The existing tile renderer is shared with Home.
Both the Start Game and library header titles use the active font line height to center
vertically alongside the back arrow, rather than fixed top offsets. GPU captures verify
alignment at different viewport sizes and with increased interface scale/safe area.

The file, optical-disc and microchip artwork comes from PCSX2's start-file.svg,
drive-cdrom.svg and start-bios.svg. Original SVG metadata and license provenance are kept
with the assets. Transparent 512 × 512 rasterizations use the existing linear sampler and
alpha-mipmap upload path, including the prior HDR color correction.

Dolphin's existing launch actions remain: controller file picker (Start File), the selected
game shortcut when present, each installed GameCube IPL region and the installed Wii
System Menu. Extra launch choices wrap to a second row and fit scale/safe-area preferences.
No optical-drive boot action was added: Dolphin has no corresponding frontend launch flow.
No global-state load hint is advertised because that operation is not supported here.

The file picker is a wide scrollable panel, with a title/folder icon and close control,
current-path row, parent-directory entry, directory/file icons and fixed keyboard/controller
hints. The Start Game tiles remain visible through the panel and cannot receive input.
Backspace and the configured Previous Page binding navigate to the parent (or Locations at
a root). Enter/Confirm selects a row; Escape/Back or the close control cancels. The path row
opens the existing path editor. Locations, disc-library browsing and folder-selection
operations remain available; Use This Folder precedes folder entries, and Scan Subfolders
shows its current state. Requests retain directory revisions and the existing host policy.

Focus/scroll are retained across child pages. Picker revision tracking is per page, so
opening text entry no longer resets a file selection. Returning from the picker restores
the launch tile, and viewport/scale changes reveal the selected file in long listings.

Validation:

- Release Dolphin, the visual fixture, core tests and Qt tests build successfully.
- Core: 1,682 passed; two optional external-preset tests skipped. Qt: 261 passed.
- Added interaction tests cover header Back geometry, horizontal tile navigation, modal
  focus isolation, retained selection, directory revision changes, parent shortcuts,
  long-list scroll restoration, scaling, installed-menu requests and physical button hints.
- Native probes passed the idle Start Game/file-picker/folder/cancel/header-Back flow on
  D3D11, D3D12, Vulkan and OpenGL, including clean exits in the final runs. Inputs were
  injected; no game or console menu was booted. The final probe allows asynchronous folder
  listings to settle before sending the next input. Earlier rapid runs included one missed
  folder activation and NVIDIA driver-unload exit exceptions; stable-wait reruns passed.
- The production builder/renderer fixture captured 60 keyboard and 60 DualSense-hint views
  across 720p, 1080p, 1634 × 977, 4K and ultrawide, in Dark/Light/AMOLED. Eight additional
  captures exercise maximum scale and overscan. Representative launch, picker, all-console
  and Light/scaled layouts were visually inspected.
- Translation extraction, clang-format and whitespace checks passed.
- The normal Dolphin target excludes all temporary native probe hooks. Ignored build
  evidence is under build/visual-refinement/start-*.

Physical controller presses, real game booting and optical media were not tested. This
change affects host UI only; emulator-core behavior is unchanged.
