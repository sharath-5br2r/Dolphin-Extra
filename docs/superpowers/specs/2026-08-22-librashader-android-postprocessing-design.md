# librashader Android/Vulkan Post-Processing — Design

**Date:** 2026-08-22
**Status:** Approved for planning
**Scope:** Android + Vulkan only. Desktop DolphinQt is out of bounds and must not be touched.

## Goal

Add [librashader](https://github.com/SnowflakePowered/librashader) (RetroArch's
official Rust slang-shader runtime) as a selectable post-processing engine on
Android's Vulkan backend, running side-by-side with Dolphin's existing homegrown
`MultipassPostProcessing` slang path. A new app setting, **"Post-processing
renderer,"** chooses between the two at runtime. The same `.slangp` preset feeds
whichever engine is active, so a user can A/B the identical shader on-device.

## Motivation

Dolphin's homegrown multipass slang translator renders some presets incorrectly
on the AYN Thor (Adreno/Vulkan): crt-royale is too dark and RetroCrisis GDV-NTSC
is black, while CRT-SatPixie is correct. Two root-cause theories were refuted
on-device and the investigation was abandoned. librashader is the reference
implementation of the slang runtime; a feasibility spike proved it
cross-compiles cleanly to `aarch64-linux-android` (NDK 29 — the same NDK Dolphin
uses), produces a valid `.so` exporting the full Vulkan C API, and loads on the
target device. Rather than keep debugging the homegrown path, we route rendering
through librashader on Vulkan while keeping the homegrown path for comparison,
fallback, and non-Vulkan backends.

## Global Constraints

- **Android + Vulkan only.** Do NOT modify DolphinQt (desktop UI). Editing
  Android Kotlin/JNI settings code is in bounds; editing `Source/Core/DolphinQt/`
  is not.
- New C++ files begin with `// Copyright 2026 Dolphin Emulator Project` and
  `// SPDX-License-Identifier: GPL-2.0-or-later`.
- No `std::from_chars` for floats (use `strtof`/`strtol`).
- VideoCommon C++ lives in `namespace VideoCommon`.
- Unit tests live under `Source/UnitTests/VideoCommon/PostProcessing/` and are
  registered via `add_dolphin_test`.
- librashader is integrated via the MIT-licensed `librashader_ld.h` runtime
  loader (dlopen); the prebuilt `.so` (MPL-2.0 OR GPL-3.0-only) is compatible
  with this personal GPL-2.0-or-later branch.

## Architecture

### Post-processor interface

Today `Presenter` owns a single concrete `MultipassPostProcessing` object
(`Present.cpp:123`) and drives it through five methods. We extract those into a
minimal abstract interface so the two engines are interchangeable without
touching Presenter's per-frame logic:

```cpp
namespace VideoCommon
{
class IPostProcessor
{
public:
  virtual ~IPostProcessor() = default;
  virtual bool Initialize(AbstractTextureFormat format) = 0;
  virtual void RecompileShader() = 0;
  virtual void RecompilePipeline() = 0;
  virtual void BlitFromTexture(const MathUtil::Rectangle<int>& dst,
                               const MathUtil::Rectangle<int>& src,
                               const AbstractTexture* src_tex, int src_layer,
                               u32 native_width, u32 native_height) = 0;
};
}
```

`MultipassPostProcessing` gains `: public IPostProcessor` and `override` on those
five methods (no behavior change). `GetPresetList()` stays a free/static function
— it is not per-instance. `Presenter::m_post_processor` becomes
`std::unique_ptr<IPostProcessor>`.

### Engine selection

A factory function decides which engine to build:

```cpp
std::unique_ptr<IPostProcessor> CreatePostProcessor();  // reads config + backend
```

Selection logic:

```
if (g_backend_info.api_type == APIType::Vulkan
    && Config::Get(GFX_ENHANCE_POST_PROCESS_RENDERER) == PostProcessRenderer::Librashader)
    -> LibrashaderPostProcessing
else
    -> MultipassPostProcessing        // default; desktop and Android-GLES always here
```

"Host is Android" does not need an explicit check: `LibrashaderPostProcessing`
is only compiled/available where the prebuilt `.so` ships (Android), and desktop
never exposes the setting. The Vulkan guard plus the config default keep every
non-Android, non-Vulkan path on the homegrown engine.

### Live toggle

Changing the renderer applies immediately, no app restart. `Present.cpp` already
watches config-change bits and calls `RecompileShader()` /`RecompilePipeline()`.
We add a bit for the renderer setting; when it flips, Presenter tears down
`m_post_processor` and rebuilds it via `CreatePostProcessor()`, then calls
`Initialize(m_backbuffer_format)`. If librashader initialization fails, the
factory returns the homegrown engine so the user is never left with a dead
screen.

### librashader binding (the C-ABI bridge)

`LibrashaderPostProcessing` is Vulkan-only and talks to the prebuilt `.so` via
`librashader_ld.h` with `#define LIBRA_RUNTIME_VULKAN`. That header dlopens
`liblibrashader_capi.so`, resolves the `libra_*` symbols into a `libra_instance_t`
struct of function pointers, and installs no-op stubs for any that fail to
load — so a missing/incompatible library degrades gracefully instead of
crashing.

Lifecycle:

- **Load instance once** (process-wide): a small RAII wrapper holds the
  `libra_instance_t`. If load fails, `Initialize` reports failure → factory falls
  back to homegrown.
- **Initialize / RecompileShader:** read `GFX_ENHANCE_POST_SHADER`, resolve the
  `.slangp` path, `libra_preset_create(path, &preset)`, then
  `libra_vk_filter_chain_create(preset, device_vk, opts, &chain)`.
  `libra_device_vk_t` has exactly five fields —
  `{ physical_device, instance, device, queue, entry }` — filled from
  `g_vulkan_context`: `GetPhysicalDevice()`, `GetVulkanInstance()`,
  `GetDevice()`, `GetGraphicsQueue()`, and `entry` = `PFN_vkGetInstanceProcAddr`
  (Dolphin's Vulkan loader exposes `vkGetInstanceProcAddr`). There is no
  queue-family-index field. The preset handle is consumed by create and must be
  recreated on the next reload.
- **BlitFromTexture (per frame):**
  1. Downcast `src_tex` → `VKTexture`; build
     `libra_image_vk_t in{ GetImage(), GetVkFormat(), width, height }`.
  2. Get the output from `g_gfx->GetCurrentFramebuffer()`; downcast its color
     attachment → `VKTexture`; build `libra_image_vk_t out{...}`.
  3. Transition `in` to the layout librashader expects for sampling and `out`
     to color-attachment/general, using `VKTexture::TransitionToLayout`; end any
     active render pass first (librashader manages its own passes internally).
  4. Build `libra_viewport_t{ dst.x, dst.y, dst.width, dst.height }`.
  5. `libra_vk_filter_chain_frame(chain, g_command_buffer_mgr->GetCurrentCommandBuffer(),
     m_frame_count++, in, out, &viewport, nullptr, &frame_opt)`.
  6. Reconcile the output image's layout back into Dolphin's `VKTexture` /
     `StateTracker` tracking so subsequent Dolphin rendering sees the correct
     layout.
- **Errors** returned by any `libra_*` call are logged and, on the frame path,
  fall back to a passthrough copy of `src_tex` into the target for that frame so
  a bad preset never blanks the screen.

The image-layout handoff and the active-render-pass boundary (steps 3 and 6) are
the primary integration risks; compile + load are already proven by the spike.
The implementation plan gives them dedicated, separately-verified tasks.

## Build & Packaging

- **Vendor the prebuilt `.so`** at
  `Source/Android/app/src/main/jniLibs/arm64-v8a/liblibrashader_capi.so`. Gradle
  packages `jniLibs/<abi>/` into the APK's `lib/<abi>/` automatically
  (`jniLibs.useLegacyPackaging = true` is already set) — no CMake change and no
  link-time dependency, because we dlopen.
- **Vendor the C headers** (`librashader.h`, `librashader_ld.h`) under a new
  `Externals/librashader/include/` and add that include dir to the Vulkan
  backend / the consuming VideoCommon target so the binding compiles. Only the
  Android build references `LibrashaderPostProcessing`.
- **Reproducibility:** a short README beside the vendored `.so` documents the
  exact build recipe (clone tag `librashader-cache-v0.12.0`; NDK
  `29.0.14206865`; `cargo build -p librashader-capi --release --target
  aarch64-linux-android --no-default-features --features runtime-vulkan` with the
  NDK clang linker in `.cargo/config.toml`).
- **`libc++_shared.so`:** required by the `.so` (a NEEDED dependency). Dolphin's
  Android build already packages `libc++_shared`; confirm it is present in the
  APK and, if not, add it to `jniLibs/arm64-v8a/`.

**Rejected alternative — build from source in CMake:** adding librashader under
`Externals/` and invoking cargo from CMake would require a Rust toolchain and the
Android cross-target in every build environment and CI. For a personal branch on
one device, vendoring a documented prebuilt binary is far simpler and adequate.

## Config & Settings UI

- **New C++ config:** `GFX_ENHANCE_POST_PROCESS_RENDERER` as an `Info<int>`
  enum (`GraphicsSettings.h/.cpp`), values `Builtin = 0` (default),
  `Librashader = 1`. A change sets a new `CONFIG_CHANGE_BIT_*` consumed by
  `Present.cpp`.
- **The `.slangp` picker (`GFX_ENHANCE_POST_SHADER`) is unchanged and shared** —
  the same preset drives whichever engine is active.
- **Android UI** (Kotlin):
  - Add an `IntSetting` enum entry `GFX_ENHANCE_POST_PROCESS_RENDERER` mapping to
    the config key (mirroring existing `IntSetting`/enum settings).
  - Add a `SingleChoiceSetting` row to the **Enhancements** section in
    `SettingsFragmentPresenter.kt`, next to the post-processing picker, titled
    **"Post-processing renderer"** with options "Built-in" and
    "librashader (Vulkan)" and a description noting librashader applies only to
    the Vulkan backend.
  - On non-Vulkan backends the setting is ignored by the `Present.cpp` guard; the
    row stays visible with the "Vulkan only" description rather than adding
    backend-conditional visibility logic.

## Testing

Most of the integration is device-bound (needs a real Adreno `VkDevice`) and is
verified on the AYN Thor by visual comparison:

- **Acceptance (on-device):** with renderer = librashader, crt-royale renders
  correctly (not dark), RetroCrisis GDV-NTSC renders (not black), CRT-SatPixie
  stays correct; toggling the setting live switches engines without restart;
  switching back to Built-in reproduces the old behavior.

TDD applies to the extractable **pure/host-testable** pieces:

- The `IPostProcessor` refactor keeps `MultipassPostProcessing`'s existing tests
  green (no behavior change) — the regression guard for the extraction.
- Rect → `libra_viewport_t` mapping and any preset-path resolution helper are
  pure and unit-tested under `Source/UnitTests/VideoCommon/PostProcessing/`.
- The instance-loader wrapper gets a seam so "library absent → graceful
  no-op/fallback" is testable without the real `.so`.

The device-bound Vulkan glue (layout transitions, `libra_vk_filter_chain_frame`
wiring) is an explicit **TDD skip**, verified on-device, and called out as such
per policy.

## Out of Scope

- Desktop/DolphinQt integration of librashader.
- Android OpenGL ES support (librashader-vk is Vulkan-only).
- Building librashader from source in CI.
- x86_64 Android ABI (spike and vendoring target arm64-v8a; the setting simply
  falls back to Built-in where no `.so` ships).
