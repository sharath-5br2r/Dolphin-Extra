# Desktop Post-Processing Parity — Design

**Date:** 2026-09-16
**Status:** approved (scope decisions recorded in §5)
**Related:** [multipass slang post-processor replacement](2026-08-22-librashader-android-postprocessing-design.md), [android driver manager and postfx perf](2026-09-13-android-driver-manager-and-postfx-perf-design.md)

## 1. Problem

The fork's slang/RetroArch post-processing stack (`MultipassPostProcessing` + the
Vulkan-only `LibrashaderPostProcessing`) was built and validated for Android.
Windows and macOS release binaries compile and run it, but with six concrete
defects that make the desktop experience materially worse than Android's.

The engine itself is portable — it contains **zero platform `#ifdef`s**, all four
desktop backends accept the translator's GLSL (D3D11/D3D12 via
SPIRV-Cross GLSL→SPIR-V→HLSL, Metal via GLSL→SPIR-V→MSL, OGL/Vulkan natively),
and `MAX_PIXEL_SHADER_SAMPLERS = 16` is already sufficient on D3D/Metal/OGL.
There are no `.vcxproj`/`.sln` files in this tree, so Windows builds are
CMake-only and need no MSVC project edits.

## 2. Blockers

### B1 — Windows preset path resolution is broken

`VideoCommon::NormalizePath` (`SlangPreset.cpp:19`) is documented POSIX-only and
splits on `/` exclusively. `ResolvePath` (`SlangPreset.cpp:90`) feeds it
`base_dir + "/" + value`, where `base_dir` on Windows is a backslash path
(`E:\...\Sys\Shaders\shaders_slang\crt`). Consequences:

- Any `../` in a preset — universal in the libretro pack, including
  `crt/crt-royale.slangp` — makes `NormalizePath` pop the *entire*
  backslash-containing prefix as one token, destroying the drive and directory.
- A leading `..` beyond the root gets pushed rather than dropped, because
  `absolute` is detected as `path.front() == '/'`, which a Windows drive path
  never satisfies.

Effect: essentially the whole libretro preset library fails to load on Windows.

`MultipassPostProcessing.cpp`'s own `DirectoryOf` already uses
`find_last_of("/\\")`, so the base-directory split is fine; the defect is
confined to the lexical normalizer.

### B2 — Mipmapped passes are Vulkan-only

`MultipassPostProcessing.cpp:415` gates mip-chain allocation on
`g_backend_info.api_type == APIType::Vulkan`, because
`AbstractTexture::GenerateMipmaps()` is a no-op everywhere except
`VKTexture`. Windows defaults to D3D11 and macOS to Metal, so the **default**
desktop path silently degrades every preset that uses `mipmap_input` —
crt-royale's halation and bloom passes, and the RetroCrisis presets.

### B3 — The OpenGL image is vertically flipped

`SlangTranslator.cpp:436` emits the clip-space Y negation under
`#ifdef API_VULKAN`. Dolphin's own precedent
(`FramebufferShaderGen::GenerateScreenQuadVertexShader`, and four other sites in
that file) flips for **both** Vulkan and OpenGL: *"NDC space is flipped in
Vulkan. We also flip in GL so that (0,0) is in the lower-left."*
`MultipassPostProcessing::BuildPassthroughPipeline` (`:511`) has the identical
bug. (Recon correction: the OGL header *does* define `API_OPENGL 1`
— `ProgramShaderCache.cpp:840` — so a widened `#ifdef` would compile; see D2 for
why the fix is a translate-time decision anyway.)

### B4 — Three features are dead but still advertised in the Qt UI

`default_pre_post_process.glsl` and the old `PostProcessingConfigWindow` went
away with the engine replacement, taking their consumers with them:

- **Output resampling** — `g_ActiveConfig.output_resampling_mode` is written by
  `VideoConfig.cpp:152` and read by nothing.
- **Color correction** — every `GFX_CC_*` value lands in
  `g_ActiveConfig.color_correction` and is read by nothing.
  `ColorCorrectionConfigWindow` has exactly one caller,
  `EnhancementsWidget::ConfigureColorCorrection`.
- **Anaglyph / Passive stereo** — nothing outside the enum declaration reads
  `StereoMode::Anaglyph` or `StereoMode::Passive`. Selecting either now renders
  two layers and presents layer 0, i.e. it halves performance for no visible
  effect. The `Toggle 3D Anaglyph` hotkey (`HotkeyScheduler.cpp:594`) is dead
  for the same reason.

**HDR is *not* dead** and must stay. `g_Config.bHDR` still drives the swapchain
color space on all three HDR-capable backends —
`D3DCommon/SwapChain.cpp:56`, `VKSwapChain.cpp:199`,
`MTLMain.mm:170` (`kCGColorSpaceExtendedLinearSRGB` + RGBA16F). What the engine
replacement removed is the paper-white/tonemap scaling that the old default
post-process shader applied, so HDR presents an unscaled SDR image into an
extended-range swapchain. That is a pre-existing quality bug, not dead UI, and
restoring the scaling is out of scope here.

### B5 — librashader is unavailable on desktop

`Externals/librashader/` ships only headers plus an Android `arm64-v8a`
`librashader.so`. `librashader_ld.h` loads by bare name
(`LoadLibraryW(L"librashader.dll")` / `dlopen("librashader.dylib", RTLD_LAZY)`),
so a macOS bundle dylib in `Contents/Frameworks` would not be found — the same
problem `VulkanLoader.cpp:54` solves for MoltenVK with an absolute
`File::GetBundleDirectory()` path.

### B6 — The Qt UI is well behind Android's

Android has a post-processing renderer toggle (Builtin/librashader), a two-step
category→shader picker with **Select** and **Add to Chain**, an arrow-joined
chain summary, and three download entries driven by the pack registry
(`libretro`, `satpixie`, `retrocrisis` with a profile prompt). Qt has a single
flat combo box and one hardcoded `SLANG_SHADER_PACK_URL` download button.

## 3. Verification gap

`SlangCompileTest` compiles translated passes only with the Vulkan GLSL header.
Nothing validates that the translator's output compiles under the D3D, Metal or
OGL headers, which is exactly what B3's fix changes.

## 4. Out of scope

The Android GPU driver manager (`GFX_DRIVER_PACKAGE`, `libadrenotools`, Turnip
driver injection) is Android-only by construction and is **not** part of desktop
parity.

## 5. Scope decisions

| Question | Decision |
| --- | --- |
| B4: what to do with the dead controls | **Remove them** from `EnhancementsWidget` and drop Anaglyph/Passive from the stereo mode list. Do not resurrect the features. Scope amended during recon: the HDR checkbox stays, because `bHDR` still selects the swapchain color space (see B4). |
| B2: which backends get mip generation | **All four**, including D3D12. |
| B5: which librashader artifacts | macOS **arm64 dylib** + Windows **x64 DLL**, `runtime-vulkan` only. Not the Metal runtime. |

## 6. Design decisions

### D1 — Normalize inside `NormalizePath`, not at every call site

Make `NormalizePath` separator-agnostic: accept `\` and `/`, always emit `/`,
and recognise a Windows drive prefix (`X:`) plus a UNC prefix (`\\host`) as an
absolute root that `..` cannot escape. Forward-slash paths work fine with every
Win32 file API, so every downstream consumer
(`ResolvePath`, `DirectoryOf`, `ExpandSlangIncludes`) becomes correct for free.
The alternative — normalizing `base_dir` in `AppendPreset` — leaves the trap
armed for any future caller.

### D2 — Decide the Y flip at translate time from the API type

Add `constexpr bool SlangNeedsClipYFlip(APIType)` to `SlangTranslator.h`
(returns true for Vulkan and OpenGL, mirroring `FramebufferShaderGen`) and pass
the result into `TranslateSlangPass` as an explicit `bool flip_clip_y`. The
translator then emits the negation unconditionally or not at all.
`BuildPassthroughPipeline` uses the same helper.

Widening the guard to `#if defined(API_VULKAN) || defined(API_OPENGL)` would also
compile — both macros exist — but the translate-time decision is preferable: it
is unit-testable with no backend (the compile oracle in §3 can assert the
injection per `APIType`), it keeps the single injection site
(`SlangTranslator.cpp:436` is the translator's *only* API macro use), and it
mirrors `FramebufferShaderGen`'s runtime `GetAPIType()` check rather than
introducing a second, macro-based convention alongside it.

### D3 — Native mip generation where it is one call; a portable draw-based
### fallback for D3D12

Metal (`generateMipmapsForTexture:`), OpenGL (`glGenerateMipmap`) and D3D11
(`GenerateMips` + `D3D11_RESOURCE_MISC_GENERATE_MIPS`) each need one call and no
extra memory, so they get native `AbstractTexture::GenerateMipmaps()` overrides
and set a new `BackendInfo::bSupportsGPUMipGeneration`.

D3D12 has no built-in equivalent. A native implementation would need per-mip
RTVs, per-mip SRVs, **per-subresource** resource barriers (the existing
`TransitionToState` transitions the whole resource) and a dedicated pipeline —
roughly 200 lines of D3D12 that cannot be exercised on the macOS dev host.
`AbstractGfx::ScaleTexture` is not usable as a shortcut: it asserts an RGBA8
destination and the post-processing intermediates are RGBA16F.

Instead, add a **portable** `VideoCommon::MipChainBuilder`, living in VideoCommon
and owned by `MultipassPostProcessing`. To fill level *k* it draws
`textureLod(src, uv, k-1)` into a scratch **single-level** render target — so the
draw's source and destination are always different resources, needing no
per-subresource barrier and no read-write hazard on the mip chain — then
`CopyRectangleFromTexture`s the scratch's top-left `w_k × h_k` sub-rect 1:1 into
the real mip level. One scratch target sized to level 1 (a quarter of level-0
memory) serves every level; the source LOD travels as a single utility uniform,
following `AbstractGfx::ScaleTexture`'s `UploadUtilityUniforms` pattern.

Because the builder is a VideoCommon consumer of `AbstractGfx`, **D3D12 itself
needs no changes**: the choice is made once at the
`MultipassPostProcessing` call site (native `GenerateMipmaps()` when
`bSupportsGPUMipGeneration`, the builder otherwise), which also means the
fallback path can be exercised on any backend by flipping one bool. Cost is N-1
extra draws plus N-1 copies per mipped pass per frame, paid only by presets that
actually request `mipmap_input`.

### D4 — Reuse the Android pack registry and preset list verbatim in Qt

`MultipassPostProcessing::GetPresetList()`, `GetShaderPackSources()`,
`DownloadShaderPackById()` and the RetroCrisis profile helpers are already
backend- and UI-agnostic — the Android JNI layer is a thin shim over them. The Qt
work is a dialog (`PostProcessingChainDialog`) plus a renderer combo, with the
category/chain string logic extracted into a testable pure helper so it is not
duplicated between the two front ends.

### D5 — Absolute-path library loading via a pure helper

Add `VideoCommon::LibrashaderLibraryPath()` returning the platform-appropriate
absolute path (macOS: `<bundle>/Contents/Frameworks/librashader.dylib`;
Windows: next to the executable; Android/Linux: the bare name, preserving
today's behaviour), and load through it rather than
`librashader_ld.h`'s bare-name macro.

## 7. Success criteria

1. `crt/crt-royale.slangp` loads and renders on Windows (D3D11, D3D12, Vulkan,
   OGL) and macOS (Metal, Vulkan), with halation/bloom visibly present.
2. The OpenGL backend renders the chain right-side up.
3. Qt exposes the renderer toggle, the category→shader picker with Select /
   Add to Chain, the chain summary, and all three pack downloads including the
   RetroCrisis profile prompt.
4. `librashader.dylib` / `librashader.dll` are packaged and
   `librashader_load_instance()` reports a loaded instance on both platforms.
5. Output Resampling, Color Correction, Anaglyph and Passive are gone from the Qt
   graphics UI; HDR still works and still selects an scRGB swapchain.
6. `ninja -C build-qt unittests` is green, including translator compilation
   against the D3D, Metal and OGL shader headers.
