# librashader Android/Vulkan Post-Processing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add librashader as a runtime-selectable post-processing engine on Android's Vulkan backend, side-by-side with the existing homegrown `MultipassPostProcessing`, chosen by a new "Post-processing renderer" app setting.

**Architecture:** Extract an `IPostProcessor` interface both engines implement. `AbstractGfx` gains a virtual `CreatePostProcessor()` factory (default → `MultipassPostProcessing`, in VideoCommon); the Vulkan backend's `VKGfx` overrides it to return the new Vulkan-only `LibrashaderPostProcessing` when the config selects it. This respects layering: the librashader class lives in `VideoBackends/Vulkan` (it needs raw Vulkan handles), never in VideoCommon. librashader is loaded at runtime via the MIT `librashader_ld.h` (dlopen); a prebuilt arm64 `.so` is vendored into `jniLibs`.

**Tech Stack:** C++ (VideoCommon + Vulkan backend), Kotlin (Android settings UI), prebuilt Rust `liblibrashader_capi.so` (v0.12.0, NDK 29, `runtime-vulkan`), librashader C headers.

**Spec:** `docs/superpowers/specs/2026-08-22-librashader-android-postprocessing-design.md`

## Global Constraints

- **Android + Vulkan only. Do NOT modify `Source/Core/DolphinQt/`.** Android Kotlin/JNI and `VideoBackends/Vulkan` are in bounds.
- New C++ files start with `// Copyright 2026 Dolphin Emulator Project` then `// SPDX-License-Identifier: GPL-2.0-or-later`.
- No `std::from_chars` for floats (use `strtof`/`strtol`).
- VideoCommon C++ in `namespace VideoCommon`.
- Unit tests under `Source/UnitTests/VideoCommon/PostProcessing/`, registered via `add_dolphin_test` in `Source/UnitTests/VideoCommon/CMakeLists.txt`.
- Commit messages end with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- Much of this is device-bound (real Adreno `VkDevice`); those tasks are explicit TDD skips verified on-device on the AYN Thor (device id `64dc3c35`).

## File Structure

- **Create** `Source/Core/VideoCommon/PostProcessing/IPostProcessor.h` — the abstract interface (5 methods Presenter uses).
- **Modify** `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.h` — `: public IPostProcessor`, `override` the 5 methods.
- **Modify** `Source/Core/VideoCommon/AbstractGfx.h/.cpp` — add `virtual std::unique_ptr<VideoCommon::IPostProcessor> CreatePostProcessor();` (default builds `MultipassPostProcessing`).
- **Modify** `Source/Core/VideoCommon/Present.h/.cpp` — hold `unique_ptr<IPostProcessor>`; build via `g_gfx->CreatePostProcessor()`; live-rebuild on renderer change.
- **Modify** `Source/Core/VideoCommon/VideoConfig.h/.cpp`, `Source/Core/Core/Config/GraphicsSettings.h/.cpp` — new enum config + change bit.
- **Create** `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.h/.cpp` — the Vulkan librashader engine.
- **Modify** `Source/Core/VideoBackends/Vulkan/VKGfx.h/.cpp` — override `CreatePostProcessor()`.
- **Modify** `Source/Core/VideoBackends/Vulkan/CMakeLists.txt` — add the new sources + librashader include dir.
- **Create** `Externals/librashader/include/{librashader.h,librashader_ld.h}` + `Externals/librashader/README.md` (build recipe).
- **Create** `Source/Android/app/src/main/jniLibs/arm64-v8a/liblibrashader_capi.so` and `.../libc++_shared.so`.
- **Modify** Kotlin: `IntSetting.kt`, `ui/SettingsFragmentPresenter.kt`, `res/values/strings.xml`, `res/values/arrays.xml`.

---

### Task 1: Extract `IPostProcessor` interface (behavior-preserving refactor)

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/IPostProcessor.h`
- Modify: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.h:27` (class decl), and add `override` to the 5 methods
- Modify: `Source/Core/VideoCommon/AbstractGfx.h` (after line 123 factory block), `Source/Core/VideoCommon/AbstractGfx.cpp`
- Modify: `Source/Core/VideoCommon/Present.h:25,94,168`, `Source/Core/VideoCommon/Present.cpp:123`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt` (add `PostProcessing/IPostProcessor.h`) and `Source/Core/DolphinLib.props` (Windows project list — header only; safe to add, does not affect Android/desktop-Vulkan build but keeps the VS project in sync)

**Interfaces:**
- Produces: `VideoCommon::IPostProcessor` (pure virtual: `Initialize`, `RecompileShader`, `RecompilePipeline`, `BlitFromTexture`); `AbstractGfx::CreatePostProcessor()` returning `std::unique_ptr<VideoCommon::IPostProcessor>`.

- [ ] **Step 1: Confirm the regression guard fails-safe first — run the existing post-processing tests to establish green baseline**

Run: `cd Source/Android && ./gradlew assembleDebug` is NOT for tests. Instead build+run the host unit tests per the macOS build workarounds memory. Establish that `SlangPresetTest`, `PassGraphTest`, `PresetNameTest` etc. currently pass. (These are the behavior-preservation guard for this refactor.)
Expected: PASS (baseline).

- [ ] **Step 2: Write `IPostProcessor.h`**

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture;

namespace VideoCommon
{
// Backend-agnostic post-processing engine interface. Presenter owns one of these and drives it
// per frame. Implemented by MultipassPostProcessing (all backends) and, on Vulkan, by
// LibrashaderPostProcessing.
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
}  // namespace VideoCommon
```

- [ ] **Step 3: Make `MultipassPostProcessing` implement the interface**

In `MultipassPostProcessing.h`: `#include "VideoCommon/PostProcessing/IPostProcessor.h"`, change `class MultipassPostProcessing` → `class MultipassPostProcessing final : public IPostProcessor`, and add `override` to `Initialize`, `RecompileShader`, `RecompilePipeline`, `BlitFromTexture`. Remove the now-redundant default arguments on the overridden `BlitFromTexture` (the interface declares the full signature `int src_layer, u32 native_width, u32 native_height` with no defaults; callers in Present.cpp already pass all args — verify at `Present.cpp:884,888,898,900,907`). Keep `GetPresetList()` as the existing `static` (not part of the interface; DolphinQt + JNI call it directly).

- [ ] **Step 4: Add the `CreatePostProcessor` virtual to `AbstractGfx`**

In `AbstractGfx.h` add near the other `Create*` virtuals (after ~line 123):
```cpp
  // Builds the post-processing engine for this backend. Default is the backend-agnostic
  // MultipassPostProcessing; Vulkan overrides this to optionally return LibrashaderPostProcessing.
  virtual std::unique_ptr<VideoCommon::IPostProcessor> CreatePostProcessor();
```
In `AbstractGfx.cpp` add:
```cpp
#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"
// ...
std::unique_ptr<VideoCommon::IPostProcessor> AbstractGfx::CreatePostProcessor()
{
  return std::make_unique<VideoCommon::MultipassPostProcessing>();
}
```
Add a forward declaration `namespace VideoCommon { class IPostProcessor; }` in `AbstractGfx.h`.

- [ ] **Step 5: Retarget Presenter to the interface**

In `Present.h`: replace fwd-decl `class MultipassPostProcessing;` (line 25) with `class IPostProcessor;` inside `namespace VideoCommon`; change `GetPostProcessor()` (line 94) return type to `VideoCommon::IPostProcessor*`; change member (line 168) to `std::unique_ptr<VideoCommon::IPostProcessor> m_post_processor;`.
In `Present.cpp:123`: replace `m_post_processor = std::make_unique<VideoCommon::MultipassPostProcessing>();` with `m_post_processor = g_gfx->CreatePostProcessor();`. Add `#include "VideoCommon/PostProcessing/IPostProcessor.h"` and remove the now-unneeded `MultipassPostProcessing.h` include if unused elsewhere in the file (grep first).

- [ ] **Step 6: Build (host + Android) and run the post-processing unit tests**

Run the host unit-test build (per macOS workarounds memory) and `cd Source/Android && ./gradlew assembleDebug`.
Expected: compiles on both; `SlangPresetTest`/`PassGraphTest`/`PresetNameTest`/… all PASS unchanged (no behavior change). DolphinQt is untouched and still references only the static `GetPresetList()`.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "VideoCommon: extract IPostProcessor interface and CreatePostProcessor factory

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Add the "Post-processing renderer" config + live-toggle plumbing

**Files:**
- Modify: `Source/Core/VideoCommon/VideoConfig.h` (enum + field + `CONFIG_CHANGE_BIT_POST_PROCESS_RENDERER`)
- Modify: `Source/Core/Core/Config/GraphicsSettings.h:124` area (decl) + `GraphicsSettings.cpp:146` area (def)
- Modify: `Source/Core/VideoCommon/VideoConfig.cpp` (Refresh mapping ~line 153; old_ capture ~line 310; change-bit ~line 363)
- Modify: `Source/Core/VideoCommon/Present.cpp:371` (`ConfigChanged`)

**Interfaces:**
- Consumes: `AbstractGfx::CreatePostProcessor()` (Task 1).
- Produces: `Config::GFX_ENHANCE_POST_PROCESS_RENDERER` (`Info<PostProcessRenderer>`); `g_ActiveConfig.post_process_renderer`; `CONFIG_CHANGE_BIT_POST_PROCESS_RENDERER = (1 << 11)`.

- [ ] **Step 1: Define the enum + config field (`VideoConfig.h`)**

Near the other enums (after `OutputResamplingMode`, ~line 76):
```cpp
enum class PostProcessRenderer : int
{
  Builtin = 0,      // homegrown MultipassPostProcessing (default, all backends)
  Librashader = 1,  // librashader via dlopen (Vulkan/Android only)
};
```
Add field to `VideoConfig` struct (near `sPostProcessingShader`, ~line 224):
```cpp
  PostProcessRenderer post_process_renderer = PostProcessRenderer::Builtin;
```
Add the change bit to `enum ConfigChangeBits` (after `CONFIG_CHANGE_BIT_HDR = (1 << 10)`, line 123):
```cpp
  CONFIG_CHANGE_BIT_POST_PROCESS_RENDERER = (1 << 11),
```

- [ ] **Step 2: Declare + define the `Info` (`GraphicsSettings.h/.cpp`)**

`GraphicsSettings.h` after line 124:
```cpp
extern const Info<PostProcessRenderer> GFX_ENHANCE_POST_PROCESS_RENDERER;
```
(Ensure `enum class PostProcessRenderer` is visible — it is declared in `VideoConfig.h`; include or forward as the file already does for `OutputResamplingMode`.)
`GraphicsSettings.cpp` after the `GFX_ENHANCE_POST_SHADER` def (~line 149):
```cpp
const Info<PostProcessRenderer> GFX_ENHANCE_POST_PROCESS_RENDERER{
    {System::GFX, "Enhancements", "PostProcessRenderer"}, PostProcessRenderer::Builtin};
```

- [ ] **Step 3: Map config → g_Config and emit the change bit (`VideoConfig.cpp`)**

In `Refresh()` after line 153 (`sPostProcessingShader = ...`):
```cpp
  post_process_renderer = Config::Get(Config::GFX_ENHANCE_POST_PROCESS_RENDERER);
```
In the old_ capture block after line 310:
```cpp
  const auto old_post_process_renderer = g_ActiveConfig.post_process_renderer;
```
After the post-shader change-bit (line 363):
```cpp
  if (old_post_process_renderer != g_ActiveConfig.post_process_renderer)
    changed_bits |= CONFIG_CHANGE_BIT_POST_PROCESS_RENDERER;
```

- [ ] **Step 4: Live-rebuild the post-processor on renderer change (`Present.cpp`)**

In `Presenter::ConfigChanged` (after the post-shader block, ~line 381):
```cpp
  if (changed_bits & ConfigChangeBits::CONFIG_CHANGE_BIT_POST_PROCESS_RENDERER && g_gfx)
  {
    g_gfx->WaitForGPUIdle();
    m_post_processor = g_gfx->CreatePostProcessor();
    if (m_post_processor)
      m_post_processor->Initialize(m_backbuffer_format);
  }
```
(At this point `CreatePostProcessor()` still always returns builtin — the toggle is inert until Task 4 adds the VKGfx override. That is intentional; the tree stays working.)

- [ ] **Step 5: Build host + Android**

Run host unit-test build and `./gradlew assembleDebug`.
Expected: both compile; no behavior change (renderer defaults to Builtin). *TDD skip: pure config/plumbing, no new logic to unit-test — called out explicitly.*

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "VideoCommon: add PostProcessRenderer config + live-toggle rebuild

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Vendor the prebuilt librashader `.so`, headers, and build wiring

**Files:**
- Create: `Externals/librashader/include/librashader.h`, `Externals/librashader/include/librashader_ld.h` (copied from the built checkout)
- Create: `Externals/librashader/README.md` (build recipe + version pin)
- Create: `Source/Android/app/src/main/jniLibs/arm64-v8a/liblibrashader_capi.so`
- Create: `Source/Android/app/src/main/jniLibs/arm64-v8a/libc++_shared.so`
- Modify: `Source/Core/VideoBackends/Vulkan/CMakeLists.txt` (add the include dir)

- [ ] **Step 1: Copy the vendored headers**

```bash
mkdir -p Externals/librashader/include
cp /tmp/librashader-spike/librashader/include/librashader.h Externals/librashader/include/
cp /tmp/librashader-spike/librashader/include/librashader_ld.h Externals/librashader/include/
```

- [ ] **Step 2: Copy the prebuilt `.so` + libc++_shared into jniLibs**

```bash
mkdir -p Source/Android/app/src/main/jniLibs/arm64-v8a
cp /tmp/librashader-spike/librashader/target/aarch64-linux-android/release/liblibrashader_capi.so \
   Source/Android/app/src/main/jniLibs/arm64-v8a/
cp "/opt/homebrew/share/android-commandlinetools/ndk/29.0.14206865/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
   Source/Android/app/src/main/jniLibs/arm64-v8a/
```
Rationale for `libc++_shared.so`: the `.so` lists it as a NEEDED dependency (spike-verified), and Dolphin builds with `ANDROID_STL=c++_static`, so libc++_shared is not otherwise present in the APK. The two libc++ instances do not clash because librashader's ABI boundary is plain C (spike-verified: the `.so` loaded and resolved symbols on-device with libc++_shared alongside).

- [ ] **Step 3: Write `Externals/librashader/README.md`**

Document: upstream `https://github.com/SnowflakePowered/librashader`, tag `librashader-cache-v0.12.0`; NDK `29.0.14206865`; the `.cargo/config.toml` linker (`aarch64-linux-android24-clang`) + `AR/CC/CXX/RANLIB_aarch64_linux_android` env; build command `cargo build -p librashader-capi --release --target aarch64-linux-android --no-default-features --features runtime-vulkan`; artifact path; ABI 2 / API 5; license MPL-2.0 OR GPL-3.0-only.

- [ ] **Step 4: Add the include dir to the Vulkan backend CMake**

In `Source/Core/VideoBackends/Vulkan/CMakeLists.txt`, add to the Vulkan target's include dirs:
```cmake
target_include_directories(videovulkan PRIVATE ${CMAKE_SOURCE_DIR}/Externals/librashader/include)
```
(Match the exact target name used in that file — grep for `target_include_directories`/`add_library` there first.)

- [ ] **Step 5: Build the APK and verify the `.so` is packaged**

Run: `cd Source/Android && ./gradlew assembleDebug`
Then verify: `unzip -l app/build/outputs/apk/debug/app-debug.apk | grep -E "librashader_capi|libc\+\+_shared"`
Expected: both `lib/arm64-v8a/liblibrashader_capi.so` and `lib/arm64-v8a/libc++_shared.so` present. *TDD skip: packaging step, verified by APK inspection.*

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Externals/Android: vendor prebuilt librashader arm64 .so + headers

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: `LibrashaderPostProcessing` skeleton — instance load, factory, passthrough

**Files:**
- Create: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.h/.cpp`
- Modify: `Source/Core/VideoBackends/Vulkan/VKGfx.h/.cpp` (override `CreatePostProcessor`)
- Modify: `Source/Core/VideoBackends/Vulkan/CMakeLists.txt` (add the two new sources)

**Interfaces:**
- Consumes: `IPostProcessor` (Task 1), `GFX_ENHANCE_POST_PROCESS_RENDERER` (Task 2), vendored headers (Task 3), `g_vulkan_context` accessors, `VKTexture` downcast.
- Produces: `Vulkan::LibrashaderPostProcessing : public VideoCommon::IPostProcessor`; `VKGfx::CreatePostProcessor()` override.

- [ ] **Step 1: Define the librashader loader + class header**

`LibrashaderPostProcessing.h` (`namespace Vulkan`): include `<librashader_ld.h>` guarded by `#define LIBRA_RUNTIME_VULKAN` in the `.cpp` only (the header defines many statics; include it in exactly one TU). Declare the class implementing `IPostProcessor`, holding: a `libra_instance_t`, a `libra_vk_filter_chain_t m_chain = nullptr`, `AbstractTextureFormat m_format`, `size_t m_frame_count = 0`, and a `bool m_available = false`. Provide a static `bool IsAvailable()` that lazily loads the instance once and reports whether the `.so` + required `vk_*` symbols resolved.

- [ ] **Step 2: Implement Initialize/RecompileShader/RecompilePipeline + passthrough BlitFromTexture**

- `Initialize(format)`: store format; if `!IsAvailable()` return `false`; call `RecompileShader()`; return true.
- `RecompileShader()`: free any existing chain (`vk_filter_chain_free`); read `Config::Get(GFX_ENHANCE_POST_SHADER)`, resolve to a `.slangp` absolute path (reuse the same resolution the builtin path uses — see `MultipassPostProcessing::LoadPreset`/`GetPresetList`); `libra_preset_create(path, &preset)`; fill `libra_device_vk_t{ GetPhysicalDevice(), GetVulkanInstance(), GetDevice(), GetGraphicsQueue(), &::vkGetInstanceProcAddr }`; `libra_vk_filter_chain_create(preset, device, nullptr, &m_chain)`. On any error, log, leave `m_chain == nullptr` (passthrough).
- `RecompilePipeline()`: no-op (librashader handles internal pipelines) or re-`RecompileShader()` on stereo change; document choice.
- `BlitFromTexture(...)`: **for this task, always do a passthrough copy** of `src_tex` into `g_gfx->GetCurrentFramebuffer()` region `dst` (reuse the existing passthrough draw the builtin path uses, or `g_gfx`-level copy). The real frame() call arrives in Task 5. This keeps the screen correct while the engine is selectable.

- [ ] **Step 3: Override `VKGfx::CreatePostProcessor`**

In `VKGfx.cpp`:
```cpp
std::unique_ptr<VideoCommon::IPostProcessor> VKGfx::CreatePostProcessor()
{
  if (Config::Get(Config::GFX_ENHANCE_POST_PROCESS_RENDERER) ==
          PostProcessRenderer::Librashader &&
      LibrashaderPostProcessing::IsAvailable())
  {
    return std::make_unique<LibrashaderPostProcessing>();
  }
  return AbstractGfx::CreatePostProcessor();  // builtin fallback
}
```
Declare the `override` in `VKGfx.h`.

- [ ] **Step 4: Add sources to the Vulkan CMake and build**

Add `LibrashaderPostProcessing.cpp/.h` to the Vulkan target sources. Build host + `./gradlew assembleDebug`.
Expected: compiles. *TDD skip: Vulkan-backend + dlopen code is not host-unit-testable (UnitTests link VideoCommon, not backends). Fallback-when-unavailable is exercised on-device.*

- [ ] **Step 5: On-device smoke test**

Install the APK; set the renderer to librashader (via Settings once Task 6 lands, or by editing the GFX ini `PostProcessRenderer = 1` under `[Enhancements]` and pushing it via adb) with a preset selected.
Expected: game renders (passthrough — unprocessed but not black/crashed); logcat shows the instance loaded and the chain created; switching renderer back to Built-in restores the homegrown effect live.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Vulkan: LibrashaderPostProcessing skeleton + VKGfx factory (passthrough)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: Wire the per-frame `libra_vk_filter_chain_frame` path (core integration)

**Files:**
- Modify: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp` (`BlitFromTexture`)

**Interfaces:**
- Consumes: `m_chain`, `VKTexture`/`VKFramebuffer` accessors, `g_command_buffer_mgr->GetCurrentCommandBuffer()`, `StateTracker`.

- [ ] **Step 1: Build the in/out images and viewport**

In `BlitFromTexture`, if `m_chain == nullptr` fall back to the Task-4 passthrough. Otherwise:
```cpp
const auto* in_tex = static_cast<const VKTexture*>(src_tex);
libra_image_vk_t in{ in_tex->GetImage(), in_tex->GetVkFormat(),
                     in_tex->GetWidth(), in_tex->GetHeight() };
auto* out_tex = static_cast<VKTexture*>(g_gfx->GetCurrentFramebuffer()->GetColorAttachment());
libra_image_vk_t out{ out_tex->GetImage(), out_tex->GetVkFormat(),
                      out_tex->GetWidth(), out_tex->GetHeight() };
libra_viewport_t vp{ static_cast<float>(dst.left), static_cast<float>(dst.top),
                     static_cast<uint32_t>(dst.GetWidth()),
                     static_cast<uint32_t>(dst.GetHeight()) };
```

- [ ] **Step 2: Handle layouts + active render pass, then call frame()**

- End any active render pass: `StateTracker::GetInstance()->EndRenderPass();` (verify method name in `StateTracker.h`).
- Transition `in_tex` to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` and `out_tex` to `VK_IMAGE_LAYOUT_GENERAL` (or `COLOR_ATTACHMENT_OPTIMAL` — verify against the header's documented expectation for `libra_image_vk_t`) via `VKTexture::TransitionToLayout(cmd, layout)`.
- Call:
```cpp
VkCommandBuffer cmd = g_command_buffer_mgr->GetCurrentCommandBuffer();
libra_error_t err = m_instance.vk_filter_chain_frame(
    m_chain, cmd, m_frame_count++, in, out, &vp, /*mvp=*/nullptr, /*opt=*/nullptr);
if (err) { /* log via libra_error_* ; fall back to passthrough this frame */ }
```
- **Reconcile layout:** after the call, tell Dolphin's tracking what layout `out_tex` is now in (librashader leaves the output in a defined layout — verify in the header; call `out_tex->OverrideImageLayout(<that layout>)` so subsequent Dolphin rendering/StateTracker is correct).

- [ ] **Step 3: On-device acceptance test**

Install; renderer = librashader. Verify on the AYN Thor:
- **crt-royale** renders correctly (NOT too dark) — the core win.
- **RetroCrisis GDV-NTSC** renders (NOT black).
- **CRT-SatPixie** still correct.
- Live-toggle renderer Built-in ↔ librashader with no restart; Built-in reproduces the old (dark) behavior, confirming a true A/B.
Capture a note of results. *TDD skip: device-bound Vulkan glue; verified on-device.*

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "Vulkan: drive librashader filter chain per frame (real post-processing)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: Android settings UI — "Post-processing renderer" picker

**Files:**
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/model/IntSetting.kt`
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsFragmentPresenter.kt:1566` (`addEnhanceSettings`)
- Modify: `Source/Android/app/src/main/res/values/strings.xml`, `Source/Android/app/src/main/res/values/arrays.xml`

- [ ] **Step 1: Add the `IntSetting` enum entry**

In `IntSetting.kt`, mirroring `GFX_ENHANCE_FORCE_TEXTURE_FILTERING` (line 111):
```kotlin
GFX_ENHANCE_POST_PROCESS_RENDERER(
    Settings.FILE_GFX,
    Settings.SECTION_GFX_ENHANCEMENTS,
    "PostProcessRenderer",
    0
),
```
(Match the exact constructor arity used by neighboring entries.)

- [ ] **Step 2: Add string + array resources**

`strings.xml`:
```xml
<string name="post_processing_renderer">Post-processing renderer</string>
<string name="post_processing_renderer_description">Engine used for post-processing shaders. librashader applies only to the Vulkan backend.</string>
```
`arrays.xml`:
```xml
<string-array name="postProcessingRendererEntries">
    <item>Built-in</item>
    <item>librashader (Vulkan)</item>
</string-array>
<integer-array name="postProcessingRendererValues">
    <item>0</item>
    <item>1</item>
</integer-array>
```

- [ ] **Step 3: Add the `SingleChoiceSetting` row**

In `addEnhanceSettings`, immediately before the post-processing picker `RunRunnable` (line 1619):
```kotlin
sl.add(
    SingleChoiceSetting(
        context,
        IntSetting.GFX_ENHANCE_POST_PROCESS_RENDERER,
        R.string.post_processing_renderer,
        R.string.post_processing_renderer_description,
        R.array.postProcessingRendererEntries,
        R.array.postProcessingRendererValues
    )
)
```

- [ ] **Step 4: Build + on-device UI check**

`./gradlew assembleDebug`, install. Open Settings → Graphics → Enhancements.
Expected: "Post-processing renderer" row shows with Built-in/librashader; changing it live-switches the engine (Task 3 rebuild) without restart. *TDD skip: UI wiring, verified manually.*

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "Android: add Post-processing renderer setting (Built-in / librashader)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Final verification (whole-branch)

- [ ] Host unit tests green (post-processing suite unchanged).
- [ ] `./gradlew assembleDebug` clean; APK contains both `.so`s.
- [ ] On-device A/B confirmed: librashader fixes crt-royale darkening + RetroCrisis black; Built-in still selectable and unchanged; live toggle works; missing/failed `.so` falls back to Built-in without crashing.
- [ ] DolphinQt untouched (`git diff --stat master -- Source/Core/DolphinQt` is empty).
