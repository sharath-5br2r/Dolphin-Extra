# Release Notes — Next Fork Release

## User-Visible Changes

### Big Picture

The initial screen now follows PCSX2's four-action layout: Game List, Start Game, Settings,
and Exit. It uses the same home icons, Roboto text and PromptFont keycaps, with Dolphin branding,
a local clock, and the signed-in RetroAchievements display name and avatar when available.
Start Game offers the controller file picker and installed console menus. Exit offers desktop
mode or application exit; Escape returns from child pages to the home screen.

Big Picture can now open without a running game through View → Big Picture, the startup
preference in Config → Big Picture, or the GUI `--big-picture` option. Normal desktop startup
remains the default. The library uses Dolphin's existing scanned games and covers, with grid/list
views, search, platform/region/tag filters, and title/play-time/file-size sorting. Selection and
view state survive boot, stop and rescans within the session.

Add Game Folder, recursive scanning, Refresh, Open Game and the in-game Change Disc action now
have controller-operated file/folder pickers. Text entry uses physical/system keyboards when
available, with controller keys as a fallback, and search can be cleared directly from the
library. Change Disc starts with matching games from the library; Browse Files starts beside
the running image or in a configured ROM folder. Library Options also
offers available GameCube IPLs and the installed Wii System Menu, fullscreen,
desktop return and exit. Big Picture keeps its presentation window and fullscreen state through
boot, stop and graphics handoffs. Stopping a game launched in this mode returns to the library. Disc
insertion requested while paused completes after Resume.

Starting in Big Picture no longer shows the desktop game grid before the renderer is ready.
The desktop window stays hidden behind separate/fullscreen Big Picture during game transitions.
Compatible boot/stop transitions also keep the native drawing surface; fresh surfaces receive
a black background before graphics initialization instead of showing uninitialized pixels.
Direct3D 12 releases its old swap-chain buffers at shutdown so the retained window can be
used again when launching or stopping a game.
List menus wrap consistently in both directions with keyboard and controller input.

Big Picture menus now share a fullscreen dark palette, separate title/summary typography,
rounded focus outlines and a fixed controller-prompt footer. The library list includes a
selected-game cover/details panel, with a direct grid/list switch in the toolbar. The pause
menu uses a narrow action column and running-game title/cover header. Save/load slots use
four-column cards, and settings retains category navigation with active category and
global/game context indicators. These layouts retain the menu scale and safe-area preferences.

Big Picture appearance can now be changed live from either its Interface settings or the
desktop Big Picture pane. Eleven theme families include Light, AMOLED and Dolphin Blue;
unknown stored theme names are preserved and render with the Dark fallback. Menus include
drawn action/category icons and checkmarks for current setting values. Library details show
platform, region, game ID, play time, size, path and tags. At large menu scales, artwork shrinks
to keep focused cards readable inside safe-area margins.

Windows file replacement now briefly retries transient sharing/access locks, preventing
temporary reader locks from immediately producing a blocking configuration/FST save alert.
Persistent failures still report failure and leave the original files intact.

Settings is a top-level Big Picture entry, separate from Library Options. It includes
interface/menu preferences, volume and audio backend,
graphics backend, internal resolution, aspect ratio, VSync, integer scaling and a folder-based
post-processing preset picker. Global and selected/running-game settings show their context,
inherited values, disabled reasons and apply timing. Reset to Default and Use Inherited Value
remain separate actions. Settings persist through Dolphin's existing configuration files;
changing the idle graphics backend rebuilds Big Picture while retaining the settings page.

The Controllers page selects physical input devices and loads existing user/stock profiles
for emulated GameCube controllers and Wii Remotes. Per-game profile selection stores a user
profile reference for the next game start without replacing the global mapping. Controller
changes require stopped emulation and are blocked during NetPlay and movie sessions. Full
mapping, controller-type setup and shader parameters remain available on the desktop.

### Post-Processing

1. **The Post-Processing Renderer setting has been removed.** There is no engine to pick any more: a preset runs through librashader whenever the active graphics backend has a librashader runtime *and* that runtime reports it can run on this system. That now covers D3D11, D3D12, Vulkan and OpenGL on Windows; Metal, Vulkan (MoltenVK) and OpenGL on macOS; and Vulkan on Android arm64-v8a. Dolphin uses its built-in post-processor everywhere else: the Software and Null backends, platforms with no vendored librashader binary (desktop Linux, Android x86_64), and any case where the library is present but unusable — a symbol that will not resolve, or an OpenGL context below librashader's floor (GLES, or desktop GL below GLSL 330). The fallback is automatic, and neither engine is user-selectable.

2. **Shader chains are no longer supported.** The post-processing shader field now accepts a single preset path instead of a semicolon-separated chain. Existing configurations holding a multi-preset chain will resolve to the first entry in the chain. This matches PCSX2's single-preset model and reflects what librashader was already doing silently on the librashader path.

3. **The preset picker is now a filterable tree.** Instead of a flat combo box with all 30,864 presets from the full libretro pack, the post-processing shader picker presents a hierarchical tree with a recursive filter box. Presets declaring `#pragma parameter` directives are now editable through a new *Parameters…* dialog with per-parameter sliders, spin boxes, and per-row *Reset* buttons. Edited parameter values are persisted per preset and survive restarts.

## Corrections to Previous Release

The `2606-fork-2` release notes claimed native `GenerateMipmaps` support for "Metal, OpenGL, D3D11 and D3D12". The correct list is **Metal, OpenGL, and D3D11**. D3D12 has no `GenerateMipmaps` implementation and was incorrectly included.
