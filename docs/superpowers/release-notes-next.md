# Release Notes — Next Fork Release

## User-Visible Changes

### Big Picture

Big Picture can now open without a running game through View → Big Picture, the startup
preference in Config → Big Picture, or the GUI `--big-picture` option. Normal desktop startup
remains the default. The initial screen can play the selected/default game, open the desktop
game picker, adjust menu preferences, toggle fullscreen, return to desktop or exit Dolphin.
Stopping a game launched in this mode returns to Big Picture. A full scanned library and
controller file picker are planned for the next slice.

### Post-Processing

1. **The Post-Processing Renderer setting has been removed.** There is no engine to pick any more: a preset runs through librashader whenever the active graphics backend has a librashader runtime *and* that runtime reports it can run on this system. That now covers D3D11, D3D12, Vulkan and OpenGL on Windows; Metal, Vulkan (MoltenVK) and OpenGL on macOS; and Vulkan on Android arm64-v8a. Dolphin uses its built-in post-processor everywhere else: the Software and Null backends, platforms with no vendored librashader binary (desktop Linux, Android x86_64), and any case where the library is present but unusable — a symbol that will not resolve, or an OpenGL context below librashader's floor (GLES, or desktop GL below GLSL 330). The fallback is automatic, and neither engine is user-selectable.

2. **Shader chains are no longer supported.** The post-processing shader field now accepts a single preset path instead of a semicolon-separated chain. Existing configurations holding a multi-preset chain will resolve to the first entry in the chain. This matches PCSX2's single-preset model and reflects what librashader was already doing silently on the librashader path.

3. **The preset picker is now a filterable tree.** Instead of a flat combo box with all 30,864 presets from the full libretro pack, the post-processing shader picker presents a hierarchical tree with a recursive filter box. Presets declaring `#pragma parameter` directives are now editable through a new *Parameters…* dialog with per-parameter sliders, spin boxes, and per-row *Reset* buttons. Edited parameter values are persisted per preset and survive restarts.

## Corrections to Previous Release

The `2606-fork-2` release notes claimed native `GenerateMipmaps` support for "Metal, OpenGL, D3D11 and D3D12". The correct list is **Metal, OpenGL, and D3D11**. D3D12 has no `GenerateMipmaps` implementation and was incorrectly included.
