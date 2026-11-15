# Big Picture library layout — 2026-10-27

Refactored the game library around the supplied PCSX2 Game Grid screenshot (1634 × 977).
The local PCSX2 implementation was read for measurements and interaction behavior; the
Dolphin renderer is independently implemented in VideoCommon/BigPicture/LibraryView.cpp.

## Layout and behavior

- Full-width header and footer, compact back control, localized Game Grid/Game List title,
  and separate grid/list icons. Active view controls are idempotent; shortcuts toggle views.
- Centered four-column grid at the reference proportions, with responsive columns for
  ultrawide screens or larger interface scaling/overscan. Reference dimensions are a
  44-pixel header, 256-pixel column pitch, 236-pixel cards, 272-pixel image boxes and
  30-pixel row spacing. Titles are centered, single-line and UTF-8-safe ellipsized.
- Cover aspect comes from decoded image dimensions; images are contained without stretching.
  Cards remain unfilled apart from hover/navigation focus, using the reference's gray frame.
- The first enabled game receives initial focus. Selected paths survive view changes,
  resizing, nested settings/options, sorting and refresh. Removing the focused game returns
  to a safe header focus rather than silently targeting a different game for launch.
- List mode shares the header/footer and provides cover rows with a separate details pane.
  Details do not take navigation focus from the game list.
- Keyboard: F1 Change View, F2 Settings, F3 Library Options, F4 Search, F5 Refresh List;
  Enter starts the selected game and Escape returns Home. Page Up/Down page through games.
- Controller: configured Previous/Next Page controls change view/open Library Options;
  navigation, start and back use the existing logical controls and physical-button glyphs.
- Search, clear search, folders, refresh and settings are accessible through Library Options.
  Empty libraries expose Add Game Folder; empty filter results expose Clear Filters.
  The existing custom-cover/GameTDB retrieval pipeline remains the source of artwork.
- Cover decoding now bounds images to 512 × 768 and preserves smaller images at their native
  resolution. Decoding stays on the existing worker; GPU texture creation stays on the
  graphics owner. Off-screen cards do not request covers or allocate custom drawing geometry.
- The obsolete generic library details renderer was removed. Other menus retain their
  existing toolkit, and the normal Dolphin executable excludes temporary test hooks.

## Validation

- Release Dolphin, core/Qt tests and GPU visual fixture build successfully.
- Core tests: 1,675 passed; two optional external-preset tests skipped.
- Qt tests: 261 passed.
- Added checks for reference grid geometry, directional navigation, paging/shortcuts,
  initial focus, view and child-page restoration, sorting/resize/overscan reveal,
  safe selection removal, visible-only cover requests, aspect preservation, folder setup,
  idempotent grid/list selection, and native resolution for small cover images.
- Native probes passed on D3D12, Vulkan, D3D11 and OpenGL: scanned library, 384 × 544 cover
  uploads, keyboard and injected controller navigation, grid/list switching with retained
  selection, settings, options/folder setup, refresh, return Home and clean exit.
  Isolated synthetic DOL listings were scanned but never booted; guest lifecycle stayed idle.
- GPU fixtures cover grid/list/empty/filtered views at 720p, 1080p, 4K, ultrawide and the
  exact reference dimensions, with Dark/Light/AMOLED and additional list-theme captures.
  Extra fixtures cover maximum scaling/overscan and DirectInput DualSense hint glyphs.
  The reference-sized grid, list details, empty state and stressed Light layout were
  visually reviewed. Cover artwork in these fixtures is synthetic and is not shipped.
- Translation extraction, clang-format and whitespace checks passed.

Ignored evidence is under build/visual-refinement: grid-final-{core,qt}.log,
grid-native-{D3D12,Vulkan,D3D,OGL}/grid-native.log, grid-final-captures/,
grid-stress-captures/, grid-controller-captures/ and grid-final-delivery-build.log.

No emulator-core behavior was changed. Actual game booting and physical controller button
presses were not exercised by these native probes; navigation events were injected into the
real renderer and routed through the production host/controller/library implementation.
