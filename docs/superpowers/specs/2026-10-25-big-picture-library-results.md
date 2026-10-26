# Big Picture slice 6 — Library and controller-only setup

**Date:** 2026-10-25

**Status:** Complete on Windows; physical-controller and other-platform checks remain release gates

**Plan:** [Big Picture implementation plan](../plans/2026-10-24-big-picture-ui.md), slice 6

## Behavior

Big Picture now displays the existing Dolphin game catalog as a cover grid or list. Library
Options cycles title, descending play time and descending file size sorting, platform, region
and existing tag filters. Search matches Unicode case-folded title, game ID and path. Clear
Filters restores the complete view. Identical titles and multi-disc games retain distinct paths.
Selection, query, filters and view survive boot/stop and renderer recreation within the current
application session; these view preferences are not persisted across application restarts.

The empty library offers Add Game Folder and Refresh. The controller picker supports Home,
drive roots, parent navigation, typed paths and a shared recursive-scanning toggle. Choosing a
folder uses Dolphin's existing game-path settings and scanner. Library Options → Open Game uses
the same picker for supported disc, executable, WAD, replay and playlist paths. Search and path
entry use a native text field when a physical or system keyboard is available, including native
IME input. With no physical keyboard, the platform input panel is requested (or Windows OSK);
controller keys appear only if no system panel becomes available. Both editors offer Clear.
The library toolbar also offers Clear Search without changing platform/region/tag filters.
Cancel leaves the previously committed search unchanged. Long paths/drafts display their tail.

The in-game Change Disc action opens the full scanned disc library, independently of active
search/filters, and retains the existing pause. Matching game IDs and similar titles rank first,
preferring another disc over the booted image. Browse Files starts beside the running image,
then falls back to a configured ROM folder and finally Home. An empty disc library opens that
folder directly. It only lists supported disc image types; insertion uses the existing guarded
DVD operation and completes after Resume. NetPlay and other core restrictions are checked again
at execution. Available GameCube IPLs and an installed Wii System Menu appear in Library Options
and use the existing boot/graphics handoff. Existing desktop configuration paths remain available.

Boot/stop and idle renderer restarts retain the same presentation widget, native window,
fullscreen state and embedded stack selection. A replaceable native drawing child gives each
graphics owner the existing compatible surface. It is replaced after the previous owner drains
only when the backend or stereo surface requirements change. Windows native background erasing
initializes fresh surfaces to black; the parent's palette alone cannot initialize a child using
`WA_PaintOnScreen`. There is no branded intermediate window. Explicit desktop return still
releases the presentation window and reveals the desktop UI.

Startup with Big Picture enabled defers showing the main window until the embedded renderer
is selected, or keeps it hidden for a separate presentation. Separate/fullscreen Big Picture
also hides the desktop window throughout game and renderer handoffs; retaining the renderer
alone left the desktop available behind it. The shell is prepared before its first show.
List menus wrap both up and down, skipping disabled entries and revealing the focused row.

### PCSX2 transition comparison

The local PCSX2 fork was inspected at `adca25d5995fbec3dbabbc061518d332a5ea935b`.
[`VMManager::Initialize` and `Shutdown`](https://github.com/ilyalissoboi/pcsx2/blob/adca25d5995fbec3dbabbc061518d332a5ea935b/pcsx2/VMManager.cpp#L1579)
remember whether GS was already open for fullscreen UI. Boot reuses it; shutdown resets GS
without closing it when it was open before boot.
[`FullscreenUI::OnVMDestroyed`](https://github.com/ilyalissoboi/pcsx2/blob/adca25d5995fbec3dbabbc061518d332a5ea935b/pcsx2/ImGui/FullscreenUI.cpp#L435)
switches the existing UI back to its library/landing screen.
[`MainWindow::onVMStopped` and `acquireRenderWindow`](https://github.com/ilyalissoboi/pcsx2/blob/adca25d5995fbec3dbabbc061518d332a5ea935b/pcsx2-qt/MainWindow.cpp#L2603)
retain the display when present and return an existing compatible render window.

Dolphin preserves that presentation-window lifetime. Its existing separate UI/core graphics
owners still drain and rebuild device resources at handoff; device reuse is not introduced.
`RenderSurfaceHost` separates the stable Qt shell from the backend-facing child so the driver
workaround for native-surface recreation remains available. Input uses the stable presentation
widget, graphics uses the child, and Windows focus checks resolve the child's top-level ancestor.
As in PCSX2's compatible-window path, compatible handoffs now retain the drawing surface too.
Compatibility is checked again after game INIs load, before starting the next GPU owner, so a
per-game backend/stereo override still gets a new surface. WGL reuses an already matching pixel
format instead of trying to set it twice. A brief black interval during device initialization
remains possible; this is not PCSX2's persistent GS-device implementation.
The implementation is independently authored using Dolphin/Qt APIs.

These screens use the shared navigation toolkit, with toolbar/grid focus, shoulder paging,
path-based selection and safe Refresh focus when a selected game disappears. Full rescans retain
the committed view until the final model arrives. They do not briefly select another title while
the native model is being repopulated. Slice 9 completes the planned PCSX2 visual unification.

## Ownership and bounds

`BigPictureLibrary` adapts the existing Qt model on its owner thread into immutable, Qt-free
library snapshots. It does not add a second game scanner or cover downloader. Model changes
coalesce through a short timer. Desktop-only operation avoids rebuilding the extra catalog.
Graphics configuration comparison now restarts the standalone renderer only for relevant GFX,
backend or emulation-speed changes; adding a folder no longer interrupts the text keyboard.

Directory enumeration and cover decoding use a joined/cancelled CPU worker. There is one active
directory request and one replaceable next request; obsolete completions fail their revision
check. Listings retain at most 4,096 entries and examine at most 16,384, with an explanatory
message and typed-path alternative when truncated. File actions carry the listing revision.
Controller context checks reject text/picker operations from unrelated screens.

Encoded covers come from existing native metadata. Inputs are capped at 16 MiB, 8,192 pixels
per dimension and 16 million source pixels. Decode scales to at most 256 × 384 RGBA. At most
four cover jobs and 24 decoded images are retained. The graphics owner uploads at most two per
frame and retains at most 32 textures, protecting textures referenced by the current draw data.
Only visible cards request artwork; missing/corrupt images use placeholders. Catalog revisions
reject obsolete decode results. GPU resources remain owned and drained by the existing render
session. Offscreen controls still participate in navigation, but do not allocate custom drawing
geometry; this prevents large libraries from overflowing draw buffers.

## Validation

Windows Release Dolphin and both test binaries build with MSVC 19.51 and pinned Qt 6.8.3;
optional emusync remains disabled as in preceding slices.

- **1,634 core tests passed**, with two existing external-preset fixture tests skipped.
- **235 Qt tests passed**, using the bundled Windows platform plugin.
- New coverage checks immutable snapshots, path selection, Unicode text/erase, all filter
  states, canonical folder setup, stale/coalesced listings, cover bounds and stale results,
  full-rescan retention, renderer configuration filtering, nested setup, boot/disc policy,
  toolbar navigation, safe focus after removal and grid paging. Follow-up coverage checks
  direct search clearing without resetting other filters, native editor clearing/cancellation,
  physical/system-keyboard selection and unavailable-panel fallback, alternate-disc ranking,
  and configured/running-game folder defaults. Surface-host tests assert that replacement
  changes only the drawing child's native handle, never hides/reparents the presentation
  widget, leaves the embedded stack selected, and retains fullscreen.
- A 5,000-card navigation test restores and activates the final game while bounding generated
  vertices/indices. A native 5,008-game D3D11 stress capture observed about 4.2 ms average and
  7.2 ms maximum presentation work across nine frames, with no alerts or guest resources.
  This short sample is not a general performance guarantee.
- The real app passed empty-library → folder/path keyboard → native scan → search → keyboard
  selection/boot → pause → disc picker → resume/actual insertion → stop → next game → stop →
  list/sort → rescan retention → confirmed Exit. Separate and embedded game windows both passed.
  D3D11, D3D12, Vulkan and OpenGL resumed idle presentation after backend changes. Follow-up
  probes check native text entry and quick search clearing, the library-first disc picker,
  unchanged presentation-window object/native handle across boot, stop and all four backend
  changes, reuse of the drawing surface across compatible boot/stop handoffs, no presentation
  hide/reparent events, retained fullscreen, and continuous embedded
  render-widget selection. These invariants replace the earlier overlay-presence check, which
  did not establish that the window itself stayed visible.
- Follow-up probes install an application event filter before the main window is constructed.
  They check that its first show already selects the renderer, that the separate desktop stays
  hidden during boot/stop/backend changes, and that explicit desktop return restores it. Separate
  and embedded/fullscreen runs passed with toolbar preferences retained and no desktop exposure.
  A new ImGui test checks symmetric keyboard/gamepad looping across clipped and disabled rows.
  The backend matrix also exposed an OpenGL teardown use-after-free: clearing guest vertex
  formats now clears the current-format pointer before a subsequent UI-only session can use it.
- The native erase regression test failed against the prior implementation (white remained)
  and passes with explicit black erasing. Compatibility tests retain a native handle across
  repeated handoffs and reject changes in backend/stereo requirements. App probes now also
  open the View menu and trigger its actual Big Picture QAction, rather than only calling the
  entry method directly. Desktop-menu embedded and startup separate runs passed; a fullscreen
  desktop-menu run with a per-game Vulkan override replaced the drawing surface at boot and
  restored the library backend at stop. OpenGL boot/stop checks exercise repeated WGL contexts
  on the same native surface. The final OpenGL startup/fullscreen rerun exited cleanly without
  alerts; the final build excludes all probe code.
- A D3D12 follow-up reproduced `DXGI_ERROR_INVALID_CALL` (`0x887a0001`) on the first boot
  with a reused surface. Renderer destruction queued back-buffer releases after the final
  GPU wait, but context destruction discarded the pending queues without releasing their
  raw COM references. Context teardown now drains those queues before its heaps/device die.
  The D3D12 startup probe then passed two game boot/stop cycles on the same presentation and
  drawing handles, disc insertion, the backend matrix, desktop return/reentry and clean exit.
  Desktop-menu entry with embedded/fullscreen presentation passed the same D3D12 sequence.
- An excluded D3D11 probe captured grid, list, empty state, options, picker and text keyboard at
  720p, 1080p and 4K with synthetic cover images, long titles and Japanese folder text. Readback
  occurs before swap, on the graphics owner. It observed no guest framebuffer, no guest
  presents, no alerts and complete graphics release.

Probes and isolated muted profiles live under ignored `build/big-picture-host/`. Integration
tests disabled cover downloading and did not modify the user's normal profile. The automated
desktop cannot reliably provide Windows foreground focus, so app probes force the shared focus
flag and inject Qt keyboard events/queued commands. Physical-controller navigation, reconnect,
native focus, Linux/macOS and hardware device-loss cases remain manual release checks. Actual
IPL/System Menu boot also needs locally installed firmware/NAND. Core boot alerts and complete
state-operation completion reporting remain slice 10. System-keyboard launch/visibility and
hardware detection still need testing on controller-only systems. User testing confirmed game
launch/stop transitions, symmetric menu wrapping and the corrected D3D12 path. Search/keyboard
selection and disc-picker follow-up checks remain pending explicit confirmation.

The final test build excludes the temporary probe hook. Temporary dependency junctions are
removed after verification.

## Trying the slice

1. Open View → Big Picture. Check the existing library in grid and list views, then search,
   cycle filters/tags/sorts and clear them. Clear Search should retain the other filters.
   Try long/Unicode titles and absent cover art. A connected keyboard should open a native
   field, without controller keys; with no keyboard, check system input and controller fallback.
2. With a controller, use Add Game Folder, Locations, Parent Folder, Enter Path and Scan
   Subfolders. Choose a folder, refresh and verify the same games appear on the desktop.
3. Boot a game, open the menu, stop to the library and select another game. Confirm the search
   and selection survive stop and a full refresh. Boot/stop must retain the same window and
   fullscreen without a desktop flash or a branded transition window. Restart Dolphin with
   Start Dolphin in Big Picture enabled and check that the desktop grid never appears first.
   In the in-game menu, Up from the first enabled entry must select the last enabled entry,
   and Down from the last must return to the first, with the selection kept visible.
   Removing the selected file must not launch another game from the same focus position.
4. In a disc game, use Change Disc. Matching library games should appear first even if library
   filters hide them. Browse Files should open the running game's folder or configured ROM
   folder. Select another image and confirm that the game
   remains paused and that insertion completes after Resume. Check Back/Cancel throughout.
5. Try Open Game, native/system/controller text input, available IPL/System Menu entries,
   fullscreen and desktop return. Compare embedded/separate windows and real-controller
   focus/reconnect, including swapped confirm/back bindings.
