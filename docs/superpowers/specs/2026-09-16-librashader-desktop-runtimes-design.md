# Librashader Desktop Runtimes and Single-Preset Shader UI — Design

**Date:** 2026-09-16
**Status:** approved (scope decisions recorded in §6; the §7 gate resolved by measurement 2026-09-18)
**Related:** [desktop post-processing parity](2026-09-16-desktop-postprocessing-parity-design.md),
[librashader Android post-processing](2026-08-22-librashader-android-postprocessing-design.md)
**Reference implementation:** PCSX2 (`~/work/pcsx2`), `pcsx2/GS/ShaderChain/` and `pcsx2-qt/Shader*Dialog.*`

## 1. Problem

Windows UAT of the `2606-fork-2` release produced three findings:

1. **D3D12 + a slang shader renders a black screen.**
2. **The chain-building UI is very hard to use.**
3. **crt-royale and its derivatives render very dark** (observed on Vulkan with the
   librashader renderer selected).

Finding 1 is a defect in the hand-rolled multipass executor's D3D12 path. Finding 2 is a
design problem — the UI was ported from Android without rethinking it for a 30,864-preset
library. Finding 3 is resolved by measurement: no Dolphin defect (§7).

## 2. What is already established

### 2.1 The D3D12 black screen (finding 1) — root cause confirmed

Post-processing draws are *utility* draws, and
`DX12Context::CreateUtilityRootSignature` (`DX12Context.cpp:394-409`) declares only **8**
SRVs and 8 samplers, while `CreateGXRootSignature` (`:352-356`) declares
`VideoCommon::MAX_PIXEL_SHADER_SAMPLERS` = **16**. The slang translator's ceiling
(`SlangTranslator.cpp:20-26`) was raised to 16 to match Vulkan's
`NUM_UTILITY_PIXEL_SAMPLERS`, so a pass declaring 9 samplers — crt-royale's mask-apply pass
does — produces a pipeline D3D12 cannot create.

`MultipassPostProcessing.cpp:628` then swallows the failure:

```cpp
if (!pass.pipeline) continue;
```

A dropped *final* pass writes nothing to the backbuffer, which is the black screen. The
failure is silent: no log line names the pass or the reason.

### 2.2 The chain UI (finding 2) — design, not a bug

`PostProcessingChainDialog` (127 lines) offers a flat list plus two buttons that **both
close the dialog**, so building an N-preset chain takes N round trips, and the chain itself
is a read-only `QLabel`. Upstream of it, `EnhancementsWidget` presents every discovered
preset in one flat `QComboBox` (`EnhancementsWidget.cpp:367-403`) — 30,864 entries in the
full libretro pack.

PCSX2's equivalent is a tree with a filter box (`ShaderPresetPickerDialog`, 135 lines:
`QStandardItemModel` + `QSortFilterProxyModel` with `setRecursiveFilteringEnabled(true)` and
`setFilterRole(ROLE_PATH)`; folder items non-selectable; the filter expands all and an empty
filter collapses all), plus a separate parameter editor (`ShaderParametersDialog`, 359
lines).

**PCSX2 is single-preset.** Its settings tab exposes one read-only preset line edit with
*Browse…*, *Clear*, *Parameters…*, *Favorites…* — there is no chain editor to copy.

### 2.3 Chains are already a lie on the librashader path

`LibrashaderPostProcessing.cpp:78-80` truncates silently:

> librashader accepts a single preset, so only the first entry of a ';'-separated chain is used.

So a user who builds a 3-preset chain and selects the librashader renderer gets preset 1 and
no warning. This live defect is what drove the decision to drop chains rather than extend
them.

### 2.4 The vendored library is Vulkan-only, but its preset API is complete

`strings` on `Externals/librashader/lib/windows-x64/librashader.dll` yields **every**
runtime-independent symbol — `libra_preset_create_with_options`, `libra_preset_ctx_*`,
`libra_preset_get_runtime_params`, `libra_preset_free_runtime_params` — and **only** the
`libra_vk_*` runtime. It was built `--no-default-features --features runtime-vulkan`
(`Externals/librashader/README.md`).

Two consequences that shape the task order:

- The **parameters UI needs no rebuild.** Parameter enumeration uses only
  runtime-independent symbols, which are present today.
- The **per-backend runtimes need a rebuild** of the Windows DLL and the macOS dylib.

### 2.5 The D3D11 and D3D12 runtimes work on the target GPU

A librashader CLI built from source on the Windows UAT host ran crt-royale successfully
through `-r d3d11` and `-r d3d12`, agreeing with each other to within 0.1% on a flat-field
photometric probe. The upstream source tree is already checked out on that machine at
`C:\src\librashader`, on the `librashader-cache-v0.12.0` tag with a working
`vcvars64` + direct-`cargo.exe` recipe (`C:\src\build-librashader.bat`). Option C's rebuild
needs no new software on that host.

### 2.6 The D3D12 runtime needs no `dxcompiler.dll` if built `-static`

`librashader-runtime-d3d12/src/util.rs:233-252` selects its DXC entry point by feature:

```rust
#[cfg(all(feature = "static", target_arch = "x86_64"))]
pub unsafe fn DxcCreateInstance<T>(...)  // mach_dxcompiler_rs, linked in

#[cfg(not(all(feature = "static", target_arch = "x86_64")))]
pub use windows::Win32::Graphics::Direct3D::Dxc::DxcCreateInstance;  // needs dxcompiler.dll
```

`librashader-capi` exposes this as `runtime-d3d12-static`. Building with it links DXC
statically, so **no `dxcompiler.dll` has to be packaged**. (This corrects an earlier
assumption in this investigation that the DLL was mandatory; PCSX2 ships no `dxcompiler.dll`
either.)

Feature sets for the rebuild:

| Target | `--features` |
|---|---|
| `x86_64-pc-windows-msvc` | `runtime-vulkan,runtime-d3d11,runtime-d3d12-static,runtime-opengl` |
| `aarch64-apple-darwin` | `runtime-vulkan,runtime-metal,runtime-opengl` |
| `aarch64-linux-android` | `runtime-vulkan` (unchanged) |

## 3. Approved approach

**Drive librashader's native runtime per backend; retire the hand-rolled path on desktop.**
The engine of record on Windows and macOS becomes librashader for every graphics backend
(D3D11, D3D12, Vulkan, Metal, OpenGL), so there is one code path to make correct instead of
five backend-specific translator/executor paths.

This subsumes finding 1: D3D12 gets a real runtime rather than a translator whose output
D3D12's utility root signature cannot accept.

It does **not** subsume finding 3 — the darkening was observed on the librashader path, so
it lives inside the path this makes exclusive. See §7.

## 4. Architecture

### 4.1 The loader: stop using `librashader_ld.h`

`librashader_ld.h` declares one `libra_instance_t` whose members are gated by
`LIBRA_RUNTIME_*` macros, and its `librashader_load_instance()` is `static inline`. Two
translation units including it with different macro sets get two incompatible structs under
one name. It also forces a single TU to include `vulkan.h`, `d3d11.h`, `d3d12.h`, the GL
headers *and* the Metal headers together.

PCSX2 solves this by not using the `_ld` header at all. Adopt its shape
(`pcsx2/GS/ShaderChain/LibrashaderLoader.h`):

- A `VideoCommon::Librashader` module includes plain `librashader.h` (which supplies the
  `PFN_libra_*` typedefs) and owns the `dlopen`/`LoadLibraryW`, the availability probe with a
  human-readable failure reason, error formatting, and a `CommonFunctions` struct of the
  runtime-independent entry points.
- It exposes `void* GetSymbol(const char* name)` so each backend TU resolves only its own
  `libra_<api>_*` symbols, with only its own `LIBRA_RUNTIME_*` macro defined and only its own
  API headers included.

This also removes the `_LIBRASHADER_LOAD` patch that `Externals/librashader/README.md`
currently documents against the vendored `librashader_ld.h`.

### 4.2 What moves into VideoCommon

Reading `LibrashaderPostProcessing.cpp` (523 lines), the Vulkan-specific surface is small.
Backend-agnostic and hoisted to a `VideoCommon::LibrashaderPostProcessing` base implementing
`IPostProcessor`:

| Currently | Depends on |
|---|---|
| `ResolvePresetPath` (`:78`) | `File::` only |
| `BuildPassthroughPipeline` (`:198`) | `g_gfx` only |
| `BuildDownscalePipeline` (`:244`) | `g_gfx` only |
| `EnsureOutputTarget` (`:375`) | `g_gfx` only |
| `DownscaleToNativeSource` (`:327`) | `g_gfx`, plus a Vulkan barrier tail |
| `BlitFromTexture` orchestration (`:392`) | `g_gfx`, plus a runtime adapter |

The per-backend interface (`VideoCommon::LibrashaderRuntime`) defines six pure virtual methods
plus one optional method with a default no-op body:

1. **`IsSupported()`** — true when every `libra_<api>_*` symbol this runtime needs resolved.
2. **`CreateChain(preset)`** — builds a chain from a `libra_shader_preset_t` with the backend's
   device struct and options.
3. **`DestroyChain()`** — frees the chain, if there is one. Must be idempotent.
4. **`HasChain()`** — true when a chain is live.
5. **`RunFrame(source, target, frame_count)`** — records the whole chain, reading `source` and
   writing `target`. Must transition images and reconcile Dolphin's own tracking of anything
   librashader touched. Vulkan reconciles the target's image layout via `OverrideImageLayout`.
   D3D11 and D3D12 must invalidate cached state and re-bind descriptor heaps, per PCSX2's
   `GSDevice12.cpp:5015-5061`.
6. **`SetParameter(name, value)`** — pushes a `#pragma parameter` override into the live chain.

The seventh method, **`DiscardPendingTargetClear()`**, has a default empty body and is
overridden only by backends with deferred clears. It is called before `RunFrame()` when the
chain writes straight into the backbuffer, so the backend can drop the clear: the chain's final
pass covers every pixel.

### 4.3 Renderer selection

The user-facing "Post-Processing Renderer" choice goes away. The rule becomes: **use
librashader when this backend has a runtime and that runtime says it can run here; use the
built-in multipass executor otherwise.** That satisfies "librashader only on desktop" without
platform `#ifdef`s.

As implemented in `AbstractGfx::CreatePostProcessor()` (`AbstractGfx.cpp:205-212`) the test is
`runtime && runtime->IsSupported()`, and each half excludes a different set of cases:

- **`runtime`** comes from the virtual `CreateLibrashaderRuntime()`, whose base implementation
  returns `nullptr` (`AbstractGfx.cpp:200-203`). A backend that does not override it — Software
  and Null — therefore gets the built-in executor *even where the library loaded and is
  perfectly usable*. The library is not consulted at all in that branch.
- **`IsSupported()`** is where library availability is actually checked, per backend, together
  with symbol resolution: every implementation is `GetAvailability().available &&
  Functions().Complete()`, so a library that loaded but is missing a `libra_<api>_*` symbol this
  runtime needs degrades to the executor rather than to a black screen
  (`LibrashaderRuntime.h:31-33`). OpenGL adds device conditions to the same predicate — GLSL
  330 or better, not GLES, and `bSupportsTextureStorage` — because librashader's GL runtime
  cannot compile below 330 and the images it is handed are allocated with `glTexStorage2D`
  (`OGLLibrashaderRuntime.cpp:164-185`).

An earlier draft of this section stated the rule as "use librashader when the library loaded",
which is the availability half alone. That is wrong in both directions: it implies Software and
Null would run librashader on a desktop host, and it hides the fact that a *loaded* library
still yields the built-in executor on an incomplete symbol set or an OpenGL 2.1 / GLES context.

So the executor stays reachable in five distinct situations, not one: no vendored binary
(Android x86_64, documented in `Externals/librashader/README.md`, and desktop Linux); the
Software and Null backends anywhere; a library that fails to load for any local reason; a
library missing a needed symbol; and an OpenGL context below librashader's floor.

`MultipassPostProcessing` therefore stays in the tree, and it is not dead code — it is the
fallback. The extent of its unit coverage should not be overstated, though: no test
instantiates it, because it needs a GPU. What the 20 targets under
`Source/UnitTests/VideoCommon/PostProcessing/` cover is the CPU-side machinery it drives —
preset parsing, pass sizing, the pass graph, mip generation, slang translation and
compilation — several pieces of which the librashader path uses too.

### 4.4 Single preset

`GFX_ENHANCE_POST_SHADER` stops being a `;`-separated chain spec and becomes one preset
path. `ShaderChainSpec` (`AppendToChainSpec`, `DescribeChainSpec`, `CHAIN_SEPARATOR`) and
`PostProcessingChainDialog` are removed along with their tests. Existing configs holding a
chain resolve to their first entry — which is what the librashader path already did
silently, now without the silence.

### 4.5 The UI

Two dialogs ported from PCSX2, matching its layout and behaviour as closely as Dolphin's
settings plumbing allows:

- **`ShaderPresetPickerDialog`** — tree of folders and presets built from the discovered
  preset list, filter box with recursive filtering, non-selectable folder items,
  double-click to accept, OK disabled until a preset is current.
- **`ShaderParametersDialog`** — one row per `#pragma parameter` (label, slider, spin box,
  per-row Reset), *Reset All*, and rows hidden behind a status label when the preset has no
  parameters or fails to load.

An earlier draft of this section listed "a debounced write" among the dialog's features. It has
none, and should not: PCSX2 debounces because its write goes through
`Host::CommitBaseSettingChanges()`, which writes the INI, so a slider drag would thrash the
disk. Dolphin's write does not. `OnValueEdited` and `OnResetAllClicked` both call
`SaveOverrides()` immediately (`ShaderParametersDialog.cpp:294-320`), and that lands in
`Config::Layer::Set` — an assignment into the layer's in-memory map plus a dirty flag. The INI
is written later, by the `Config::Save()` that runs when the settings window closes
(`SettingsWindow.cpp:216`). There is nothing here to debounce.

The value reaches a running game by a different route again, not by the settings write: `Save`
bumps a generation counter, and the video thread compares it once per frame in
`BlitFromTexture` (`LibrashaderPostProcessing.cpp:414-420`), rebuilding the parameter set when
it has moved. That push sends *every* parameter, not only the stored ones, because a filter
chain retains the last value it was given, so a reset has to state the default explicitly.

The `EnhancementsWidget` post-processing row becomes PCSX2's shape: a read-only preset field
plus *Browse…*, *Clear*, *Parameters…*, keeping this fork's existing *Download…* menu.

Parameter values are enumerated through `libra_preset_get_runtime_params` — available in the
current vendored binaries — and persisted per preset, then pushed into the live chain with
`libra_<api>_filter_chain_set_param`.

## 5. Files

**New:** `VideoCommon/PostProcessing/LibrashaderLoader.{h,cpp}`,
`LibrashaderPostProcessing.{h,cpp}` (VideoCommon), `LibrashaderParameters.{h,cpp}`;
per-backend runtime adapters under `VideoBackends/{D3D,D3D12,Metal,OGL,Vulkan}/`;
`DolphinQt/Config/Graphics/ShaderPresetPickerDialog.{h,cpp}`,
`ShaderParametersDialog.{h,cpp}`.

**Modified:** `AbstractGfx.{h,cpp}` (selection rule), each backend's `*Gfx.cpp`,
`EnhancementsWidget.{h,cpp}`, `DX12Context.cpp` (§2.1),
`MultipassPostProcessing.cpp:628` (§2.1), `Externals/librashader/README.md` (rebuild
provenance and the removal of the `_LIBRASHADER_LOAD` patch), the CMake packaging for the
rebuilt binaries.

**Deleted:** `PostProcessingChainDialog.{h,cpp}`, `ShaderChainSpec.{h,cpp}`,
`ShaderChainSpecTest.cpp`.

## 6. Scope decisions

Recorded from this session's review:

- **Approach C** — librashader only on desktop, native runtime per backend. (Not B, which
  kept both engines.)
- **Drop chains** — single preset, exactly as PCSX2. (Not: extend the chain editor, and not:
  keep chains on the built-in path only.)
- **Port the picker and the parameters dialog.** Explicitly *not* ported: PCSX2's
  *Favorites…* dialog and its preset-cycling hotkeys, neither of which Dolphin has an
  equivalent for.
- **Shader parameters are global, not per game.** An earlier draft of this list bundled
  PCSX2's per-game settings layer in with the two items above, as something "Dolphin has no
  equivalent for". That premise is false: Dolphin has `Config::LayerType::LocalGame`, built by
  `GenerateLocalGameConfigLoader` (`GameConfigLoader.cpp:334`), and the graphics pane already
  writes to it — `EnhancementsWidget` holds the pane's layer and reads or writes through it
  precisely so a per-game page does not disturb the global value
  (`EnhancementsWidget.cpp:52,95-115`). The preset path itself is per-game for that reason,
  since it is an ordinary `Config::Info` on a graphics page.
  Parameter *values* are not, and this is a real limitation rather than a platform constraint:
  `LibrashaderParameters::Save` writes the base layer unconditionally, so editing a parameter
  from a game's own graphics page changes it for every game using that preset. The dialog says
  so out loud when it was opened from game properties (`ShaderParametersDialog.cpp:76-86`)
  rather than letting the user discover it afterwards. Making them per-game means keying the
  stored string by game ID or routing the write through the pane's layer; neither is done here.
- **Match PCSX2's layout as closely as possible** rather than inventing a Dolphin-native
  arrangement.

Out of scope: the librashader D3D9 runtime (Dolphin has no D3D9 backend); Linux and macOS
Intel librashader binaries; the R50 `#endif` preprocessor bug in UBO-member extraction
(~956 of 2987 presets), which belongs to the built-in translator and is tracked separately.

## 7. Resolved: the crt-royale darkening

This section left three candidates open, all of which needed pixels from a running Dolphin:
**(1)** there is no defect and crt-royale is simply this dark; **(2)** the chain's input is already
dark, so the defect is upstream in the XFB or the downscale; **(3)** the chain's output is mishandled
downstream, in the passthrough blit or the backbuffer handling.

The answer is **(1): no Dolphin defect.** Measured 2026-09-18 on the UAT host against real GameCube
content. crt-royale's brightness loss is the shader's own, and Dolphin hands the chain's output to the
screen intact: on one static frame the on-screen picture is **99.0%** of the chain's own output image.
UAT finding 3 closes on this measurement.

Refuted before that measurement, each by evidence, and none of them resurrected by it:

| Theory | Refuted by |
|---|---|
| Dynamic rendering path differs | Both branches bind the same image view and `render_pass_format` |
| Library version differs from PCSX2's | Both are 0.12.0, built on the same machine |
| Our box-downscale loses energy | `sum` over n×n then `* (1.0/n²)` — verified correct |
| Input-resolution policy | PCSX2's regime measures 70.8%, ours 68% |
| 10-bit swapchain format | librashader maps `A2B10G10R10_UNORM_PACK32` correctly (`librashader-common/src/vk.rs:18-19,56-57`). Right about the renderer — but the 10-bit swapchain is exactly what broke the *instrument*, see §7.2 |
| HDR/color-space frame options | Both Dolphin and PCSX2 pass `nullptr` frame options |

The earlier numbers here came from librashader's CLI on a **flat sRGB-128 field**, not from a running
Dolphin — the UAT host had no GameCube/Wii images at the time:

| runtime | source | output | mean vs input |
|---|---|---|---|
| vulkan | 640×528 | 640×528 | 110.9% |
| vulkan | 1920×1080 | 1920×1080 | 70.8% |
| d3d11 | 640×528 | 1920×1080 | 68.1% |
| d3d12 | 640×528 | 1920×1080 | 68.0% |

That table is kept for provenance, but its ~68-71% is **not a criterion**. A phosphor-mask shader's
loss depends on both the output resolution and the picture: the same table already spans 68% to 111%
purely by changing resolution, and the measurement below spans 86% to 56% purely by changing which
frame it lands on. Candidate 1 was originally phrased as "output/input ≈ 68-71%"; its substance
— no Dolphin defect — is what was established, by a different and stronger test than a band.

### 7.1 What was measured

Game images arrived on the UAT host (`F:\games\emulated\gc`), so the chain dump could be pointed at
real content. All figures are mean channel value over the region, 0-255, via
`Tools/chain-dump-mean.py`. The frame is the static
Metroid Prime (GM8E01) title screen, dump armed at post-processed frame 2100 (~35 s in, at the rig's
measured 58-59 fps). The chain input dump is **bit-identical across runs and across backends** — sha1
`34a584f20d1634c0c33992e1f726c532a9528b17` on both Vulkan and D3D12 — which is what makes these rows
comparable. Chain input is 640×448; chain output and the window's render rect are both 640×477, so the
screen column is `--crop 8,32,640,477` of the 656×519 window capture.

| backend | executor | preset | chain input | chain output | on screen | output/input | screen/input |
|---|---|---|---|---|---|---|---|
| Vulkan | librashader | crt-royale | 63.292 | 54.760 | 54.203 | **86.5%** | 85.6% |
| Vulkan | librashader | `nearest` (control) | 63.292 | 63.271 | 62.200 | **100.0%** | 98.3% |
| D3D12 | librashader | crt-royale | 63.292 | — (§7.2) | 52.710 | — | 83.3% |
| D3D12 | librashader | crt-geom | — | — | 48.240 | — | 76.2% |
| D3D12 | built-in | crt-geom | — | — | 46.272 | — | 73.1% |

Three things this establishes:

- **The chain's input is not dark** (rules out candidate 2). `nearest.slangp` is `stock.slang`, a pure
  passthrough; its chain output is 100.0% of its input and its on-screen picture is 98.3% of it — the
  residual being the 448→477 letterbox rows the crop includes.
- **Nothing downstream loses light** (rules out candidate 3). For crt-royale the screen is 99.0% of the
  chain's own output image. The passthrough blit and backbuffer handling that Task 4 restructured are
  faithful.
- **librashader is not darker than Dolphin's built-in executor.** On the same preset and backend it is
  *brighter* — 76.2% against 73.1% — which is the opposite of the direction UAT finding 3 assumed.

The loss is strongly content-dependent, so one frame is not the answer. On the dark Retro Studios
logo frame (frame 1200) the same Vulkan + crt-royale chain measures input 19.697 → output 11.015, i.e.
**55.9%**. A mask-and-scanline simulation costs proportionally more on a dark picture, which has less
headroom for its bloom to recover.

Visual check, not only ratios: every capture above was inspected. All show the Metroid Prime title
screen at normal brightness with the preset's characteristic look — crt-royale's fine phosphor triads,
crt-geom's barrel curvature and vignette. None is a black or near-black screen.

Note for anyone repeating this: **Dolphin's own screenshot will not show the shader.**
`Present.cpp:322` hands the frame dumper `m_xfb_entry->texture`, which is pre-post-processing. An
OS-level capture is required.

### 7.2 The instrument had to be fixed twice first, and O10 is diagnosed

Neither defect below is in Dolphin's rendering; both are in the debug dump added early in this branch.
Both are recorded because either one silently fabricates this section's numbers.

1. **The dump fired on the black boot screen.** The one-shot budget spends itself on the first frame
   the chain runs, which on a fresh boot is the console's black screen, so the first pair came back
   bit-exact zero — mean 0.000, max byte 0. Fixed in `bc6e7e1ccd` by adding
   `LibrashaderDumpChainDelayFrames` (default 0, deliberately not in the UI), counting only frames
   seen while the flag is set.
2. **The chain output dump is not RGBA8, and reading it as RGBA8 fabricates a plausible number.** The
   chain output target inherits the backbuffer's format — `BlitFromTexture` passes
   `framebuffer->GetColorFormat()` to `EnsureOutputTarget`
   (`LibrashaderPostProcessing.cpp:474`), which creates the target with it
   (`:369`) — and this host's swapchain is 10-bit. `AbstractTexture::Save` builds an **RGBA8** readback staging
   texture regardless of the source format, copies into it, and hands the bytes to the PNG encoder
   unconverted — so the file is a valid PNG full of misread `A2B10G10R10` words. It does not look
   broken. Read that way, the passthrough control measured **160.0%** of its own input, and crt-royale
   appeared to be 1.72× brighter than the screen, which is what an earlier pass of this task
   misdiagnosed as a downstream blit defect. The giveaway is alpha: an opaque 10-bit image read as
   RGBA8 has alpha in exactly **192..255**, because a 2-bit alpha of 3 occupies the top two bits of
   the fourth byte. `chain-dump-mean.py --rgb10a2-output` unpacks it; unpacked, the passthrough
   reproduces its input to within 0.05/255 with alpha == 3 everywhere.

**The same defect takes Dolphin down on D3D12**, which is almost certainly O10 — the never-diagnosed
crash where setting `LibrashaderDumpChainImages` killed Dolphin shortly after the first dump on D3D11.

What was found on 2026-09-18, with evidence kept apart from inference:

- **Observed.** With the dump armed on D3D12, the **input** dump line is written (that texture really
  is RGBA8) and the log ends there — no output-dump line, and no window by the next capture 14 s
  later. The byte-identical run without the dump flag survived all three captures and logged no error.
  So the fatal step is the **output** dump's readback: not rendering, and not the input dump.
- **Observed.** O10 was not re-run on D3D11 here, but the log kept from the run that first hit it
  (`E:\work\log-d3d11-dump.txt` on the UAT host) shows the **same signature** — an input-dump line and
  no output-dump line. On the evidence available the two are one defect rather than two.
- **Inferred, not verified at the API level.** The cause is the RGB10A2 staging copy described in
  item 2 above: D3D's `CopyTextureRegion` requires copy-compatible formats, while Vulkan's
  image-to-buffer copy is byte-legal at 4 bytes per pixel and so succeeds with the wrong meaning
  instead of failing. Nobody has yet read the D3D12 debug layer's own message for this.

Neither of the two hypotheses this task was asked to test survives as the explanation. `Save`
disturbing D3D11 state mid-frame is refuted outright: the *input* dump calls `Save` at the same point
in the same frame and succeeds. The repeating per-frame readback is refuted **as the cause of this
crash** — `RunFrame` did not fail, so the loop never ran — but the code path is real and worth
knowing about: `NoteChainImagesDumped` is called only on the success path, so a `RunFrame` that keeps
failing would re-dump the input every frame. Both hazards are noted on the config key in
`GraphicsSettings.cpp`.

**Not fixed here.** The dump is a debug-only, UI-hidden, default-off option. The fix — convert to
RGBA8 before saving, or refuse and log the format — is a scope decision rather than part of resolving
this section. Carried forward as **O11**. If this option is ever exposed in the UI, it has to be fixed
first.

### 7.3 UAT finding 1: D3D12 + slang shader renders a black screen

**Not reproducible on this build.** Run on 2026-09-18 at `bc6e7e1ccd`, three legs, Metroid Prime,
D3D12, validation layer on:

| leg | executor | preset | log evidence | result |
|---|---|---|---|---|
| 1 | librashader | crt-royale | `Direct3D 12 filter chain created` | renders; 292 distinct sampled colours |
| 2 | librashader | crt-geom | `Direct3D 12 filter chain created` | renders; 418 distinct sampled colours |
| 3 | built-in | crt-geom | `librashader unavailable (…); using the built-in post-processor.` | renders; 421 distinct sampled colours |

Leg 3 renamed the build tree's `librashader.dll` aside and back, and is the **first observation of
Task 10's availability-driven fallback on Windows** rather than an argument about it: the fallback
fired, named the reason, and the built-in executor rendered the preset. No errors or warnings in any
leg's log.

One limit of this matrix: leg 3 exercised the built-in executor with **crt-geom**, not crt-royale, so
§2.1's specific case — a pass declaring 9 samplers against D3D12's 8-SRV utility root signature — was
not re-tested. §2.1 stands as written and is unaffected by these three legs.

**No commit is claimed as the fix.** Tasks 1, 9a, 9b and 9d all touched D3D12 correctness on this
branch and any of them could plausibly account for it, but the mechanism was never identified, so this
is reported as unreproducible rather than fixed. If it returns, this matrix is the baseline to
re-run.

## 8. Risks

- **Rebuilding librashader with four runtimes** enlarges the shipped binary. **Materialised:**
  Windows grew from 8.6 MB to 36.6 MB (Task 5); macOS from 11.3 MB to 11.9 MB. Both D3D
  runtimes were proven on the target GPU and need no extra DLL (`runtime-d3d12-static` links
  DXC statically).
- **Metal and OpenGL runtimes are unproven here.** **Partially avoided:** both built
  successfully (Task 5). OpenGL was exercised on Windows with crt-royale (Task 8): the chain
  rendered correctly, but the whole frame presented upside down — including with post-processing
  off, so the flip predated this work. Diagnosed as one predicate answering two different
  questions and fixed in `d478b93f04`, which split it into `SlangNeedsClipYFlip` (texture
  targets: `Vulkan || OpenGL`) and `SlangNeedsPresentClipYFlip` (the present blit: `Vulkan`
  only, because OpenGL's lower-left window origin already accounts for it) —
  see `SlangTranslator.h:100-127`. OpenGL renders correctly and upright as of that commit.
  Metal compiled and links but was never run — no chain was created or
  executed on macOS (Task 9). The §4.3 fallback rule degrades backends whose runtime fails to
  initialise to the built-in executor, so Metal's unexercised state does not break the backend.
- **`RunFrame` (state reconciliation) is the likeliest source of new bugs**, because a missed
  invalidation shows up as corruption in unrelated later draws. **Avoided:** no corruption was
  observed on any backend during UAT. D3D11/D3D12 invalidate cached state and re-bind descriptor
  heaps per PCSX2's sequence; Vulkan reconciles image layout via `OverrideImageLayout`; Metal's
  encoder lifecycle management invalidates automatically (Tasks 6, 7, 9).
- **Dropping chains is user-visible.** **Materialised:** anyone who built a multi-preset chain
  loses entries (Task 11). On the librashader path they had already lost them silently (§2.3).

### 8.1 Accepted and not fixed

Two defects found in the final whole-branch review are real, were considered, and are
deliberately left alone. They are recorded here so that a later reader does not have to
rediscover them, and so that neither is mistaken for something nobody noticed.

1. **The parameter write is not synchronised against the video thread's read.** Editing a
   slider calls `Config::Layer::Set` on the Qt thread while the video thread may be inside
   `Config::GetAsString` for the same key. `Layer` carries no mutex — neither `Layer.h` nor
   `Layer.cpp` has one — so this is a data race on the layer's map in the strict sense.
   Not fixed because it is Dolphin's existing config model rather than anything this branch
   introduced: every `ConfigSlider` writes the same way from the same thread
   (`ConfigSlider.cpp:26` connects `valueChanged` straight to the config write), and dozens of
   graphics settings are read from the video thread the same way. Fixing it here would mean
   either locking Dolphin's config layers globally or giving this one setting a private
   snapshot, and the first is a tree-wide change while the second buys safety for one key out
   of hundreds. What this branch *does* keep off the hot path is the trigger: the video thread's
   per-frame check reads only `std::atomic<u32> s_generation`
   (`LibrashaderParameters.cpp:28`, polled at `LibrashaderPostProcessing.cpp:414-420`), so the
   unsynchronised string read happens only on a frame where an edit actually landed, not every
   frame. If Dolphin's config ever grows a lock, this site needs nothing new.
2. **In side-by-side and top-and-bottom stereo, the librashader frame counter advances twice per
   presented frame.** `Present.cpp:905-916` calls `BlitFromTexture` once per eye, and the
   executor increments `m_frame_count` on each successful `RunFrame`
   (`LibrashaderPostProcessing.cpp:482-484`, `:506-508`), so a preset using `FrameCount` — for
   animated noise, scanline phase, interlacing — advances at 2× in those two modes.
   Not fixed because the shape is pre-existing and shared: the built-in executor does exactly
   the same thing (`MultipassPostProcessing.cpp:724` increments inside `BlitFromTexture` too),
   and it did so at this branch's merge base, so nothing here regressed. Fixing it properly
   means deciding what a "frame" means to a shader that is invoked once per eye, which is a
   design question about stereo rather than about librashader — the honest answers are to
   increment on the first eye only, or to advance the counter in `Present` and pass it down. The
   visible effect is limited to animated presets, in the three stereo modes that blit once per eye:
   Side-by-Side and Top-and-Bottom (`Present.cpp:908-916`) and quad-buffered stereo on OpenGL
   (`:893-906`, taken when `bUsesExplictQuadBuffering`).

## 9. Verification

- Unit tests, all CPU-only, no GPU or Qt event loop. What that covers, stated as narrowly as
  the tests actually do — an earlier draft of this list claimed more than exists:
  - **Loader** — `LibrashaderLoaderTest.cpp`, four tests, and every one of them is a negative
    or a packaging assertion: a missing library reports a reason instead of crashing, the
    expected library path matches how each platform packages it, `DescribeAndFreeError`
    tolerates a null handle, and `GetSymbol` returns null for a name that is not there. It is
    *not* the "availability, symbol resolution, error formatting" triple this list used to
    claim: no test performs a successful load, resolves a real symbol, or formats a non-null
    librashader error. Those need the vendored binary to be loadable in the test process, which
    is now true on macOS and Windows — `Source/UnitTests/CMakeLists.txt` copies it beside the
    test binary unconditionally — but is not yet asserted anywhere.
    `LibrashaderParameters.EnumerateFixturePreset` is the one test that does exercise a real
    load end to end, and it does so through `Enumerate` rather than the loader's own surface.
  - **Parameters** — formatting, parsing, the decimals formula, the default-value epsilon, key
    derivation, and the Config round trip including deletion and the generation counter
    (`LibrashaderParametersTest.cpp`), plus enumeration across the C ABI against an in-tree
    fixture preset.
  - **Picker** — tree construction only: `PresetTreeTest.cpp`'s nine tests over
    `BuildPresetTree`. **Filtering is not covered.** It is `QSortFilterProxyModel` with
    `setRecursiveFilteringEnabled(true)` on `ROLE_PATH`
    (`ShaderPresetPickerDialog.cpp:76-80`), i.e. Qt's own behaviour driven from the dialog, so
    testing it needs a `QApplication` and a widget tree that this suite has no fixture for. The
    dialog-side logic worth testing is the selection bookkeeping when the filter hides the
    current row (`:136-150`); that is a gap, recorded rather than papered over.
  - **Preset resolution** — `PostProcessingConfigTest.cpp` covers what
    `ResolveConfiguredPreset` *returns* for a chain spec, whitespace and empty tails. The
    warning it logs when it drops trailing entries is not observed by any test; doing so would
    mean standing up `LogManager` and a listener, which no test in this suite does.
- Full unit suite green on macOS arm64: **1170 tests, 1168 passed, 2 skipped.** Both skips are
  environment-gated by design — `SlangCompile.RealPresetCompilesAllPasses` and
  `LibrashaderParameters.EnumerateRealPresetFromEnv`, which run only with `SLANG_PRESET` set to
  a preset from a real shader pack. (The figures previously recorded here, 1143/1144 and one
  skip, predate this branch's tests.)
- Windows x64: **not re-measured.** The last figure written down was 1455/1456 with one skip,
  from before this branch added tests and before the second environment-gated skip existed, so
  it cannot be right now and is not carried forward as if it were. It has to be re-run on the
  UAT host.
- Per-backend smoke on the Windows host: crt-royale on D3D11, D3D12 and Vulkan.
- The §7 instrumentation dump compared against the photometric table before finding 3 is
  called resolved.
