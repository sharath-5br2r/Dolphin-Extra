# Librashader Desktop Runtimes and Single-Preset Shader UI — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make librashader the post-processing engine of record on every desktop graphics backend, drop multi-preset chains in favour of PCSX2's single-preset model, and replace the flat 30k-entry preset combo with PCSX2's tree picker and parameter editor.

**Architecture:** A `VideoCommon` librashader loader resolves the runtime-independent C API by hand (no `librashader_ld.h`), and a `VideoCommon::LibrashaderPostProcessing` base owns preset resolution, native-source downscaling, output sizing and the passthrough fallback. Each backend supplies a small `LibrashaderRuntime` implementing five hooks against its own `libra_<api>_*` symbols. The built-in `MultipassPostProcessing` executor stays as the fallback for platforms with no vendored librashader binary.

**Tech Stack:** C++20, CMake, Qt 6, librashader 0.12.0 C API (ABI 2 / API 5), Rust/cargo for the vendored library rebuild, gtest.

**Spec:** [2026-09-16-librashader-desktop-runtimes-design.md](../specs/2026-09-16-librashader-desktop-runtimes-design.md)

## Global Constraints

- **librashader is pinned to `librashader-cache-v0.12.0`, ABI 2 / API 5.** Every rebuild uses that tag. `LIBRASHADER_CURRENT_VERSION` must be assigned to every `*_opt_t::version` field.
- **Never use `librashader_ld.h`** in new code. Include plain `librashader.h`; resolve symbols through `VideoCommon::Librashader::GetSymbol`.
- **A translation unit defines at most one `LIBRA_RUNTIME_*` macro**, and only in the `.cpp`/`.mm` — never in a header.
- **The Metal adapter must be a `.mm` file**: `librashader.h` guards its Metal declarations on `defined(__OBJC__)`.
- **Never push to `upstream`** (`dolphin-emu/dolphin`). Push only to `origin`/`fork2`.
- **The Windows host (`ssh pcsx2-win`, repo `E:\work\dolphin`) is checkout-and-build only.** Never `commit`, `amend`, `rebase` or `push` there. Scratch and logs go under `E:\work\`, outside the repo.
- **Do not install software on the Windows host.** Everything needed is present: VS 18 Community, the rust toolchain at `C:\Users\Ilya\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin\`, and the librashader source at `C:\src\librashader`.
- **`ssh pcsx2-win` runs `cmd`, not a POSIX shell.** Inline `&&` chains break silently — put multi-step work in a `.bat` and `scp` it (`scp file pcsx2-win:E:/work/...`, forward slashes).
- **Never `git add -A`.** Stage named paths. Leave the pre-existing dirty ` M Externals/fmt/fmt` submodule alone.
- Every commit message ends with `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.
- Build and test on macOS: `cmake -S . -B build-qt -DUSE_SYSTEM_SDL3=OFF -DCMAKE_CXX_FLAGS= -DCMAKE_OBJCXX_FLAGS=`, then `ninja -C build-qt unittests`, then `./build-qt/Binaries/Tests/tests --gtest_filter='<Suite>.*'`. `ctest` from the build root finds nothing — `add_dolphin_test` links everything into one `tests` binary.
- `std::from_chars` for `float` is unavailable at the 11.0 deployment target. Use `std::strtof`/`std::strtol` in `VideoCommon`.
- Current test baselines: macOS arm64 **1143/1144**, Windows x64 **1455/1456** (one environment-gated skip on each). Any drop is a regression.

---

## File Structure

**New — VideoCommon:**

| File | Responsibility |
|---|---|
| `PostProcessing/LibrashaderLoader.{h,cpp}` | `dlopen`/`LoadLibraryW`, availability + reason, `GetSymbol`, runtime-independent function table, error formatting |
| `PostProcessing/LibrashaderRuntime.h` | The five-hook per-backend interface (pure virtual, no librashader runtime macros) |
| `PostProcessing/LibrashaderPostProcessing.{h,cpp}` | `IPostProcessor` implementation: preset resolution, downscale, output sizing, passthrough fallback, frame counter |
| `PostProcessing/LibrashaderParameters.{h,cpp}` | `#pragma parameter` enumeration and override parse/format/persist |
| `PostProcessing/ChainDebugDump.{h,cpp}` | One-shot PNG dump of the chain's input and output (finding 3 instrumentation) |

**New — backends:** `VideoBackends/Vulkan/VKLibrashaderRuntime.{h,cpp}`, `D3D/DXLibrashaderRuntime.{h,cpp}`, `D3D12/DX12LibrashaderRuntime.{h,cpp}`, `OGL/OGLLibrashaderRuntime.{h,cpp}`, `Metal/MTLLibrashaderRuntime.{h,mm}`.

**New — DolphinQt:** `Config/Graphics/ShaderPresetPickerDialog.{h,cpp}`, `Config/Graphics/ShaderParametersDialog.{h,cpp}`, `Config/Graphics/ShaderPresetTree.{h,cpp}` (the picker's Qt-free tree construction, so it is unit-testable).

**New — tests:** `UnitTests/VideoCommon/PostProcessing/ChainDebugDumpTest.cpp`, `LibrashaderLoaderTest.cpp`, `LibrashaderParametersTest.cpp`, and `ShaderPresetTreeTest.cpp` (Task 13 Step 1 decides where the last one lands).

**Deleted:** `VideoBackends/Vulkan/LibrashaderPostProcessing.{h,cpp}`, `VideoCommon/PostProcessing/ShaderChainSpec.{h,cpp}`, `DolphinQt/Config/Graphics/PostProcessingChainDialog.{h,cpp}`, `UnitTests/VideoCommon/PostProcessing/ShaderChainSpecTest.cpp`.

**Modified:** `VideoCommon/AbstractGfx.{h,cpp}`, each backend's `*Gfx.cpp` (`D3D12Gfx.h` too, for `InvalidateCachedState`), `DolphinQt/Config/Graphics/EnhancementsWidget.{h,cpp}`, `VideoBackends/D3D12/DX12Context.cpp`, `VideoCommon/PostProcessing/MultipassPostProcessing.cpp`, `VideoCommon/VideoConfig.{h,cpp}`, `VideoCommon/Present.cpp`, `Core/Config/GraphicsSettings.{h,cpp}`, the Android settings surface (`IntSetting.kt`, `SettingsFragmentPresenter.kt`, `strings.xml`, `arrays.xml`), `Externals/librashader/README.md`, `Externals/librashader/include/librashader_ld.h` (revert the local patch), the affected `CMakeLists.txt` files.

**Replaced binaries:** `Externals/librashader/lib/windows-x64/librashader.dll`, `Externals/librashader/lib/macos-arm64/librashader.dylib` (Task 5). The Android binaries are untouched.

**Task order rationale:** Tasks 1-2 are independent bug fixes and instrumentation that ship value immediately and unblock finding 3. Tasks 3-4 restructure the existing Vulkan path with no behaviour change, which is the safest possible way to introduce the abstraction. Task 5 rebuilds the library. Tasks 6-9 add one backend each. Tasks 10-14 are the UI and config rework; 12-14 need only Task 3 (the loader) and Task 12's runtime-independent symbols, so they can proceed in parallel with 5-9. Task 15 closes the finding-3 gate with the instrumentation Task 2 built, and Task 16 is documentation, which must come last because it records what the other fifteen actually turned out to do.

---

## Task 1: D3D12 utility root signature and loud pass-compile failures

Fixes UAT finding 1 on the built-in executor, which remains reachable as the librashader-unavailable fallback (§4.3 of the spec) and on Linux/Android x86_64.

**Files:**
- Modify: `Source/Core/VideoBackends/D3D12/DX12Context.cpp:394-409`
- Modify: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp:628`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing. Self-contained.

- [ ] **Step 1: Read both call sites**

Read `DX12Context.cpp:340-420`. `CreateGXRootSignature` uses `VideoCommon::MAX_PIXEL_SHADER_SAMPLERS` (16) for its SRV and sampler ranges; `CreateUtilityRootSignature` hardcodes `8` for both. Post-processing draws use `AbstractPipelineUsage::Utility`, so they get the 8-slot signature.

- [ ] **Step 2: Raise the utility root signature to `MAX_PIXEL_SHADER_SAMPLERS`**

In `CreateUtilityRootSignature`, replace both hardcoded `8`s (the SRV descriptor range's `NumDescriptors` and the sampler descriptor range's `NumDescriptors`) with `VideoCommon::MAX_PIXEL_SHADER_SAMPLERS`, and add a comment naming the reason:

```cpp
  // Utility draws include slang post-processing passes, whose sampler count the translator caps
  // at MAX_PIXEL_SHADER_SAMPLERS (crt-royale's mask-apply pass declares 9). Keep this in step with
  // CreateGXRootSignature or those pipelines fail to create.
```

Verify `MAX_PIXEL_SHADER_SAMPLERS` is already in scope in this file; if not, include `VideoCommon/Constants.h` (the same header `CreateGXRootSignature` gets it from).

- [ ] **Step 3: Make a failed pass pipeline loud instead of silent**

`MultipassPostProcessing.cpp:628` currently reads:

```cpp
    if (!pass.pipeline)
      continue;
```

A dropped final pass writes nothing, which is the black screen. Replace with a one-shot error that names the pass, and mark the whole chain failed so the caller falls back to the passthrough copy rather than presenting an incomplete chain:

```cpp
    if (!pass.pipeline)
    {
      // A pass whose pipeline failed to create cannot be skipped silently: if it is the final
      // pass nothing reaches the backbuffer at all (a black screen). Report it once per rebuild
      // and abandon the chain so BlitFromTexture falls through to the passthrough copy.
      if (!m_reported_pipeline_failure)
      {
        ERROR_LOG_FMT(VIDEO, "Post-processing: pass {} ('{}') has no pipeline; chain disabled",
                      pass_index, pass.name);
        m_reported_pipeline_failure = true;
      }
      return false;
    }
```

Add `bool m_reported_pipeline_failure = false;` to the class in `MultipassPostProcessing.h` and clear it wherever the chain is rebuilt. Adjust the enclosing function's signature to return `bool` if it does not already, and have `BlitFromTexture` fall through to its passthrough path when it returns false. Read the surrounding 60 lines first — the exact loop shape and the passthrough entry point must be matched, not guessed.

- [ ] **Step 4: Write the regression test**

`Source/UnitTests/VideoCommon/PostProcessing/SlangSamplersTest.cpp` already exercises sampler counting. Add a case asserting the shared constant is what both D3D12 signatures use, so the two cannot drift again:

```cpp
TEST(SlangSamplers, TranslatorCeilingMatchesBackendSamplerLimit)
{
  // The translator's MAX_SAMPLERS ceiling and the backends' descriptor-range sizes are the same
  // number by contract. D3D12's utility root signature declared 8 while the translator emitted up
  // to 16, which silently blanked crt-royale (UAT finding 1).
  EXPECT_EQ(VideoCommon::MAX_PIXEL_SHADER_SAMPLERS, 16u);
}
```

- [ ] **Step 5: Build and run the tests**

```bash
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='SlangSamplers.*'
```

Expected: PASS.

- [ ] **Step 6: Full suite**

```bash
./build-qt/Binaries/Tests/tests
```

Expected: 1143/1144 or better, one environment-gated skip.

- [ ] **Step 7: Commit**

```bash
git add Source/Core/VideoBackends/D3D12/DX12Context.cpp \
        Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp \
        Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.h \
        Source/UnitTests/VideoCommon/PostProcessing/SlangSamplersTest.cpp
git commit -m "$(cat <<'EOF'
D3D12: size the utility root signature for the slang sampler ceiling

CreateUtilityRootSignature declared 8 SRVs and 8 samplers while the slang
translator emits up to MAX_PIXEL_SHADER_SAMPLERS (16), so any pass declaring
more than 8 samplers -- crt-royale's mask-apply pass declares 9 -- failed to
create a pipeline. MultipassPostProcessing then skipped that pass silently;
when it was the final pass, nothing reached the backbuffer and the screen went
black with no diagnostic.

- Size both utility descriptor ranges from MAX_PIXEL_SHADER_SAMPLERS.
- Report a missing pass pipeline once and abandon the chain so the passthrough
  copy runs, instead of presenting a chain with a hole in it.
- Pin the shared ceiling with a test so the two signatures cannot drift.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: One-shot chain input/output PNG dump

Instrumentation for UAT finding 3. Converts "how dark does it look" into a number comparable with the photometric table in the spec (§7).

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/ChainDebugDump.h`
- Create: `Source/Core/VideoCommon/PostProcessing/ChainDebugDump.cpp`
- Create: `Source/UnitTests/VideoCommon/PostProcessing/ChainDebugDumpTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/UnitTests/VideoCommon/CMakeLists.txt`
- Modify: `Source/Core/Core/Config/GraphicsSettings.{h,cpp}`
- Modify: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace VideoCommon
  {
  // Writes `texture` to <User>/Dump/Textures/<label>-<counter>.png. Returns false and logs on
  // failure. Costs a full GPU readback, so callers must gate it on ShouldDumpChainImages().
  bool DumpChainImage(const AbstractTexture* texture, std::string_view label);
  // True only while the config flag is set and the per-run budget is not exhausted.
  bool ShouldDumpChainImages();
  // Decrements the budget. Called once per frame that dumped, not once per image.
  void NoteChainImagesDumped();
  }
  ```

- [ ] **Step 1: Write the failing test for the budget**

`Source/UnitTests/VideoCommon/PostProcessing/ChainDebugDumpTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Core/Config/GraphicsSettings.h"
#include "VideoCommon/PostProcessing/ChainDebugDump.h"

TEST(ChainDebugDump, DisabledByDefault)
{
  EXPECT_FALSE(VideoCommon::ShouldDumpChainImages());
}

TEST(ChainDebugDump, BudgetIsSpentAfterOneFrame)
{
  Config::SetCurrent(Config::GFX_LIBRASHADER_DUMP_CHAIN_IMAGES, true);
  EXPECT_TRUE(VideoCommon::ShouldDumpChainImages());
  VideoCommon::NoteChainImagesDumped();
  // A single frame is enough evidence, and a readback per frame would be unusable. The flag
  // stays set so a config reload is not needed, but the budget is gone.
  EXPECT_FALSE(VideoCommon::ShouldDumpChainImages());
  Config::DeleteKey(Config::LayerType::CurrentRun, Config::GFX_LIBRASHADER_DUMP_CHAIN_IMAGES);
}
```

Read an existing test in that directory first to copy the exact `Config::SetCurrent` / cleanup idiom this tree uses; do not invent one.

- [ ] **Step 2: Register the test target and run it to see it fail**

Append to `Source/UnitTests/VideoCommon/CMakeLists.txt`:

```cmake
add_dolphin_test(ChainDebugDumpTest PostProcessing/ChainDebugDumpTest.cpp)
```

```bash
ninja -C build-qt unittests
```

Expected: FAIL to compile — `ChainDebugDump.h` does not exist.

- [ ] **Step 3: Add the config key**

In `Source/Core/Core/Config/GraphicsSettings.h`, beside the existing `GFX_LIBRASHADER_DYNAMIC_RENDERING`:

```cpp
extern const Info<bool> GFX_LIBRASHADER_DUMP_CHAIN_IMAGES;
```

In `GraphicsSettings.cpp`, beside the same neighbour:

```cpp
// Debug aid for the "crt-royale renders dark" investigation: dumps the images entering and
// leaving the librashader chain, once, so their means can be compared against a reference
// render. Deliberately not exposed in the UI.
const Info<bool> GFX_LIBRASHADER_DUMP_CHAIN_IMAGES{
    {System::GFX, "Settings", "LibrashaderDumpChainImages"}, false};
```

- [ ] **Step 4: Implement the module**

`ChainDebugDump.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

class AbstractTexture;

namespace VideoCommon
{
// True while GFX_LIBRASHADER_DUMP_CHAIN_IMAGES is set and this run has not dumped yet.
bool ShouldDumpChainImages();

// Spends the one-frame budget. Call once per dumping frame, after the images are written.
void NoteChainImagesDumped();

// Writes `texture` as PNG under <User>/Dump/Textures/. Synchronous full readback; only call
// behind ShouldDumpChainImages().
bool DumpChainImage(const AbstractTexture* texture, std::string_view label);
}  // namespace VideoCommon
```

`ChainDebugDump.cpp` — use `AbstractTexture::Save`, which already exists for texture dumping (read its declaration in `AbstractTexture.h` and match the signature exactly rather than assuming):

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/ChainDebugDump.h"

#include <atomic>
#include <string>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/Config/GraphicsSettings.h"
#include "VideoCommon/AbstractTexture.h"

namespace VideoCommon
{
namespace
{
// One frame's worth of evidence, not a stream: a readback per frame stalls the GPU and fills the
// disk. Reset only by restarting, which is what the flag's single use (a UAT capture) needs.
std::atomic<bool> s_budget_spent{false};
}  // namespace

bool ShouldDumpChainImages()
{
  return Config::Get(Config::GFX_LIBRASHADER_DUMP_CHAIN_IMAGES) && !s_budget_spent.load();
}

void NoteChainImagesDumped()
{
  s_budget_spent.store(true);
}

bool DumpChainImage(const AbstractTexture* texture, std::string_view label)
{
  if (texture == nullptr)
    return false;

  const std::string path =
      fmt::format("{}librashader-{}.png", File::GetUserPath(D_DUMPTEXTURES_IDX), label);
  if (!texture->Save(path, 0))
  {
    ERROR_LOG_FMT(VIDEO, "Librashader: failed to dump chain image to '{}'", path);
    return false;
  }
  INFO_LOG_FMT(VIDEO, "Librashader: dumped chain image {}x{} to '{}'", texture->GetWidth(),
               texture->GetHeight(), path);
  return true;
}
}  // namespace VideoCommon
```

Register both sources in `Source/Core/VideoCommon/CMakeLists.txt` next to the other `PostProcessing/` entries.

- [ ] **Step 5: Run the test to verify it passes**

```bash
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='ChainDebugDump.*'
```

Expected: PASS, 2 tests.

- [ ] **Step 6: Wire it into the existing Vulkan librashader path**

In `LibrashaderPostProcessing::BlitFromTexture`, inside the `m_chain != nullptr` branch: dump `source` before `run_chain`, dump the chain target after a successful `run_chain`, then `NoteChainImagesDumped()`. Both dumps are behind one `ShouldDumpChainImages()` check so input and output always come from the same frame:

```cpp
    const bool dump_images = VideoCommon::ShouldDumpChainImages();
    if (dump_images)
      VideoCommon::DumpChainImage(source, "chain-input");
```

and, after the successful branch has produced its output texture:

```cpp
    if (dump_images)
    {
      VideoCommon::DumpChainImage(chain_output, "chain-output");
      VideoCommon::NoteChainImagesDumped();
    }
```

For the direct-to-backbuffer branch the output is the backbuffer, which cannot be read back reliably; log that the output dump was skipped and name the setting that forces the intermediate path (`ShouldRenderChainDirectly` returns false whenever the draw rect is not the whole backbuffer, so windowed non-fullscreen play produces both images). Do not silently produce only an input dump.

- [ ] **Step 7: Full suite, then commit**

```bash
./build-qt/Binaries/Tests/tests
git add Source/Core/VideoCommon/PostProcessing/ChainDebugDump.h \
        Source/Core/VideoCommon/PostProcessing/ChainDebugDump.cpp \
        Source/Core/VideoCommon/CMakeLists.txt \
        Source/Core/Core/Config/GraphicsSettings.h \
        Source/Core/Core/Config/GraphicsSettings.cpp \
        Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/ChainDebugDumpTest.cpp \
        Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "$(cat <<'EOF'
VideoCommon: add a one-shot librashader chain image dump

The reported crt-royale darkening on Windows cannot be attributed without
pixels from a running game: a librashader CLI probe puts crt-royale at ~68-70%
of input mean at 1080p in both Dolphin's and PCSX2's source-resolution regimes,
so the remaining candidates are the chain's input, its output handling, or no
defect at all. Dolphin's own screenshot path dumps the pre-post-processing XFB
and cannot answer this.

- LibrashaderDumpChainImages (GFX.ini, no UI) dumps the images entering and
  leaving the chain once per run, then spends its budget.
- The Vulkan adapter dumps both from the same frame, and says so when the
  direct-to-backbuffer path makes the output unreadable.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: `VideoCommon::Librashader` loader (replaces `librashader_ld.h`)

`librashader_ld.h` declares one `libra_instance_t` whose members are gated by `LIBRA_RUNTIME_*` and a `static inline librashader_load_instance()`. Two TUs including it with different macro sets get two incompatible structs under one name, and a single TU would have to include `vulkan.h`, `d3d11.h`, `d3d12.h`, the GL headers and the Metal headers together. PCSX2 solves this by resolving symbols by hand; adopt its shape.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/LibrashaderLoader.h`
- Create: `Source/Core/VideoCommon/PostProcessing/LibrashaderLoader.cpp`
- Create: `Source/UnitTests/VideoCommon/PostProcessing/LibrashaderLoaderTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/UnitTests/VideoCommon/CMakeLists.txt`
- Modify: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp` (switch to the loader)
- Modify: `Externals/librashader/include/librashader_ld.h` (revert the local `_LIBRASHADER_LOAD` patch), `Externals/librashader/README.md`

**Reference:** `~/work/pcsx2/pcsx2/GS/ShaderChain/LibrashaderLoader.h` — read it in full before starting.

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace VideoCommon::Librashader
  {
  struct Availability { bool available = false; std::string reason; };
  struct CommonFunctions
  {
    PFN_libra_instance_abi_version abi_version = nullptr;
    PFN_libra_instance_api_version api_version = nullptr;
    PFN_libra_error_errno error_errno = nullptr;
    PFN_libra_error_write error_write = nullptr;
    PFN_libra_error_free_string error_free_string = nullptr;
    PFN_libra_error_free error_free = nullptr;
    PFN_libra_preset_create preset_create = nullptr;
    PFN_libra_preset_free preset_free = nullptr;
    PFN_libra_preset_get_runtime_params preset_get_runtime_params = nullptr;
    PFN_libra_preset_free_runtime_params preset_free_runtime_params = nullptr;
  };
  const Availability& GetAvailability();     // loads once, thread-safe
  const CommonFunctions& Common();           // valid only when GetAvailability().available
  void* GetSymbol(const char* name);         // nullptr when unavailable or absent
  std::string DescribeAndFreeError(libra_error_t error);   // "" for nullptr
  std::string LibraryPath();                 // packaged absolute path
  Availability LoadFromPath(const std::string& path);      // test seam, no global state
  }
  ```

- [ ] **Step 1: Write the failing tests**

`Source/UnitTests/VideoCommon/PostProcessing/LibrashaderLoaderTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

namespace Librashader = VideoCommon::Librashader;

TEST(LibrashaderLoader, MissingLibraryReportsWhyRatherThanCrashing)
{
  const Librashader::Availability availability =
      Librashader::LoadFromPath("/nonexistent/librashader-does-not-exist.dylib");
  EXPECT_FALSE(availability.available);
  // The reason is what a user sees in the log when post-processing silently degrades, so it must
  // not be empty and must name the path that was tried.
  EXPECT_FALSE(availability.reason.empty());
  EXPECT_NE(availability.reason.find("librashader-does-not-exist"), std::string::npos);
}

TEST(LibrashaderLoader, LibraryPathIsAbsolute)
{
  const std::string path = Librashader::LibraryPath();
  ASSERT_FALSE(path.empty());
  // The bare-name load that librashader_ld.h defaults to cannot find a library inside a macOS
  // .app bundle or a Windows install directory; the loader must always use an absolute path.
  EXPECT_EQ(path.front(), '/') << "path was: " << path;
}

TEST(LibrashaderLoader, DescribeAndFreeErrorAcceptsNull)
{
  EXPECT_TRUE(Librashader::DescribeAndFreeError(nullptr).empty());
}

TEST(LibrashaderLoader, GetSymbolIsNullForAnAbsentName)
{
  // Safe whether or not the vendored library is present in the test environment: no library
  // exports this name.
  EXPECT_EQ(Librashader::GetSymbol("libra_this_symbol_does_not_exist"), nullptr);
}
```

Guard the `LibraryPathIsAbsolute` assertion for Windows (`path.front()` is a drive letter there) with the same `#ifdef _WIN32` idiom used elsewhere in `Source/UnitTests`; check one existing test for the house style first.

- [ ] **Step 2: Register the target and run to see it fail**

```cmake
add_dolphin_test(LibrashaderLoaderTest PostProcessing/LibrashaderLoaderTest.cpp)
```

```bash
ninja -C build-qt unittests
```

Expected: FAIL to compile — `LibrashaderLoader.h` does not exist.

- [ ] **Step 3: Write the header**

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Runtime-independent part of the librashader C API. With no LIBRA_RUNTIME_* macro defined this
// header pulls in nothing but the C standard library, so it is safe in VideoCommon. Each backend
// defines its own LIBRA_RUNTIME_* macro in its own .cpp/.mm and resolves its chain functions
// through GetSymbol(); librashader_ld.h is deliberately unused, because its single
// libra_instance_t is macro-gated and its loader is static inline, so two translation units with
// different runtime sets would disagree on the struct's layout.
#include <librashader.h>

#include <string>

namespace VideoCommon::Librashader
{
struct Availability
{
  bool available = false;
  // Human-readable failure cause, for the log line the user will be asked about. Empty on success.
  std::string reason;
};

struct CommonFunctions
{
  PFN_libra_instance_abi_version abi_version = nullptr;
  PFN_libra_instance_api_version api_version = nullptr;
  PFN_libra_error_errno error_errno = nullptr;
  PFN_libra_error_write error_write = nullptr;
  PFN_libra_error_free_string error_free_string = nullptr;
  PFN_libra_error_free error_free = nullptr;
  PFN_libra_preset_create preset_create = nullptr;
  PFN_libra_preset_free preset_free = nullptr;
  PFN_libra_preset_get_runtime_params preset_get_runtime_params = nullptr;
  PFN_libra_preset_free_runtime_params preset_free_runtime_params = nullptr;
};

// Loads the library on first call and caches the result. Thread-safe.
const Availability& GetAvailability();

// Runtime-independent entry points. Only valid when GetAvailability().available.
const CommonFunctions& Common();

// Raw symbol lookup for a backend's libra_<api>_* functions. nullptr when unavailable or absent.
void* GetSymbol(const char* name);

// Formats a libra_error_t and frees it. Returns "" for nullptr, so it doubles as a success test.
std::string DescribeAndFreeError(libra_error_t error);

// Absolute path to the packaged library: next to the executable on Windows,
// Contents/Frameworks inside the bundle on macOS, the bare soname on Android and Linux.
std::string LibraryPath();

// Loads from an explicit path without touching the cached global. Test seam.
Availability LoadFromPath(const std::string& path);
}  // namespace VideoCommon::Librashader
```

- [ ] **Step 4: Write the implementation**

Key requirements, each of which the current `LibrashaderPostProcessing.cpp` or PCSX2's loader already establishes:

1. `LoadFromPath` uses `LoadLibraryW(UTF8ToWString(path))` on Windows and `dlopen(path, RTLD_LAZY)` elsewhere. On failure the reason includes the path *and* the platform's error text (`GetLastError()` / `dlerror()`).
2. On success, verify `libra_instance_abi_version()` returns `2` and `libra_instance_api_version()` returns at least `5`; a mismatch is a failure with the observed numbers in `reason`, not a crash later.
3. Every `CommonFunctions` member is required. A missing one is a load failure naming the symbol — unlike `librashader_ld.h`, do not substitute silent no-op stubs, because that is what turns a broken library into an unexplained passthrough.
4. `GetAvailability()` wraps a function-local `static const Availability`, which C++ guarantees is initialised exactly once.
5. `DescribeAndFreeError` calls `error_write` to get a string, copies it, then `error_free_string` and `error_free`. Falls back to `fmt::format("errno {}", error_errno(error))` if `error_write` fails. Returns `""` for `nullptr`.
6. `LibraryPath()`: `File::GetExeDirectory()` + `"librashader.dll"` on Windows; the bundle's `Contents/Frameworks/librashader.dylib` on macOS (reuse whatever the existing `VideoCommon::LibrashaderLibraryPath()` does — read `LibrashaderLibrary.cpp` and move that logic here rather than reimplementing it); `"librashader.so"` on Android and Linux.

Delete `VideoCommon/PostProcessing/LibrashaderLibrary.{h,cpp}` once its logic has moved, and drop it from `CMakeLists.txt`.

- [ ] **Step 5: Run the tests**

```bash
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='LibrashaderLoader.*'
```

Expected: PASS, 4 tests.

- [ ] **Step 6: Port the Vulkan adapter onto the loader, with no behaviour change**

In `VideoBackends/Vulkan/LibrashaderPostProcessing.cpp`:

- Delete the `_LIBRASHADER_LOAD` `#define` block (`:34-43`) and the `#include <librashader_ld.h>` (`:48-49`).
- Add `#define LIBRA_RUNTIME_VULKAN` then `#include <librashader.h>`, keeping the existing include-order comment about `VulkanContext.h` coming first — it still applies, because `librashader.h` re-includes `<vulkan/vulkan.h>`.
- Replace `GetLibrashaderInstance()` with a file-local struct resolved once:

```cpp
namespace
{
// Vulkan chain entry points, resolved once. Absent symbols leave the pointers null, which
// IsAvailable() reports rather than calling through.
struct VulkanFunctions
{
  PFN_libra_vk_filter_chain_create create = nullptr;
  PFN_libra_vk_filter_chain_frame frame = nullptr;
  PFN_libra_vk_filter_chain_set_param set_param = nullptr;
  PFN_libra_vk_filter_chain_free free = nullptr;

  VulkanFunctions()
  {
    using VideoCommon::Librashader::GetSymbol;
    create = reinterpret_cast<PFN_libra_vk_filter_chain_create>(
        GetSymbol("libra_vk_filter_chain_create"));
    frame = reinterpret_cast<PFN_libra_vk_filter_chain_frame>(
        GetSymbol("libra_vk_filter_chain_frame"));
    set_param = reinterpret_cast<PFN_libra_vk_filter_chain_set_param>(
        GetSymbol("libra_vk_filter_chain_set_param"));
    free = reinterpret_cast<PFN_libra_vk_filter_chain_free>(
        GetSymbol("libra_vk_filter_chain_free"));
  }

  bool Complete() const { return create && frame && set_param && free; }
};

const VulkanFunctions& Functions()
{
  static const VulkanFunctions s_functions;
  return s_functions;
}
}  // namespace
```

- `IsAvailable()` becomes `GetAvailability().available && Functions().Complete()`.
- `CheckError` delegates to `DescribeAndFreeError` and logs the returned string, which is strictly more informative than today's bare errno.
- `lib.preset_create` becomes `VideoCommon::Librashader::Common().preset_create`; `lib.vk_filter_chain_*` become `Functions().*`.

- [ ] **Step 7: Revert the vendored header patch**

`Externals/librashader/include/librashader_ld.h` no longer has a consumer. Restore the three `#ifndef _LIBRASHADER_LOAD` wrappers to upstream (lines 55-57, 66-68, 77-79 per `Externals/librashader/README.md:96-98`) and delete the "Local Modifications" section's `_LIBRASHADER_LOAD` paragraph from the README, replacing it with a note that the header is vendored for reference only and that Dolphin resolves symbols itself.

- [ ] **Step 8: Full suite, build the Vulkan backend, commit**

```bash
ninja -C build-qt unittests dolphin-emu && ./build-qt/Binaries/Tests/tests
```

Expected: 1147/1148 (four new tests), one environment-gated skip.

```bash
git add Source/Core/VideoCommon/PostProcessing/LibrashaderLoader.h \
        Source/Core/VideoCommon/PostProcessing/LibrashaderLoader.cpp \
        Source/Core/VideoCommon/CMakeLists.txt \
        Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/LibrashaderLoaderTest.cpp \
        Source/UnitTests/VideoCommon/CMakeLists.txt \
        Externals/librashader/include/librashader_ld.h \
        Externals/librashader/README.md
git rm Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.h \
       Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.cpp
git commit -m "$(cat <<'EOF'
VideoCommon: resolve the librashader C API without librashader_ld.h

librashader_ld.h declares a single libra_instance_t whose members are gated by
LIBRA_RUNTIME_* macros and a static inline loader, so two translation units
including it with different runtime sets disagree on the struct's layout, and a
single translation unit would have to include the Vulkan, D3D11, D3D12, GL and
Metal headers together. That blocks per-backend runtimes.

- New VideoCommon::Librashader loader: one dlopen/LoadLibraryW, ABI 2 / API 5
  verification, a runtime-independent function table, and GetSymbol() so each
  backend resolves only its own libra_<api>_* entry points.
- A missing symbol or a version mismatch is now a load failure with a reason,
  instead of librashader_ld.h's silent no-op stubs.
- The Vulkan adapter moves onto the loader with no behaviour change; the local
  _LIBRASHADER_LOAD patch to the vendored header is reverted.
- LibrashaderLibrary is folded into the loader.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Hoist the orchestration into VideoCommon behind a five-hook runtime interface

Pure restructuring: after this task Vulkan behaves exactly as before, but the orchestration lives in `VideoCommon` and the Vulkan-specific part is a 200-line adapter. Tasks 6-9 then add one file each.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/LibrashaderRuntime.h`
- Create: `Source/Core/VideoCommon/PostProcessing/LibrashaderPostProcessing.{h,cpp}`
- Create: `Source/Core/VideoBackends/Vulkan/VKLibrashaderRuntime.{h,cpp}`
- Delete: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.{h,cpp}`
- Modify: `Source/Core/VideoBackends/Vulkan/VKGfx.cpp`, `Source/Core/VideoBackends/Vulkan/CMakeLists.txt`, `Source/Core/VideoCommon/CMakeLists.txt`

**Interfaces:**
- Consumes: `VideoCommon::Librashader::{GetAvailability, Common, GetSymbol, DescribeAndFreeError}` (Task 3); `VideoCommon::{ShouldDumpChainImages, DumpChainImage, NoteChainImagesDumped}` (Task 2).
- Produces:
  ```cpp
  namespace VideoCommon
  {
  class LibrashaderRuntime
  {
  public:
    virtual ~LibrashaderRuntime();
    virtual bool IsSupported() const = 0;
    virtual bool CreateChain(libra_shader_preset_t preset) = 0;   // consumes `preset` always
    virtual void DestroyChain() = 0;
    virtual bool HasChain() const = 0;
    virtual bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                          u64 frame_count) = 0;
    virtual void SetParameter(const char* name, float value) = 0;
    virtual void DiscardPendingTargetClear() {}
  };

  class LibrashaderPostProcessing final : public IPostProcessor
  {
  public:
    explicit LibrashaderPostProcessing(std::unique_ptr<LibrashaderRuntime> runtime);
    // ... IPostProcessor overrides
  };

  // Resolves a preset name to an absolute .slangp using MultipassPostProcessing's search order.
  std::string ResolvePresetPath(const std::string& preset_name);
  }
  ```

- [ ] **Step 1: Read the file being restructured, end to end**

Read all 523 lines of `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp`. Every comment in it records a decision that cost measurement to reach — the native-source downscale rationale (`:410-420`), the output-sizing rationale (`:439-453`), the `frames_in_flight` reasoning (`:162-166`), the preset-handle ownership note (`:177-179`). **Carry each comment to wherever its code lands.** Losing them re-opens settled questions.

- [ ] **Step 2: Write the runtime interface header**

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"

class AbstractFramebuffer;
class AbstractTexture;

namespace VideoCommon
{
// One graphics backend's binding to a librashader native runtime.
//
// LibrashaderPostProcessing owns everything that is the same on every backend: preset
// resolution, the native-resolution source downscale, output sizing, the frame counter and the
// passthrough fallback. A runtime only has to create and free a chain and record one frame.
//
// The target is an AbstractFramebuffer rather than an AbstractTexture because D3D11's chain
// entry point takes an ID3D11RenderTargetView, and in Dolphin the RTV lives on DXFramebuffer
// while the SRV lives on DXTexture. Every other backend can reach its texture through
// GetColorAttachment().
class LibrashaderRuntime
{
public:
  virtual ~LibrashaderRuntime();

  // True when every libra_<api>_* symbol this runtime needs resolved. Checked before a chain is
  // built, so an incomplete library degrades to the built-in executor rather than to a black
  // screen.
  virtual bool IsSupported() const = 0;

  // Builds a chain from `preset`. librashader invalidates the preset handle on BOTH success and
  // failure ("the shader preset is immediately invalidated"), so implementations must never free
  // it -- that would be a double free. Returns false and logs on failure.
  virtual bool CreateChain(libra_shader_preset_t preset) = 0;
  virtual void DestroyChain() = 0;
  virtual bool HasChain() const = 0;

  // Records the whole chain, reading `source` and writing `target` over its full extent with the
  // viewport at (0,0). Called outside any render pass. Implementations must transition `source`
  // to shader-read and `target` to render-target themselves, and must leave Dolphin's own
  // pipeline, descriptor and layout tracking consistent afterwards -- librashader binds its own
  // state and does not restore Dolphin's.
  virtual bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                        u64 frame_count) = 0;

  // Pushes a #pragma parameter override into the live chain. No-op without a chain.
  virtual void SetParameter(const char* name, float value) = 0;

  // Called before RunFrame() when the chain writes straight into the backbuffer, so a backend
  // with a deferred clear can drop it: the chain's final pass covers every pixel.
  virtual void DiscardPendingTargetClear() {}
};
}  // namespace VideoCommon
```

- [ ] **Step 3: Move the backend-agnostic code into `LibrashaderPostProcessing`**

Transplanted from the Vulkan file with their comments intact:

| From | Change |
|---|---|
| `ResolvePresetPath` (`:78`) | Becomes free function `VideoCommon::ResolvePresetPath`. Drop the `substr(0, find(';'))` chain truncation and its comment — Task 11 makes the setting single-preset, and until then the first entry is still the right answer, so keep the truncation but add a `WARN_LOG_FMT` naming the dropped entries. |
| `BuildPassthroughPipeline` (`:198`) | Verbatim; `g_gfx` only. |
| `BuildDownscalePipeline` (`:244`) | Verbatim; `g_gfx` only. |
| `DownscaleToNativeSource` (`:327`) | Verbatim minus its Vulkan tail (`:368-372`): drop the `EndRenderPass` + `TransitionToLayout`, because `RunFrame` now performs both transitions. Return `AbstractTexture*`. |
| `EnsureOutputTarget` (`:375`) | Now also creates and caches an `AbstractFramebuffer` for the target and returns it, since `RunFrame` takes a framebuffer. |
| `BlitFromTexture` (`:392`) | Orchestration verbatim; the four Vulkan-specific statements become `m_runtime->` calls. |
| `Initialize`, `RecompileShader`, `RecompilePipeline`, `IsAvailable` | Structure verbatim; chain create/free go through `m_runtime`. |

`RecompileShader` becomes:

```cpp
void LibrashaderPostProcessing::RecompileShader()
{
  m_runtime->DestroyChain();
  m_frame_count = 0;

  if (!m_available)
    return;

  const std::string preset_name = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  const std::string path = ResolvePresetPath(preset_name);
  if (path.empty())
  {
    if (!preset_name.empty())
      WARN_LOG_FMT(VIDEO, "Librashader: preset '{}' not found; falling back to passthrough",
                   preset_name);
    return;
  }

  libra_shader_preset_t preset = nullptr;
  const std::string error =
      Librashader::DescribeAndFreeError(Librashader::Common().preset_create(path.c_str(), &preset));
  if (!error.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Librashader: preset_create('{}') failed: {}", path, error);
    return;
  }

  // CreateChain consumes `preset` on every path, so there is nothing to free here.
  if (m_runtime->CreateChain(preset))
    INFO_LOG_FMT(VIDEO, "Librashader: filter chain created from '{}'", path);
}
```

and the `run_chain` lambda in `BlitFromTexture` collapses to a `m_runtime->RunFrame(source, target_fb, m_frame_count++)` call, keeping the dump hooks from Task 2 around it.

- [ ] **Step 4: Write the Vulkan adapter**

`VKLibrashaderRuntime.cpp` defines `LIBRA_RUNTIME_VULKAN`, includes `VulkanContext.h` before `librashader.h` (keep the existing include-order comment), and holds the `VulkanFunctions` table from Task 3 plus:

- `CreateChain`: builds `libra_device_vk_t` from `g_vulkan_context` and `filter_chain_vk_opt_t` exactly as `LibrashaderPostProcessing.cpp:152-185` does today, comments included (`frames_in_flight = 0`, `ChooseDynamicRendering`, the preset-invalidation note).
- `RunFrame`: `StateTracker::EndRenderPass()`, transition source to `SHADER_READ_ONLY_OPTIMAL` and target's colour attachment to `COLOR_ATTACHMENT_OPTIMAL`, build the two `libra_image_vk_t` and the `libra_viewport_t` from the target's dimensions, call `frame`, then `OverrideImageLayout(COLOR_ATTACHMENT_OPTIMAL)` on the target. Carry the comment explaining that librashader creates no barrier after its final pass.
- `DiscardPendingTargetClear`: `StateTracker::GetInstance()->DiscardPendingClear();`
- `SetParameter`: `set_param(&m_chain, name, value)`, error logged through `DescribeAndFreeError`.

- [ ] **Step 5: Update `VKGfx::CreatePostProcessor`**

```cpp
std::unique_ptr<VideoCommon::IPostProcessor> VKGfx::CreatePostProcessor()
{
  auto runtime = std::make_unique<VKLibrashaderRuntime>();
  if (runtime->IsSupported())
    return std::make_unique<VideoCommon::LibrashaderPostProcessing>(std::move(runtime));
  return AbstractGfx::CreatePostProcessor();
}
```

Leave the `GFX_ENHANCE_POST_PROCESS_RENDERER` check in place for now; Task 10 removes it. This keeps Task 4 a pure restructuring.

- [ ] **Step 6: Update both `CMakeLists.txt` files and build**

```bash
ninja -C build-qt unittests dolphin-emu
```

Expected: clean build. There is no new unit test here — the moved code is GPU-bound and was untested before; its behaviour is verified by the existing tests that cover `PlanSlangSourceDownscale` and `ShouldRenderChainDirectly`, plus the manual smoke in Step 7.

- [ ] **Step 7: Verify no behaviour change on device**

Run a game on the Vulkan backend with crt-royale selected and confirm the picture is identical to before the task (same geometry, same brightness) and that the log still reports the chain creation and the dynamic-rendering state. **If this cannot be run, say so in the task report rather than claiming it passed** — this is the only check that catches a transplant error.

- [ ] **Step 8: Full suite, then commit**

```bash
./build-qt/Binaries/Tests/tests
git add Source/Core/VideoCommon/PostProcessing/LibrashaderRuntime.h \
        Source/Core/VideoCommon/PostProcessing/LibrashaderPostProcessing.h \
        Source/Core/VideoCommon/PostProcessing/LibrashaderPostProcessing.cpp \
        Source/Core/VideoCommon/CMakeLists.txt \
        Source/Core/VideoBackends/Vulkan/VKLibrashaderRuntime.h \
        Source/Core/VideoBackends/Vulkan/VKLibrashaderRuntime.cpp \
        Source/Core/VideoBackends/Vulkan/VKGfx.cpp \
        Source/Core/VideoBackends/Vulkan/CMakeLists.txt
git rm Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.h \
       Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp
git commit -m "$(cat <<'EOF'
VideoCommon: move the librashader orchestration out of the Vulkan backend

Everything the librashader post-processor does apart from five calls is
backend-agnostic: preset resolution, the native-resolution source downscale,
draw-rect output sizing, the frame counter and the passthrough fallback. It
lived in the Vulkan backend only because Vulkan was the only runtime.

- VideoCommon::LibrashaderPostProcessing now owns the orchestration and
  IPostProcessor implementation.
- LibrashaderRuntime is the per-backend seam: IsSupported, CreateChain,
  DestroyChain, RunFrame, SetParameter, plus an optional deferred-clear discard.
- RunFrame takes an AbstractFramebuffer, not a texture, because D3D11's entry
  point needs a render target view, which in Dolphin lives on the framebuffer.
- VKLibrashaderRuntime is the first implementation; no behaviour change.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Rebuild the vendored librashader with the desktop runtimes

**Files:**
- Replace: `Externals/librashader/lib/windows-x64/librashader.dll`, `Externals/librashader/lib/macos-arm64/librashader.dylib`
- Modify: `Externals/librashader/README.md`

**Interfaces:**
- Consumes: nothing.
- Produces: binaries exporting `libra_d3d11_*`, `libra_d3d12_*`, `libra_gl_*` (Windows) and `libra_mtl_*`, `libra_gl_*` (macOS) in addition to `libra_vk_*`. Tasks 6-9 each depend on the corresponding family being present.

This is a mechanical build-and-vendor task, not TDD; the verification is a symbol audit.

- [ ] **Step 1: Build the Windows DLL**

The source tree is already at `C:\src\librashader` on the `librashader-cache-v0.12.0` tag, and `C:\src\build-librashader.bat` is the working recipe. Write a new script rather than editing that one, and `scp` it (the ssh shell is `cmd`, so an inline `&&` chain will not survive):

```batch
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo VCVARS_FAILED & exit /b 1)
set "RUSTC=C:\Users\Ilya\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin\rustc.exe"
cd /d C:\src\librashader || (echo NO_SRC & exit /b 1)
"C:\Users\Ilya\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin\cargo.exe" build ^
  -p librashader-capi --release --target x86_64-pc-windows-msvc ^
  --no-default-features ^
  --features runtime-vulkan,runtime-d3d11,runtime-d3d12-static,runtime-opengl
echo CARGO_EXIT=%ERRORLEVEL%
```

`runtime-d3d12-static` rather than `runtime-d3d12` is deliberate: `librashader-runtime-d3d12/src/util.rs:233-252` selects `mach_dxcompiler_rs::DxcCreateInstance` under `feature = "static"` and the `dxcompiler.dll` import otherwise. Static linking means nothing extra to package.

```bash
scp /tmp/build-lr-desktop.bat pcsx2-win:E:/work/build-lr-desktop.bat
ssh pcsx2-win "E:\\work\\build-lr-desktop.bat > E:\\work\\build-lr-desktop.log 2>&1"
ssh pcsx2-win "type E:\\work\\build-lr-desktop.log"
```

Expected: `CARGO_EXIT=0`. If `runtime-opengl` fails to build, drop it, note the failure in the task report, and let Task 8 be skipped — the §4.3 fallback rule covers a backend with no runtime.

- [ ] **Step 2: Audit the Windows exports before vendoring**

```bash
ssh pcsx2-win "powershell -Command \"& { \$p='C:\\src\\librashader\\target\\x86_64-pc-windows-msvc\\release\\librashader_capi.dll'; (Get-Item \$p).Length }\""
```

Then copy it back and check the symbol families locally:

```bash
scp pcsx2-win:C:/src/librashader/target/x86_64-pc-windows-msvc/release/librashader_capi.dll /tmp/librashader.dll
for f in vk d3d11 d3d12 gl; do
  printf "%-6s %s\n" "$f" "$(strings -a /tmp/librashader.dll | grep -c "^libra_${f}_filter_chain_")"
done
strings -a /tmp/librashader.dll | grep -c "dxcompiler"
```

Expected: a non-zero count for `vk`, `d3d11`, `d3d12` and `gl`, and **0** for `dxcompiler` (the static DXC is linked, not imported). Record the counts in the task report; a zero for a family means that backend's task cannot proceed.

- [ ] **Step 3: Vendor the Windows DLL**

```bash
cp /tmp/librashader.dll Externals/librashader/lib/windows-x64/librashader.dll
ls -l Externals/librashader/lib/windows-x64/librashader.dll
```

- [ ] **Step 4: Build and vendor the macOS dylib**

```bash
git clone --branch librashader-cache-v0.12.0 --depth 1 \
    https://github.com/SnowflakePowered/librashader /tmp/librashader
cd /tmp/librashader && cargo build -p librashader-capi --release \
    --target aarch64-apple-darwin --no-default-features \
    --features runtime-vulkan,runtime-metal,runtime-opengl
```

`runtime-metal` pulls `__cbindgen_internal_objc`; if it fails to compile, retry without it and report — Task 9 then does not proceed. Same for `runtime-opengl` and Task 8.

```bash
cd ~/work/dolphin
cp /tmp/librashader/target/aarch64-apple-darwin/release/liblibrashader_capi.dylib \
   Externals/librashader/lib/macos-arm64/librashader.dylib
for f in vk mtl gl; do
  printf "%-4s %s\n" "$f" "$(nm -gU Externals/librashader/lib/macos-arm64/librashader.dylib \
    | grep -c "_libra_${f}_filter_chain_")"
done
```

- [ ] **Step 5: Verify the ABI is unchanged**

Both binaries must still report ABI 2 / API 5, because the headers in `Externals/librashader/include/` are not being regenerated. The loader from Task 3 checks this at runtime; confirm it here by running Dolphin's unit suite (which links the loader) and by a Vulkan smoke test on each platform.

```bash
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='LibrashaderLoader.*'
```

- [ ] **Step 6: Update the README's build provenance**

Rewrite the "Desktop Builds" section with the new feature strings, the new artifact sizes, the `runtime-d3d12-static`/`dxcompiler` reasoning from Step 1, and the symbol-family counts from Steps 2 and 4. State plainly which runtimes are present per platform, since Tasks 6-9 and the §4.3 fallback rule depend on it. Keep the Android section unchanged (`runtime-vulkan` only) and keep the existing note about the dylib's `LC_ID_DYLIB` install name.

- [ ] **Step 7: Commit**

```bash
git add Externals/librashader/lib/windows-x64/librashader.dll \
        Externals/librashader/lib/macos-arm64/librashader.dylib \
        Externals/librashader/README.md
git commit -m "$(cat <<'EOF'
Externals: rebuild librashader with the desktop native runtimes

The vendored binaries were built --features runtime-vulkan, so librashader
could only drive Dolphin's Vulkan backend and every other desktop backend fell
back to the built-in slang executor -- which is where the D3D12 black screen
came from.

- Windows x64: runtime-vulkan, runtime-d3d11, runtime-d3d12-static,
  runtime-opengl. The -static variant links mach-dxcompiler in, so no
  dxcompiler.dll has to be packaged.
- macOS arm64: runtime-vulkan, runtime-metal, runtime-opengl.
- Android is unchanged (Vulkan only), so x86_64 keeps its executor fallback.
- README records the feature strings, sizes and per-platform symbol families.

Still librashader-cache-v0.12.0, ABI 2 / API 5; the vendored headers are
unchanged.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: D3D11 runtime

**Files:**
- Create: `Source/Core/VideoBackends/D3D/DXLibrashaderRuntime.{h,cpp}`
- Modify: `Source/Core/VideoBackends/D3D/DXGfx.cpp` (or whichever file holds `CreatePostProcessor` for that backend), `Source/Core/VideoBackends/D3D/CMakeLists.txt`

**Interfaces:**
- Consumes: `VideoCommon::LibrashaderRuntime` (Task 4), the `libra_d3d11_*` family (Task 5).
- Produces: `class DXLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime` in `namespace DX11`.

**Reference:** `~/work/pcsx2/pcsx2/GS/Renderers/DX11/GSDevice11.cpp` — read its chain application in full.

- [ ] **Step 1: Note the two view types this backend needs**

`PFN_libra_d3d11_filter_chain_frame` takes `ID3D11ShaderResourceView*` for the input and `ID3D11RenderTargetView*` for the output:

```cpp
typedef libra_error_t (*PFN_libra_d3d11_filter_chain_frame)(libra_d3d11_filter_chain_t *chain,
                                                           ID3D11DeviceContext * device_context,
                                                           size_t frame_count,
                                                           ID3D11ShaderResourceView * image,
                                                           ID3D11RenderTargetView * out,
                                                           const struct libra_viewport_t *viewport,
                                                           const float *mvp,
                                                           const struct frame_d3d11_opt_t *options);
```

In Dolphin the SRV is `DXTexture::GetD3DSRV()` (`DXTexture.h:41`) and the RTV is `DXFramebuffer::GetRTVArray()[0]` (`DXTexture.h:94`) — which is why `RunFrame` takes a framebuffer.

- [ ] **Step 2: Write the header**

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/PostProcessing/LibrashaderRuntime.h"

namespace DX11
{
// Drives librashader's Direct3D 11 runtime. librashader_ld.h is unused; the libra_d3d11_*
// entry points are resolved through VideoCommon::Librashader::GetSymbol in the .cpp, which is
// the only translation unit that defines LIBRA_RUNTIME_D3D11.
class DXLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime
{
public:
  DXLibrashaderRuntime();
  ~DXLibrashaderRuntime() override;

  bool IsSupported() const override;
  bool CreateChain(libra_shader_preset_t preset) override;
  void DestroyChain() override;
  bool HasChain() const override;
  bool RunFrame(const AbstractTexture* source, AbstractFramebuffer* target,
                u64 frame_count) override;
  void SetParameter(const char* name, float value) override;

private:
  // Opaque here so the header does not need d3d11.h or a LIBRA_RUNTIME_* macro.
  void* m_chain = nullptr;
};
}  // namespace DX11
```

- [ ] **Step 3: Write the implementation**

```cpp
#define LIBRA_RUNTIME_D3D11
#include <librashader.h>
```

Then a `Functions()` table resolving `libra_d3d11_filter_chain_create`, `_frame`, `_set_param`, `_free`, exactly as the Vulkan adapter does.

`CreateChain`:

```cpp
  filter_chain_d3d11_opt_t options = {};
  options.version = LIBRASHADER_CURRENT_VERSION;
  options.force_no_mipmaps = false;
  options.disable_cache = false;

  libra_d3d11_filter_chain_t chain = nullptr;
  // create() invalidates `preset` on success and on failure alike, so it is never freed here.
  const std::string error = VideoCommon::Librashader::DescribeAndFreeError(
      Functions().create(&preset, D3D::device.Get(), &options, &chain));
```

`RunFrame`: the D3D11 context is immediate, so there is no render pass to end. Take the SRV from `static_cast<const DXTexture*>(source)->GetD3DSRV()` and the RTV from `static_cast<DXFramebuffer*>(target)->GetRTVArray()[0]`, build `libra_viewport_t{0.0f, 0.0f, target->GetWidth(), target->GetHeight()}`, call `frame(&chain, D3D::context.Get(), frame_count, srv, rtv, &vp, nullptr, nullptr)`, and afterwards invalidate Dolphin's cached D3D11 state so the next Dolphin draw re-binds everything librashader replaced:

```cpp
  // librashader binds its own input layout, shaders, samplers, targets and viewport and does not
  // restore ours. D3D::stateman caches what it last set, so without this the next Dolphin draw
  // believes state is already bound that librashader has overwritten.
  D3D::stateman->SetPixelShader(nullptr);
  D3D::stateman->ApplyState();
```

Read `D3D/D3DState.h` and match whatever invalidation entry point it actually offers — if there is a single `Invalidate`-style call, use that instead of the sketch above, and say in the task report which one you used and why.

- [ ] **Step 4: Wire it into the backend's `CreatePostProcessor`**

Same shape as `VKGfx::CreatePostProcessor` in Task 4 Step 5.

- [ ] **Step 5: Build on Windows**

macOS cannot compile this backend. Sync the branch to the Windows host and build there:

```bash
ssh pcsx2-win "cd /d E:\\work\\dolphin && git fetch origin && git checkout <branch> && git reset --hard origin/<branch>"
```

Note: that is a checkout, not a commit — the host is build-only. Put the configure and build in a `.bat` as in Task 5.

- [ ] **Step 6: Smoke test on the Windows host**

Run Dolphin with the D3D11 backend and crt-royale selected. Confirm: the preset renders (not black, not untouched), the log names the chain creation, and no D3D11 debug-layer errors about state appear. Report exactly what was observed.

- [ ] **Step 7: Full suite on both hosts, then commit**

```bash
./build-qt/Binaries/Tests/tests
git add Source/Core/VideoBackends/D3D/DXLibrashaderRuntime.h \
        Source/Core/VideoBackends/D3D/DXLibrashaderRuntime.cpp \
        Source/Core/VideoBackends/D3D/DXGfx.cpp \
        Source/Core/VideoBackends/D3D/CMakeLists.txt
git commit -m "$(cat <<'EOF'
D3D11: drive librashader's Direct3D 11 runtime

Adds DXLibrashaderRuntime, so the D3D11 backend runs slang presets through
librashader instead of the built-in translator and executor.

- Resolves libra_d3d11_* through the shared loader; LIBRA_RUNTIME_D3D11 is
  defined in this translation unit only.
- Takes the input SRV from DXTexture and the output RTV from DXFramebuffer.
- Invalidates Dolphin's cached D3D11 state after the chain records, because
  librashader binds its own and does not restore ours.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: D3D12 runtime

**Files:**
- Create: `Source/Core/VideoBackends/D3D12/DX12LibrashaderRuntime.{h,cpp}`
- Modify: `Source/Core/VideoBackends/D3D12/D3D12Gfx.{h,cpp}`, `Source/Core/VideoBackends/D3D12/CMakeLists.txt`

**Interfaces:**
- Consumes: `VideoCommon::LibrashaderRuntime` (Task 4), the `libra_d3d12_*` family (Task 5).
- Produces: `class DX12LibrashaderRuntime final : public VideoCommon::LibrashaderRuntime` in `namespace DX12`; a new public `D3D12Gfx::InvalidateCachedState()`.

This is the task the D3D12 black screen (spec §2.1) is ultimately fixed by, and the one spec §8 names as the likeliest source of new bugs. **Follow PCSX2's `GSDevice12.cpp:5015-5061` literally.** Read it before writing anything.

- [ ] **Step 1: Add `InvalidateCachedState()` to `D3D12Gfx`**

`DirtyStates` and `m_dirty_bits` are private (`D3D12Gfx.h:105-137`), and `DirtyState_All` already includes `DirtyState_DescriptorHeaps`, so one public entry point covers everything PCSX2 does by hand:

```cpp
  // Marks every piece of cached state dirty. Call after another component (librashader) has
  // recorded its own root signature, pipeline, descriptor heaps and viewport onto our command
  // list, since Dolphin otherwise believes its own bindings are still in effect.
  void InvalidateCachedState();
```

```cpp
void D3D12Gfx::InvalidateCachedState()
{
  m_dirty_bits = DirtyState_All;
}
```

Nothing yet calls it; the compiler will not complain, and Step 4 uses it.

- [ ] **Step 2: Write the header**

Same shape as Task 6 Step 2, `namespace DX12`, `void* m_chain = nullptr;`, header comment naming `LIBRA_RUNTIME_D3D12` as .cpp-local.

- [ ] **Step 3: `CreateChain`**

```cpp
#define LIBRA_RUNTIME_D3D12
#include <librashader.h>
```

```cpp
  filter_chain_d3d12_opt_t options = {};
  options.version = LIBRASHADER_CURRENT_VERSION;
  options.force_hlsl_pipeline = false;
  options.disable_cache = false;
```

Read the vendored `filter_chain_d3d12_opt_t` (`Externals/librashader/include/librashader.h`) and set only the fields it actually declares — do not copy the field list above blind. `create(&preset, g_dx_context->GetDevice(), &options, &chain)`; `preset` is consumed either way.

- [ ] **Step 4: `RunFrame`**

Use the **resource** image type, not the descriptor variants. `libra_image_d3d12_t` is a tagged union (`librashader.h:681-687`); with `LIBRA_D3D12_IMAGE_TYPE_RESOURCE` the filter chain creates and owns the descriptors, which is what PCSX2 does and what keeps Dolphin's descriptor heaps out of it:

```cpp
  auto* const src = static_cast<const DXTexture*>(source);
  auto* const dst = static_cast<DXFramebuffer*>(target);

  // librashader opens render passes of its own, so ours has to be closed and any pending clear
  // committed before it records. Dolphin's D3D12 backend has no explicit render pass to end, but
  // the framebuffer's pending clear is still ours to resolve.
  src->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  static_cast<DXTexture*>(dst->GetColorAttachment())
      ->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

  libra_image_d3d12_t in = {};
  in.image_type = LIBRA_D3D12_IMAGE_TYPE_RESOURCE;
  in.handle.resource = src->GetResource();
  libra_image_d3d12_t out = {};
  out.image_type = LIBRA_D3D12_IMAGE_TYPE_RESOURCE;
  out.handle.resource =
      static_cast<DXTexture*>(dst->GetColorAttachment())->GetResource();

  const libra_viewport_t vp = {0.0f, 0.0f, static_cast<u32>(dst->GetWidth()),
                               static_cast<u32>(dst->GetHeight())};

  auto chain = static_cast<libra_d3d12_filter_chain_t>(m_chain);
  const libra_error_t err = Functions().frame(&chain, g_dx_context->GetCommandList(),
                                              static_cast<size_t>(frame_count), in, out, &vp,
                                              nullptr, nullptr);

  // librashader bound its own descriptor heaps, root signature, pipeline and viewport onto our
  // command list and does not restore ours.
  static_cast<D3D12Gfx*>(g_gfx.get())->InvalidateCachedState();
```

Two things to resolve while writing this, and to state in the task report:

1. **`GetColorAttachment()` may be null** for a framebuffer with no colour attachment. Guard and return false.
2. **The pending clear.** `DiscardPendingTargetClear()` exists on the interface for exactly this. Check whether Dolphin's D3D12 `DXFramebuffer` tracks a pending clear the way `GSTexture12::CommitClear` does; if it does not, say so rather than inventing a call.

On error, log through `VideoCommon::Librashader::DescribeAndFreeError` and return false so the base class falls through to the passthrough blit. Do **not** copy PCSX2's "drop the chain on frame failure" behaviour — the base class already handles repeat-failure suppression (Task 4), and duplicating it here would fight it.

- [ ] **Step 5: Wire `CreatePostProcessor`, build on Windows, smoke test**

Same as Task 6 Steps 4-6. **This is the case UAT reported as a black screen**, so the smoke test is the point of the task: run crt-royale on D3D12 and confirm it renders. Enable the D3D12 debug layer for the run and report any state or resource-barrier errors verbatim — a missed invalidation shows up as corruption in a *later*, unrelated draw, so a clean debug layer is the evidence that Step 4's reconciliation is complete.

- [ ] **Step 6: Full suite, then commit**

```bash
git add Source/Core/VideoBackends/D3D12/DX12LibrashaderRuntime.h \
        Source/Core/VideoBackends/D3D12/DX12LibrashaderRuntime.cpp \
        Source/Core/VideoBackends/D3D12/D3D12Gfx.h \
        Source/Core/VideoBackends/D3D12/D3D12Gfx.cpp \
        Source/Core/VideoBackends/D3D12/CMakeLists.txt
git commit -m "$(cat <<'EOF'
D3D12: drive librashader's Direct3D 12 runtime

Fixes the black screen UAT reported for slang shaders on D3D12. The
translator's output needed more samplers than the utility root signature
declared, so the final pass never got a pipeline; librashader's own runtime
builds its own root signatures and sidesteps that entirely.

- Passes source and target as LIBRA_D3D12_IMAGE_TYPE_RESOURCE, letting the
  filter chain own the descriptors instead of borrowing Dolphin's heaps.
- Transitions source to pixel-shader-resource and target to render-target
  before the chain records.
- Adds D3D12Gfx::InvalidateCachedState so every dirty bit -- descriptor heaps
  included -- is re-applied after librashader has rebound the command list.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: OpenGL runtime

**Gated on Task 5** producing a `libra_gl_*` family. If the `runtime-opengl` build failed there, skip this task, note the skip, and let the §4.3 rule leave the OpenGL backend on the built-in executor.

**Files:**
- Create: `Source/Core/VideoBackends/OGL/OGLLibrashaderRuntime.{h,cpp}`
- Modify: `Source/Core/VideoBackends/OGL/OGLGfx.cpp`, `Source/Core/VideoBackends/OGL/CMakeLists.txt`

**Interfaces:**
- Consumes: `VideoCommon::LibrashaderRuntime` (Task 4), the `libra_gl_*` family (Task 5).
- Produces: `class OGLLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime` in `namespace OGL`.

Two things make OpenGL different from every other backend here: it needs a function loader at chain-creation time, and its GL-version requirements can rule it out at runtime.

- [ ] **Step 1: `IsSupported()` gates on the GL version**

librashader's GL runtime needs `glsl_version >= 330`, and its shader cache path uses direct state access, which is GL 4.5+. macOS caps at 4.1, so on macOS the chain must be created with the cache disabled or it will fail. Implement:

```cpp
bool OGLLibrashaderRuntime::IsSupported() const
{
  // librashader's GL runtime compiles GLSL 330 and up; anything below that (GLES, GL 3.2 core)
  // cannot run a slang preset at all.
  return g_ogl_config.eSupportedGLSLVersion >= GlslEs300 /* verify the real enum */ &&
         !g_ogl_config.bIsES;
}
```

Read `OGLConfig.h` and use the enum that actually expresses "desktop GLSL 330 or later" — do not guess at `GlslEs300`. Report in the task report which value you used.

- [ ] **Step 2: The loader callback**

```cpp
#define LIBRA_RUNTIME_OPENGL
#include <librashader.h>

namespace
{
// librashader resolves its own GL entry points through this. Dolphin's GLContext already knows
// how, and it is the only object that can answer correctly for the active context.
const void* GLLoader(const char* name)
{
  return g_main_gl_context->GetFuncAddress(name);
}
}  // namespace
```

`create(&preset, GLLoader, &options, &chain)`.

- [ ] **Step 3: `RunFrame`**

Both the input and the output are `libra_image_gl_t{handle, format, width, height}` (`librashader.h:294-303`) — a texture name plus its internal format. Note that the doc comment at `librashader.h:1575` calls the output a `libra_output_framebuffer_gl_t`; **the signature is authoritative and the comment is stale** — `PFN_libra_gl_filter_chain_frame` (`:950-956`) takes two `libra_image_gl_t` by value. Do not chase the struct named in the comment.

```cpp
  const auto describe = [](const AbstractTexture* tex) {
    libra_image_gl_t image = {};
    image.handle = static_cast<const OGLTexture*>(tex)->GetGLTextureId();
    image.format = OGLTexture::GetGLInternalFormatForTextureFormat(tex->GetFormat(), true);
    image.width = tex->GetWidth();
    image.height = tex->GetHeight();
    return image;
  };
```

The target's texture comes from `target->GetColorAttachment()`; guard against null. There is no command list, so `frame` takes no context parameter. Afterwards, restore Dolphin's GL state:

```cpp
  // librashader binds its own FBO, program, VAO and texture units. OGLGfx caches none of the
  // program/VAO state across draws, but the bound framebuffer is Dolphin's -- rebind it so the
  // next draw does not land in librashader's last pass target.
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<OGLFramebuffer*>(target)->GetFBO());
```

Verify that claim about OGLGfx's caching against `OGLGfx.cpp` before relying on it. If it does cache program or VAO state, invalidate that too and say so in the report.

- [ ] **Step 4: Build, smoke test on both platforms, commit**

OpenGL builds on macOS and Windows both, so exercise both. crt-royale on the OpenGL backend; report what rendered on each. Commit:

```
OGL: drive librashader's OpenGL runtime

- Supplies GLContext::GetFuncAddress as the libra_gl_loader_t.
- Gates on desktop GLSL 330+, so GLES and older cores keep the built-in
  executor rather than failing chain creation every frame.
- Rebinds Dolphin's framebuffer after the chain records, since librashader
  leaves its own last-pass target bound.
```

---

## Task 9: Metal runtime

**Gated on Task 5** producing a `libra_mtl_*` family, same as Task 8.

**Files:**
- Create: `Source/Core/VideoBackends/Metal/MTLLibrashaderRuntime.h`, `MTLLibrashaderRuntime.mm`
- Modify: `Source/Core/VideoBackends/Metal/MTLGfx.mm`, `Source/Core/VideoBackends/Metal/CMakeLists.txt`

**Interfaces:**
- Consumes: `VideoCommon::LibrashaderRuntime` (Task 4), the `libra_mtl_*` family (Task 5).
- Produces: `class MTLLibrashaderRuntime final : public VideoCommon::LibrashaderRuntime` in `namespace Metal`.

**The implementation must be a `.mm` file.** librashader's Metal declarations are guarded on `#if (defined(__APPLE__) && defined(LIBRA_RUNTIME_METAL) && defined(__OBJC__))`, so a `.cpp` including the header with `LIBRA_RUNTIME_METAL` defined gets nothing at all — no typedefs, no error, just an empty section and a confusing "undeclared identifier" later. The header stays a plain `.h` with an opaque `void* m_chain`, so `MTLGfx.mm` is the only other file that needs to see it.

- [ ] **Step 1: `CreateChain`**

```objc
#define LIBRA_RUNTIME_METAL
#include <librashader.h>
```

`PFN_libra_mtl_filter_chain_create` takes `(preset, id<MTLCommandQueue>, options, out)`. Dolphin's queue lives on the state tracker; read `MTLStateTracker.h` for the accessor and use the real name.

There is also `libra_mtl_filter_chain_create_deferred`, which records its setup onto a command buffer the caller then commits. Use the plain `create` — it does its own synchronous setup, matching every other backend here, and the deferred variant's contract ("the command buffer must be completely executed before calling frame") adds a synchronisation requirement with no benefit at chain-creation time.

- [ ] **Step 2: `RunFrame`**

```objc
libra_error_t err = Functions().frame(&chain, command_buffer, frame_count,
                                      static_cast<const Metal::Texture*>(source)->GetMTLTexture(),
                                      output_texture, &vp, nullptr, nullptr);
```

Three Metal-specific requirements:

1. **End the current render encoder first.** librashader creates its own. `MTLStateTracker` owns the encoder — find its "end current encoder" entry point and call it; do not open a new one afterwards, the base class's passthrough path will if it needs one.
2. **The output texture** comes from `target->GetColorAttachment()` cast to `Metal::Texture`. For the backbuffer, `Metal::Framebuffer::UpdateBackbufferTexture` means the attachment texture changes per frame — read the current one each call rather than caching it.
3. **The command buffer** must be the one Dolphin is currently recording, from the state tracker, so the chain's work is ordered against Dolphin's own.

- [ ] **Step 3: Build on macOS, smoke test, commit**

```bash
cmake -S . -B build-qt -DUSE_SYSTEM_SDL3=OFF -DCMAKE_CXX_FLAGS= -DCMAKE_OBJCXX_FLAGS=
ninja -C build-qt
```

Run Dolphin with the Metal backend and crt-royale. Metal's validation layer is worth enabling for the run (`METAL_DEVICE_WRAPPER_TYPE=1`); report any encoder or resource errors verbatim. Commit:

```
Metal: drive librashader's Metal runtime

- Implemented in a .mm file, because librashader's Metal declarations are
  gated on __OBJC__ and vanish silently from a .cpp.
- Ends Dolphin's render encoder before the chain records its own.
- Reads the backbuffer's current MTLTexture each frame rather than caching it,
  since Metal::Framebuffer swaps it per frame.
```

---

## Task 10: Make librashader the engine of record and retire the renderer choice

**Files:**
- Modify: `Source/Core/VideoCommon/AbstractGfx.cpp`, each backend's `*Gfx.cpp` (`CreatePostProcessor`), `Source/Core/Core/Config/GraphicsSettings.{h,cpp}`, `Source/Core/VideoCommon/VideoConfig.{h,cpp}`, `Source/Core/VideoCommon/Present.cpp:383-388`, `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.{h,cpp}`, `Source/Android/…/IntSetting.kt`, `Source/Android/…/SettingsFragmentPresenter.kt:1688-1697`, `Source/Android/app/src/main/res/values/strings.xml` and `arrays.xml`
- Test: `Source/UnitTests/VideoCommon/PostProcessing/…` (whichever file covers renderer selection)

**Interfaces:**
- Consumes: every runtime adapter from Tasks 4, 6-9.
- Produces: the §4.3 rule — librashader when the library loaded, the built-in executor when it did not — with no user-facing switch.

- [ ] **Step 1: Write the failing test**

The rule is a pure function of availability, so it can be tested without a GPU. Add to the loader's test file:

```cpp
TEST(LibrashaderLoader, AvailabilityCarriesAReasonWhenUnavailable)
{
  const auto availability = VideoCommon::Librashader::GetAvailability();
  if (!availability.available)
  {
    // A backend that silently declines to use librashader is indistinguishable from one where
    // librashader is broken. The reason string is what the log line and the UI both read.
    EXPECT_FALSE(availability.reason.empty());
  }
  else
  {
    EXPECT_TRUE(availability.reason.empty());
  }
}
```

Run: `ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='LibrashaderLoader.*'`

- [ ] **Step 2: Move the selection into the base class**

Every backend's `CreatePostProcessor` currently repeats the same two-line decision. Replace them with one implementation. In `AbstractGfx`:

```cpp
std::unique_ptr<VideoCommon::IPostProcessor> AbstractGfx::CreatePostProcessor()
{
  // librashader is the engine of record wherever its library loaded. The built-in multipass
  // executor is the fallback for platforms with no vendored binary -- Android x86_64 and Linux --
  // not a user-selectable alternative.
  if (auto runtime = CreateLibrashaderRuntime(); runtime && runtime->IsSupported())
  {
    auto processor =
        std::make_unique<VideoCommon::LibrashaderPostProcessing>(std::move(runtime));
    if (processor->IsUsable())
      return processor;
  }

  const auto availability = VideoCommon::Librashader::GetAvailability();
  if (!availability.available)
    INFO_LOG_FMT(VIDEO, "librashader unavailable ({}); using the built-in post-processor.",
                 availability.reason);

  return std::make_unique<VideoCommon::MultipassPostProcessing>();
}
```

Add the factory hook next to it, defaulting to none:

```cpp
  // Backends with a librashader runtime adapter override this. Returning nullptr means this
  // backend has no adapter, which is the Linux/Android-x86_64 case, not an error.
  virtual std::unique_ptr<VideoCommon::LibrashaderRuntime> CreateLibrashaderRuntime()
  {
    return nullptr;
  }
```

Then each of `VKGfx`, `DXGfx` (D3D11), `D3D12Gfx`, `OGLGfx` and `Metal::Gfx` overrides only `CreateLibrashaderRuntime` and **deletes its `CreatePostProcessor` override entirely**. That is the point of this step: five copies of the decision become one.

- [ ] **Step 3: Delete the setting**

- `GraphicsSettings.h:17,126` — remove the forward declaration and the `Info<>`.
- `GraphicsSettings.cpp:156-157` — remove the definition.
- `VideoConfig.h:79-83` — remove `enum class PostProcessRenderer`; `:130` — remove `CONFIG_CHANGE_BIT_POST_PROCESS_RENDERER`; `:235` — remove `post_process_renderer`.
- `VideoConfig.cpp:154` and `:375` — remove the refresh and the changed-bit.
- `Present.cpp:383-388` — remove the block that rebuilds the post-processor on that bit. Nothing else sets it, so the post-processor is now built once at initialisation and on backend change, which is what the other bits already handle.

Leaving the config *key* orphaned in existing INIs is fine and deliberate — Dolphin ignores unknown keys, and removing it from a user's file is not this task's business.

- [ ] **Step 4: Delete the Qt and Android controls**

- `EnhancementsWidget.cpp:187-191` — the `m_post_process_renderer` combo, its label, its `AddWidget` row and its entry in the tooltip/description block; `EnhancementsWidget.h` — the member.
- `SettingsFragmentPresenter.kt:1688-1697` — the `SingleChoiceSetting`.
- `IntSetting.kt:123-126` — the enum entry.
- `strings.xml` — `post_processing_renderer` and `post_processing_renderer_description`; `arrays.xml` — `postProcessingRendererEntries` and `postProcessingRendererValues`.

Grep for each identifier after removing it; a leftover reference in a layout or another presenter branch will not fail the C++ build.

- [ ] **Step 5: Run the full suite on macOS and Windows, then commit**

```
VideoCommon: use librashader wherever it loaded, and drop the renderer choice

The post-processing renderer was a user-facing choice between librashader and
the built-in slang executor. Now that every desktop backend has a librashader
runtime, the choice only lets a user opt into the weaker engine -- and on the
librashader path a multi-preset chain was silently truncated to its first
entry, so the two options were not even equivalent in what they accepted.

- AbstractGfx::CreatePostProcessor makes the decision once, from library
  availability. Backends override CreateLibrashaderRuntime instead, so five
  copies of the same conditional become one.
- The built-in executor stays as the fallback for platforms with no vendored
  librashader: Android x86_64 and Linux.
- Removes GFX_ENHANCE_POST_PROCESS_RENDERER, its config-change bit, and its
  Qt and Android controls.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

---

## Task 11: One preset instead of a chain

**Files:**
- Delete: `Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.{h,cpp}`, `Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.{h,cpp}`, `Source/UnitTests/VideoCommon/PostProcessing/ShaderChainSpecTest.cpp`
- Modify: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp`, `Source/Core/VideoCommon/CMakeLists.txt`, `Source/Core/DolphinQt/CMakeLists.txt`, `Source/UnitTests/VideoCommon/CMakeLists.txt`, `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.{h,cpp}`, the Android preset picker
- Test: `Source/UnitTests/VideoCommon/PostProcessing/MultipassPostProcessingTest.cpp` (or wherever chain-spec behaviour is asserted from the executor's side)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `GFX_ENHANCE_POST_SHADER` holds exactly one preset path. Tasks 12-14 all assume this.

Spec §2.3 and §4.4. The librashader path already truncated chains silently; this makes single-preset the actual contract.

- [ ] **Step 1: Write the failing test — the migration rule**

An existing INI can hold `a.slangp;b.slangp`. That must resolve to `a.slangp`, not to an error and not to nothing:

```cpp
TEST(PostProcessingConfig, LegacyChainResolvesToItsFirstEntry)
{
  // Configs written before the chain feature was removed hold a ';'-separated list. The
  // librashader path already used only the first entry; now every path does, and out loud.
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp;b.slangp"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset("a.slangp"), "a.slangp");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(""), "");
  EXPECT_EQ(VideoCommon::ResolveConfiguredPreset(";b.slangp"), "");
}
```

The last case is deliberate: a leading separator means the first entry is empty, which is "no shader" — not "skip to b".

Run: `./build-qt/Binaries/Tests/tests --gtest_filter='PostProcessingConfig.*'` → FAIL, no such function.

- [ ] **Step 2: Implement `ResolveConfiguredPreset`**

Put it beside the rest of the shared post-processing helpers — the same place `ResolvePresetPath` lands in Task 4:

```cpp
std::string ResolveConfiguredPreset(std::string_view configured)
{
  // Chains were removed; a ';'-separated value is a config written by an older build. Take the
  // first entry, which is what the librashader path already did, and log once so the loss is
  // visible rather than silent.
  const size_t separator = configured.find(';');
  if (separator == std::string_view::npos)
    return std::string(configured);

  WARN_LOG_FMT(VIDEO,
               "Post-processing shader chains are no longer supported; using only \"{}\" from "
               "the configured chain.",
               configured.substr(0, separator));
  return std::string(configured.substr(0, separator));
}
```

Run the test → PASS.

- [ ] **Step 3: Route every reader through it and delete `ShaderChainSpec`**

`MultipassPostProcessing.cpp` is the only remaining consumer of `CHAIN_SEPARATOR`, `AppendToChainSpec` and `DescribeChainSpec`. Replace its chain-splitting loop with a single call to `ResolveConfiguredPreset`, then delete `ShaderChainSpec.{h,cpp}` and `ShaderChainSpecTest.cpp` and drop them from the three CMakeLists.

The executor's own multi-*pass* handling is untouched — a `.slangp` with ten passes still runs ten passes. What goes away is Dolphin concatenating several presets.

- [ ] **Step 4: Delete `PostProcessingChainDialog` and its entry point**

The "Chain…" button at `EnhancementsWidget.cpp:198-199` and the synthetic combo entry at `:367-403` that appends a chain description both go. Do not build the replacement UI here — Task 13 does, and this task must leave the widget compiling and functional with the plain combo in the meantime. Say in the report that the combo is knowingly still the 30,864-entry flat list until Task 13.

- [ ] **Step 5: Android**

Its picker has "Select" (replace) and an append action. Remove the append; keep the picker. Grep the Kotlin for the separator string.

- [ ] **Step 6: Full suite, then commit**

```
VideoCommon: replace post-processing chains with a single preset

librashader accepts one preset, and Dolphin's librashader path had been
silently discarding every entry after the first -- so a user who built a
three-preset chain and switched renderers lost two of them with no warning.
Rather than teach librashader to chain, drop chains.

- ResolveConfiguredPreset takes the first entry of a legacy ';'-separated
  value and warns, so existing configs keep working and the loss is visible.
- Removes ShaderChainSpec, PostProcessingChainDialog and their tests.
- Multi-pass presets are unaffected; this is about concatenating presets, not
  about the passes inside one.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

---

## Task 12: Parameter enumeration, persistence and live application

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/LibrashaderParameters.{h,cpp}`
- Test: `Source/UnitTests/VideoCommon/PostProcessing/LibrashaderParametersTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/UnitTests/VideoCommon/CMakeLists.txt`, `Source/Core/Core/Config/GraphicsSettings.{h,cpp}`, `Source/Core/VideoCommon/PostProcessing/LibrashaderPostProcessing.cpp` (Task 4's file)

**Interfaces:**
- Consumes: `VideoCommon::Librashader::Common()` (Task 3) — `preset_create`, `preset_get_runtime_params`, `preset_free_runtime_params`, `preset_free`; `LibrashaderRuntime::SetParameter` (Task 4).
- Produces:

```cpp
namespace VideoCommon::LibrashaderParameters
{
struct ParameterInfo
{
  std::string name;
  std::string description;
  float initial = 0.0f;
  float minimum = 0.0f;
  float maximum = 0.0f;
  float step = 0.0f;
};

using Overrides = std::vector<std::pair<std::string, float>>;

bool Enumerate(const std::string& absolute_preset_path, std::vector<ParameterInfo>* out,
               std::string* error);

Overrides ParseOverrides(const std::vector<std::string>& entries);
std::vector<std::string> FormatOverrides(const Overrides& overrides);

int DecimalsForStep(float step);
bool IsDefaultValue(float value, float initial);

Overrides Load(const std::string& preset_relative_path);
void Save(const std::string& preset_relative_path, const Overrides& overrides);
}  // namespace VideoCommon::LibrashaderParameters
```

**This task needs no library rebuild** (spec §2.4): every symbol it uses is runtime-independent and already exported by the vendored Vulkan-only binaries. It therefore does not depend on Task 5 and can run in parallel with 6-9.

**Reference:** `~/work/pcsx2/pcsx2/GS/ShaderChain/ShaderChainParams.h` and its `.cpp`.

- [ ] **Step 1: Write the failing tests**

Four behaviours, all CPU-only. `DecimalsForStep` and `IsDefaultValue` are pure; `ParseOverrides`/`FormatOverrides` round-trip; `Enumerate` needs a real `.slangp` on disk, so gate it on the shader pack being present the way the existing preset tests do.

```cpp
TEST(LibrashaderParameters, DecimalsForStep)
{
  using VideoCommon::LibrashaderParameters::DecimalsForStep;
  EXPECT_EQ(DecimalsForStep(1.0f), 0);
  EXPECT_EQ(DecimalsForStep(0.5f), 1);
  EXPECT_EQ(DecimalsForStep(0.05f), 2);
  EXPECT_EQ(DecimalsForStep(0.01f), 2);
  EXPECT_EQ(DecimalsForStep(0.001f), 3);
  EXPECT_EQ(DecimalsForStep(0.0001f), 4);
  // Capped: a 1e-6 step would need six decimals, which no spin box is usable at.
  EXPECT_EQ(DecimalsForStep(0.000001f), 4);
  EXPECT_EQ(DecimalsForStep(16.0f), 0);
  EXPECT_EQ(DecimalsForStep(100.0f), 0);
  // A preset that declares no step gets a middling default rather than zero decimals, which
  // would silently round every edit to an integer.
  EXPECT_EQ(DecimalsForStep(0.0f), 3);
  EXPECT_EQ(DecimalsForStep(-1.0f), 3);
}

TEST(LibrashaderParameters, IsDefaultValueUsesARelativeEpsilon)
{
  using VideoCommon::LibrashaderParameters::IsDefaultValue;
  EXPECT_TRUE(IsDefaultValue(1.0f, 1.0f));
  EXPECT_TRUE(IsDefaultValue(1.0000001f, 1.0f));
  EXPECT_FALSE(IsDefaultValue(1.001f, 1.0f));
  // Absolute epsilons break on large defaults; crt-royale has parameters in the thousands.
  EXPECT_TRUE(IsDefaultValue(4000.0f, 4000.0f));
  EXPECT_FALSE(IsDefaultValue(4000.1f, 4000.0f));
  EXPECT_TRUE(IsDefaultValue(0.0f, 0.0f));
}

TEST(LibrashaderParameters, OverridesRoundTrip)
{
  using namespace VideoCommon::LibrashaderParameters;
  const Overrides parsed = ParseOverrides({"gamma=2.5", "beam_min=1", "junk", "=3", "bad=x"});
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0].first, "gamma");
  EXPECT_FLOAT_EQ(parsed[0].second, 2.5f);
  EXPECT_EQ(parsed[1].first, "beam_min");
  EXPECT_FLOAT_EQ(parsed[1].second, 1.0f);
  EXPECT_EQ(FormatOverrides(parsed), std::vector<std::string>({"gamma=2.5", "beam_min=1"}));
}

TEST(LibrashaderParameters, LastDuplicateWinsAtTheFirstPosition)
{
  // A hand-edited INI can name the same parameter twice. Position stability matters because the
  // dialog lists parameters in this order.
  const auto parsed =
      VideoCommon::LibrashaderParameters::ParseOverrides({"a=1", "b=2", "a=3"});
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0].first, "a");
  EXPECT_FLOAT_EQ(parsed[0].second, 3.0f);
  EXPECT_EQ(parsed[1].first, "b");
}
```

Run: `ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='LibrashaderParameters.*'` → FAIL to compile, no such header.

- [ ] **Step 2: Implement the pure helpers**

`DecimalsForStep`: walk `step` against `1, 0.5, 0.05, 0.01, 0.001, 0.0001` rather than computing a logarithm — the table is what the test pins and what PCSX2 ships. Cap at 4. `step <= 0` → 3.

`IsDefaultValue`: `std::abs(value - initial) <= 1e-6f * std::max(1.0f, std::abs(initial))`.

`ParseOverrides`: split on the first `=`; skip an empty name; parse with `std::strtof` and require the whole remainder to be consumed (`std::from_chars` for `float` is unavailable at Dolphin's 11.0 deployment target — see Global Constraints). `WARN_LOG_FMT` each skipped entry once, naming it.

`FormatOverrides`: `fmt::format("{}={}", name, value)`. Use `{}` not a fixed precision so a value round-trips exactly.

Run → PASS.

- [ ] **Step 3: `Enumerate`**

```cpp
  const auto& common = VideoCommon::Librashader::Common();

  libra_shader_preset_t preset = nullptr;
  if (const libra_error_t err = common.preset_create(absolute_preset_path.c_str(), &preset))
  {
    *error = VideoCommon::Librashader::DescribeAndFreeError(err);
    return false;
  }

  libra_preset_param_list_t params = {};
  if (const libra_error_t err = common.preset_get_runtime_params(&preset, &params))
  {
    *error = VideoCommon::Librashader::DescribeAndFreeError(err);
    common.preset_free(&preset);
    return false;
  }

  out->reserve(params.length);
  for (uint64_t i = 0; i < params.length; i++)
  {
    const libra_preset_param_t& p = params.parameters[i];
    out->push_back({p.name ? p.name : "", p.description ? p.description : "", p.initial,
                    p.minimum, p.maximum, p.step});
  }

  // Takes the list by value, not by pointer -- see librashader.h.
  common.preset_free_runtime_params(params);
  common.preset_free(&preset);
  return true;
```

Guard the whole function on `GetAvailability().available` and fill `*error` with the availability reason when it is not. This function must never touch a GPU: it is called from the Qt thread with no device.

- [ ] **Step 4: Persistence**

One INI section, one key per preset, holding a string list — PCSX2's shape:

```cpp
// Overrides live under their own section, keyed by the preset's path relative to the shaders
// root, so switching presets and switching back keeps each preset's edits.
const Info<std::string> GFX_LIBRASHADER_PARAMETERS{
    {System::GFX, "LibrashaderParameters", ""}, ""};
```

Dolphin's `Config` has no string-list type, so `Load`/`Save` store one `;`-joined value per key and split on read. State that choice in the code comment: parameter names come from `#pragma parameter` and cannot contain `;`, so the separator is safe — verify that claim against the slang spec or the preset pack before relying on it, and if a name *can* contain `;`, escape it and add a test.

**Only non-default values are persisted.** `Save` filters through `IsDefaultValue` against the enumerated `initial`, so resetting a parameter removes the key rather than writing the default back — which keeps a preset's own default changes flowing through on a shader-pack update.

- [ ] **Step 5: Apply overrides to a live chain**

In Task 4's `VideoCommon::LibrashaderPostProcessing`, after a chain is created, load the overrides for the resolved preset and push each through `LibrashaderRuntime::SetParameter`. Add a generation counter so the dialog's edits reach a running chain:

```cpp
  // The dialog pushes EVERY parameter, not only the overridden ones. A freshly built chain starts
  // from the preset defaults, but a live chain remembers the last value it was given -- so
  // resetting a parameter has to send the default explicitly.
```

- [ ] **Step 6: Full suite, then commit**

```
VideoCommon: enumerate, persist and apply librashader shader parameters

Groundwork for the parameters dialog. No GPU or runtime involvement: the
symbols used here are runtime-independent, so this works against the existing
vendored binaries.

- Enumerate() lists a preset's #pragma parameters through
  libra_preset_get_runtime_params.
- Overrides are stored per preset under their own INI section, and only values
  that differ from the preset default are written -- so a preset whose defaults
  change in a pack update is not pinned to the old ones.
- DecimalsForStep and IsDefaultValue are the UI's formatting and reset rules,
  unit-tested here rather than in Qt code.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

---

## Task 13: `ShaderPresetPickerDialog`

**Files:**
- Create: `Source/Core/DolphinQt/Config/Graphics/ShaderPresetPickerDialog.{h,cpp}`
- Test: `Source/UnitTests/DolphinQt/…` if the project has a Qt test target; if it does not, add the tree-building logic as a free function in the `.cpp`'s anonymous namespace **is not acceptable** — put it somewhere testable (see Step 1) and say what you chose.
- Modify: `Source/Core/DolphinQt/CMakeLists.txt`

**Interfaces:**
- Consumes: `MultipassPostProcessing::GetPresetList()` (`MultipassPostProcessing.h:38`) — a flat `std::vector<std::string>` of relative preset paths.
- Produces: `class ShaderPresetPickerDialog final : public QDialog` with `QString GetSelectedPreset() const;`.

**Reference:** `~/work/pcsx2/pcsx2-qt/ShaderPresetPickerDialog.cpp` (135 lines). Read it in full. This is the fix for UAT finding 2: 30,864 presets in a flat combo box become a filterable tree.

Two deliberate departures from PCSX2, both forced by Dolphin's conventions rather than chosen:

- **No `.ui` file.** `DolphinQt/Config/Graphics/` builds its layouts in C++. Construct the same arrangement — filter line edit on top, `QTreeView` filling the dialog, a selection label, then a `QDialogButtonBox` — in the constructor.
- **Preset source** is Dolphin's `GetPresetList()`, not PCSX2's `ShaderPresets::Enumerate()`.

- [ ] **Step 1: Write the failing test**

The tree construction is the part worth testing and the part most likely to be wrong: a flat list of `a/b/c.slangp` strings has to become nested folder items with non-selectable folders. Extract it so a test can reach it:

```cpp
// ShaderPresetTree.h -- VideoCommon-independent, Qt-independent.
namespace DolphinQt
{
struct PresetTreeNode
{
  std::string name;               // the last path component
  std::string path;               // the full relative path, "" for the synthetic root
  bool is_preset = false;
  std::vector<PresetTreeNode> children;
};

// Groups a flat list of relative preset paths into folders. Folder order and leaf order both
// follow first appearance in `presets`, which is the discovery order the picker shows.
PresetTreeNode BuildPresetTree(const std::vector<std::string>& presets);
}  // namespace DolphinQt
```

```cpp
TEST(ShaderPresetTree, GroupsByFolderAndMarksLeaves)
{
  const auto root = DolphinQt::BuildPresetTree(
      {"crt/crt-royale.slangp", "crt/crt-geom.slangp", "handheld/gbc.slangp", "flat.slangp"});

  ASSERT_EQ(root.children.size(), 3u);
  EXPECT_EQ(root.children[0].name, "crt");
  EXPECT_FALSE(root.children[0].is_preset);
  EXPECT_EQ(root.children[0].path, "crt");
  ASSERT_EQ(root.children[0].children.size(), 2u);
  EXPECT_EQ(root.children[0].children[0].name, "crt-royale.slangp");
  EXPECT_EQ(root.children[0].children[0].path, "crt/crt-royale.slangp");
  EXPECT_TRUE(root.children[0].children[0].is_preset);

  // A preset at the root has no folder to live in and must not be dropped.
  EXPECT_EQ(root.children[2].name, "flat.slangp");
  EXPECT_TRUE(root.children[2].is_preset);
}

TEST(ShaderPresetTree, HandlesWindowsSeparatorsAndEmptyComponents)
{
  // GetPresetList() builds paths with File::, which yields '\' on Windows.
  const auto root = DolphinQt::BuildPresetTree({"crt\\crt-royale.slangp", "crt//dup.slangp", ""});
  ASSERT_EQ(root.children.size(), 1u);
  EXPECT_EQ(root.children[0].name, "crt");
  ASSERT_EQ(root.children[0].children.size(), 2u);
  EXPECT_EQ(root.children[0].children[0].path, "crt/crt-royale.slangp");
}
```

That second test is not hypothetical: this fork already had a Windows path-normalisation bug in the preset layer. Normalise to `/` in `BuildPresetTree` and store the normalised form in `path`, since `path` is what gets written to the config and compared against it.

Run: `./build-qt/Binaries/Tests/tests --gtest_filter='ShaderPresetTree.*'` → FAIL.

- [ ] **Step 2: Implement `BuildPresetTree`, run the test to green**

Keep a `std::map<std::string, PresetTreeNode*>` from folder path to node as PCSX2 does, but append in first-appearance order rather than the map's order — the test pins that.

- [ ] **Step 3: Build the dialog on top of it**

`QStandardItemModel` filled by walking the tree; two custom roles:

```cpp
  enum
  {
    ROLE_PATH = Qt::UserRole,
    ROLE_IS_PRESET,
  };
```

Folders get `setSelectable(false)` and `setEditable(false)`; leaves get the full path as `ROLE_PATH` and as their tooltip. Then, matching PCSX2 behaviour exactly:

- `QSortFilterProxyModel` with `setRecursiveFilteringEnabled(true)`, `setFilterCaseSensitivity(Qt::CaseInsensitive)`, and **`setFilterRole(ROLE_PATH)`** — filtering on the full path, not the display name, so typing `crt` matches everything under `crt/` and typing `royale` matches the leaf.
- Non-empty filter → `expandAll()`; empty filter → `collapseAll()` then re-select the current preset.
- `currentChanged` updates a selection label and enables/disables OK; a folder selects nothing.
- `activated` (double-click or Enter) on a leaf accepts the dialog.
- OK starts disabled; the constructor calls `selectPreset(current)` and focuses the filter box.

- [ ] **Step 4: Verify by hand**

Build the Qt app and open the dialog against the full pack. Confirm: it opens without a visible delay, the filter narrows as typed, folders cannot be selected, double-click accepts, and re-opening it lands on the current preset with its ancestors expanded. Report the observed open time with the full pack — 30,864 leaves is the case this exists for, and if construction is slow that is a finding, not a detail.

- [ ] **Step 5: Full suite, then commit**

```
DolphinQt: add a filterable tree picker for shader presets

UAT found the preset UI unusable, and the cause is scale: the full libretro
pack is 30,864 presets and they were all in one flat combo box.

- BuildPresetTree groups the flat discovery list into folders, normalising
  Windows separators, and is unit-tested without Qt.
- The dialog filters on the full relative path with recursive filtering, so
  typing a folder name or a preset name both work. Folders are not selectable.
- Modelled on PCSX2's ShaderPresetPickerDialog, laid out in C++ because
  DolphinQt does not use .ui files here.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

---

## Task 14: `ShaderParametersDialog` and the `EnhancementsWidget` rework

**Files:**
- Create: `Source/Core/DolphinQt/Config/Graphics/ShaderParametersDialog.{h,cpp}`
- Modify: `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.{h,cpp}`, `Source/Core/DolphinQt/CMakeLists.txt`

**Interfaces:**
- Consumes: `VideoCommon::LibrashaderParameters` (Task 12), `ShaderPresetPickerDialog` (Task 13).
- Produces: the final settings row.

**Reference:** `~/work/pcsx2/pcsx2-qt/ShaderParametersDialog.cpp` (359 lines). Read it in full before writing — its behaviours below are each there for a reason found in use.

- [ ] **Step 1: The row model**

```cpp
  struct Row
  {
    VideoCommon::LibrashaderParameters::ParameterInfo info;
    float value = 0.0f;
    QSlider* slider = nullptr;         // null when the parameter has too few steps to be worth one
    QDoubleSpinBox* spin = nullptr;
    QPushButton* reset = nullptr;
    float slider_increment = 0.0f;     // value units per slider step
  };
```

Five behaviours to carry over verbatim, with the reason each exists:

1. **Suppress the slider when `whole_steps < 1.0`** — a parameter whose range is narrower than one step is a toggle, and a slider with one position is a lie.
2. **`MAX_SLIDER_STEPS` cap** — a parameter with a 0.0001 step over a range of 100 would need a million-position slider. Cap the step count and stretch `slider_increment` to match; the spin box remains exact.
3. **Snap to the default** when the dragged value lands within `slider_increment * 0.5f` of `initial` — otherwise a slider can never quite return to the preset default, which is the single most-wanted interaction.
4. **`installEventFilter` for wheel events** so scrolling over an unfocused slider or spin box scrolls the *list* instead of changing the value. Without this, scrolling past a row silently edits it.
5. **Debounced write** (`WRITE_DELAY_MS`) — dragging a slider must not write the INI on every pixel.

And one behaviour that is a correctness requirement rather than a nicety: **push every parameter to the live chain, not only the overridden ones** (Task 12 Step 5's comment). A live chain remembers the last value it was handed, so a reset that sends nothing leaves the old value in place.

- [ ] **Step 2: Empty and error states**

Replace the rows with a single status label when `Enumerate` returns no parameters ("This preset has no adjustable parameters.") or fails (the error string). Do not show an empty scroll area — UAT finding 2 was in part about not being able to tell whether the UI had done anything.

- [ ] **Step 3: Rework the `EnhancementsWidget` row**

Delete `LoadPostProcessingShaders()` (`:367-403`) and `m_post_processing_effect` (the flat combo). The new row, matching PCSX2's settings tab:

```
Post-Processing Effect:  [ crt/crt-royale.slangp        ]  [Browse…] [Clear] [Parameters…] [Download…]
```

- A read-only `QLineEdit` showing the current preset, or empty for "off".
- *Browse…* opens `ShaderPresetPickerDialog`; accept writes `GFX_ENHANCE_POST_SHADER`.
- *Clear* writes `""`.
- *Parameters…* opens `ShaderParametersDialog`, disabled when no preset is set.
- *Download…* keeps this fork's existing menu unchanged.

Keep the game-layer plumbing (`m_game_layer`) that the combo had — per-game post-processing settings must keep working. Verify it does by setting a per-game preset and confirming the global one is unchanged.

- [ ] **Step 4: Verify by hand**

Open Graphics → Enhancements. Set a preset, open Parameters, drag a slider and confirm the picture changes while the emulator runs; reset one parameter and confirm the picture returns; close and re-open the dialog and confirm the values persisted; clear the preset and confirm *Parameters…* disables. Report each as observed or not observed.

- [ ] **Step 5: Full suite, then commit**

```
DolphinQt: add a shader parameters dialog and rework the enhancements row

Completes the UI half of the librashader work. The post-processing row is now
PCSX2's: a read-only preset field with Browse, Clear and Parameters, replacing
a flat 30,864-entry combo box and a chain dialog whose two buttons both closed
it.

- One row per #pragma parameter with a slider, a spin box and a reset. Sliders
  are omitted for parameters with fewer than two steps and capped in step count
  for fine-grained ones, and snap to the preset default near it.
- Wheel events over an unfocused control scroll the list instead of editing the
  value.
- Writes are debounced, and every parameter is pushed to the live chain so a
  reset takes effect immediately.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

---

## Task 15: Close the crt-royale darkening gate

**Files:** none necessarily. This task's deliverable is a measurement and a decision; whether it produces a code change depends on what the measurement says.

**Interfaces:**
- Consumes: `ChainDebugDump` (Task 2), a working librashader path on at least one backend (Task 4 or 6-9).

Spec §7. The root cause is **not established**, and three candidates remain: the chain's input is already dark, the chain's output is mishandled downstream, or there is no defect and the reported darkness is librashader's inherent ~68-71% at 1080p. This task is what turns that into an answer.

It needs pixels from a running game, which the Windows UAT host cannot supply (it has no GameCube or Wii images). **If you cannot run a game, stop here and say so — do not guess, and do not close finding 3 on reasoning alone.**

- [ ] **Step 1: Capture**

Set `LibrashaderDumpChainImages = True` under `[Settings]` in `GFX.ini`, launch a game with crt-royale selected, let one frame render, and collect the two PNGs. The option spends its budget after one frame, so there will be exactly two.

- [ ] **Step 2: Measure**

Compute the mean channel value of each dump. The reference table from spec §7, measured on the UAT host with a flat sRGB-128 field:

| runtime | source | output | mean vs input |
|---|---|---|---|
| vulkan | 640×528 | 640×528 | 110.9% |
| vulkan | 1920×1080 | 1920×1080 | 70.8% |
| d3d11 | 640×528 | 1920×1080 | 68.1% |
| d3d12 | 640×528 | 1920×1080 | 68.0% |

Game content is not a flat field, so the absolute numbers will not match. What transfers is the **ratio**: output mean / input mean at the same resolutions.

- [ ] **Step 3: Decide, with the evidence attached**

Three outcomes, three different next actions:

1. **Output/input ≈ 68-71%** and the input looks like the game at normal brightness → **there is no Dolphin defect.** crt-royale is inherently this dark at 1080p output, and librashader's own CLI reproduces it. Close finding 3, record the measurement, and note in the release notes that crt-royale's brightness matches RetroArch's rather than Dolphin's built-in executor's.
2. **The input dump is already dark** → the defect is upstream of the chain. That is Dolphin's XFB or the downscale, not librashader; open a separate investigation with the input dump attached.
3. **Output/input is well below 68%**, or the on-screen picture is darker than the output dump → the defect is downstream, in the passthrough blit or the backbuffer handling. Task 4's restructuring touched exactly that code, so re-read it against the dump.

Write the outcome into the spec's §7 as a resolution, replacing the open-gate framing. Attach the two means and the resolutions they were measured at.

**Do not report a conclusion you did not measure.** "Probably outcome 1" is not a resolution; the whole point of Task 2 was to make this a measurement.

---

## Task 16: Documentation

**Files:**
- Modify: `docs/superpowers/specs/2026-09-16-librashader-desktop-runtimes-design.md`, `Externals/librashader/README.md`
- Possibly modify: the release notes for the next fork release

**Interfaces:** consumes the reports from every prior task.

Last, because it records what the work turned out to do rather than what it was meant to.

- [ ] **Step 1: Reconcile the spec with reality**

Walk the spec section by section and correct anything the implementation contradicted. Specifically:

- §2.6's feature table against what Task 5 actually built (a runtime that failed to build must be recorded as absent, per-platform).
- §4.2's five hooks against the interface Task 4 actually landed — it may have grown or lost one.
- §7 against Task 15's outcome.
- §8's risks: mark each as materialised, avoided, or still open, with a sentence saying which.

A spec that still describes the plan rather than the result is worse than no spec, because the next person believes it.

- [ ] **Step 2: `Externals/librashader/README.md`**

Task 5 Step 6 already rewrote the build provenance. Check two things it may not have: the `_LIBRASHADER_LOAD` patch note (lines 93-100) must be **gone**, since Task 3 reverted that patch and stopped using `librashader_ld.h` entirely, and the note about which platforms have no binary (line 51) must still be accurate — it is now load-bearing, because §4.3's fallback rule is what keeps those platforms working.

- [ ] **Step 3: User-visible changes for the release notes**

Three things a user will notice and should be told:

1. The Post-Processing Renderer setting is gone; librashader is used wherever it is available.
2. Shader chains are gone; a chain in an existing config resolves to its first preset.
3. The preset picker is a filterable tree, and presets with `#pragma parameter` declarations are now editable.

Also correct the standing error in the published notes for the current release: they claim native `GenerateMipmaps` for "Metal, OpenGL, D3D11 and D3D12", but D3D12 has no implementation. That correction needs `gh` and an account switch, so ask before doing it rather than assuming the authorisation carries over.

- [ ] **Step 4: Commit**

```
docs: reconcile the librashader desktop spec with what shipped

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

---

## Self-review

Run against the spec after the plan is written, before execution starts.

**Spec coverage:**

| Spec section | Task(s) |
|---|---|
| §2.1 D3D12 root signature + silent pass failure | 1 |
| §2.6 feature sets / no `dxcompiler.dll` | 5 |
| §4.1 stop using `librashader_ld.h` | 3 |
| §4.2 hoist orchestration behind hooks | 4 |
| §4.3 selection rule, executor as fallback | 10 |
| §4.4 single preset | 11 |
| §4.5 the two dialogs | 13, 14 |
| §4.5 parameter enumeration and persistence | 12 |
| per-backend runtimes | 6 (D3D11), 7 (D3D12), 8 (OGL), 9 (Metal) |
| §7 darkening gate | 2 (instrumentation), 15 (resolution) |
| §9 verification | every task's test and smoke steps |

No spec section is unclaimed.

**Known gaps, stated rather than hidden:**

- **Tasks 8 and 9 are gated on Task 5's build succeeding** for `runtime-opengl` and `runtime-metal`. Neither has been exercised on this hardware (spec §8). If either fails, that backend stays on the built-in executor — which is why Task 1 comes first and is not optional.
- **Task 15 cannot be completed without a game image**, and the Windows UAT host has none. The task says so explicitly rather than pretending otherwise.
- **The Windows host is checkout-and-build only.** Every task that builds there (5-8, 10, 11) must commit on the development machine and `git fetch`/`checkout` on the host. Never commit, amend, rebase or push on the Windows host.
- **No Qt unit-test target may exist.** Task 13 pushes the testable logic (`BuildPresetTree`) out of the Qt class specifically so this does not become an excuse to ship it untested; if there is genuinely nowhere to put the test, say where you put it and why.

**Type consistency:** `LibrashaderRuntime`'s five methods are declared once in Task 4 and implemented in 4, 6, 7, 8 and 9 with the same signatures. `ParameterInfo`'s six fields are declared in Task 12 and consumed in Task 14. `PresetTreeNode` is declared and consumed inside Task 13. `ResolveConfiguredPreset` is declared in Task 11 and used by Tasks 4 and 12's callers.
