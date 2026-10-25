# Desktop Post-Processing Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the Windows and macOS builds full feature parity with Android for the slang/RetroArch post-processing stack.

**Architecture:** The post-processing engine (`MultipassPostProcessing`, `SlangPreset`, `SlangTranslator`) is already backend-agnostic; the desktop gaps are a POSIX-only path normalizer, a Vulkan-only clip-space Y flip, a Vulkan-only mip-generation capability, a Qt front end that predates the Android chain UI, and missing librashader binaries. Every fix goes into the shared VideoCommon layer behind a pure, unit-testable helper wherever the logic is not GPU-bound; the two GPU-bound pieces (native `GenerateMipmaps`, the draw-based mip fallback) are verified on real hardware.

**Tech Stack:** C++20, CMake + Ninja, Qt 6, GoogleTest (`Source/UnitTests`), SPIRV-Cross/glslang, Rust (`cargo`, for the librashader C API only).

**Spec:** [docs/superpowers/specs/2026-09-16-desktop-postprocessing-parity-design.md](../specs/2026-09-16-desktop-postprocessing-parity-design.md)

## Global Constraints

- Branch: `feature/desktop-postprocessing-parity` (already checked out). Base: `master`.
- Every commit message must be shown in chat for review **before** running `git commit`, and must end with `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.
- Test build/run (macOS host): `cmake -S . -B build-qt -DUSE_SYSTEM_SDL3=OFF -DCMAKE_CXX_FLAGS= -DCMAKE_OBJCXX_FLAGS=` then `ninja -C build-qt unittests` then `./build-qt/Binaries/Tests/tests --gtest_filter='<Suite>.*'`. `ctest` from the build root reports "No tests were found!!!" — every `add_dolphin_test` links into the single `tests` binary. Baseline before this plan (whole `tests` binary, measured 2026-09-16): **1113 tests, 68 suites, 1112 passed, 1 skipped** (`SlangCompile.RealPresetCompilesAllPasses`, gated on the `SLANG_PRESET` env var). Each task adds tests, so compare against the previous task's count, not this one — after Task 1 it is 1121/68.
- Windows verification host: `ssh pcsx2-win`, repo at `E:\work\dolphin`, shell is `cmd.exe` (no `head`/`grep`/POSIX utilities). `cmake` is not on `PATH` — use `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`, and run inside `vcvars64.bat`.
- Do not use `std::from_chars` on `float` in VideoCommon — unavailable at the macOS 11.0 deployment target. Use `std::strtof`/`std::strtol`.
- New pure helpers follow the house style of `PostProcessing/ChainOutputPolicy.h`: `constexpr` where possible, with `static_assert`s next to the declaration.
- Copyright header on every new file: `// Copyright 2026 Dolphin Emulator Project` / `// SPDX-License-Identifier: GPL-2.0-or-later`.
- Out of scope, do not touch: the Android GPU driver manager (`GFX_DRIVER_PACKAGE`, adrenotools, Turnip injection), HDR paper-white/tonemap restoration, the librashader Metal runtime.

## File Structure

| File | Responsibility | Task |
| --- | --- | --- |
| `Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp` | separator-agnostic lexical path normalization | 1 |
| `Source/Core/VideoCommon/PostProcessing/SlangShader.cpp` | `#include` expansion, both separators | 1 |
| `Source/Core/VideoCommon/PostProcessing/SlangTranslator.{h,cpp}` | clip-Y-flip decision + injection | 2 |
| `Source/Core/VideoCommon/PostProcessing/MipGen.h` | `MipLevelCount` / `MipLevelSize` pure math | 3 |
| `Source/Core/VideoCommon/VideoConfig.h` | `bSupportsGPUMipGeneration` capability | 3 |
| `Source/Core/VideoBackends/{Metal,OGL,D3D}/…Texture.{mm,cpp}` | native `GenerateMipmaps` | 4 |
| `Source/Core/VideoCommon/MipChainBuilder.{h,cpp}` | portable draw-based mip fallback (D3D12) | 5 |
| `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.{h,cpp}` | remove dead controls, add renderer combo + chain button | 6, 8 |
| `Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.{h,cpp}` | chain/category string logic shared with Android's UI semantics | 7 |
| `Source/Core/VideoCommon/PostProcessing/RetroCrisisInstall.{h,cpp}` | `GetRetroCrisisProfiles()` source of truth | 7 |
| `Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.{h,cpp}` | category → preset picker, Select / Add to Chain | 8 |
| `Externals/librashader/` | desktop binaries + `#ifndef`-guarded load macros | 9 |
| `Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.{h,cpp}` | absolute library path per platform | 9 |
| `Source/UnitTests/VideoCommon/PostProcessing/SlangCompileTest.cpp` | compile oracle across D3D/Metal/OGL/Vulkan headers | 10 |

---

### Task 1: Separator-agnostic path normalization

Windows `base_dir` values contain backslashes and a drive letter. `NormalizePath` splits on `/` only, so `E:\Sys\Shaders\crt` is one indivisible token that a single `../` pops — destroying the whole prefix. Fixing the one normalizer fixes `ResolvePath`, `#reference` resolution, and `RetroCrisisInstall.cpp:98` at once.

**Files:**
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp:17-58` (`NormalizePath`), `:90-93` (`ResolvePath`), `:159` (`DirectoryOf`)
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangPreset.h:61-63` (doc comment)
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangShader.cpp:71-80` (`JoinPath`, `DirectoryOf`)
- Test: `Source/UnitTests/VideoCommon/PostProcessing/SlangPresetTest.cpp`, `Source/UnitTests/VideoCommon/PostProcessing/SlangShaderTest.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `std::string VideoCommon::NormalizePath(const std::string& path)` — unchanged signature, now separator-agnostic; always returns `/` separators. No other task depends on Task 1.

- [ ] **Step 1: Write the failing tests**

Append to `Source/UnitTests/VideoCommon/PostProcessing/SlangPresetTest.cpp`:

```cpp
TEST(SlangPreset, NormalizePathAcceptsWindowsSeparators)
{
  EXPECT_EQ(NormalizePath("E:\\work\\dolphin\\Sys\\Shaders"), "E:/work/dolphin/Sys/Shaders");
  EXPECT_EQ(NormalizePath("crt\\shaders\\royale"), "crt/shaders/royale");
}

TEST(SlangPreset, NormalizePathCollapsesDotDotAcrossWindowsSeparators)
{
  EXPECT_EQ(NormalizePath("E:\\a\\b\\..\\c.slang"), "E:/a/c.slang");
  EXPECT_EQ(NormalizePath("E:\\Sys\\Shaders\\crt/../misc/x.slang"), "E:/Sys/Shaders/misc/x.slang");
}

TEST(SlangPreset, NormalizePathKeepsWindowsDriveRoot)
{
  // ".." must never eat the drive designator.
  EXPECT_EQ(NormalizePath("E:\\..\\..\\x"), "E:/x");
  EXPECT_EQ(NormalizePath("E:/"), "E:/");
}

TEST(SlangPreset, NormalizePathKeepsUncRoot)
{
  EXPECT_EQ(NormalizePath("\\\\host\\share\\..\\a"), "//host/a");
}

TEST(SlangPreset, NormalizePathPreservesPosixBehavior)
{
  EXPECT_EQ(NormalizePath("/root/preset/../b.slang"), "/root/b.slang");
  EXPECT_EQ(NormalizePath("/root/./a//b"), "/root/a/b");
  EXPECT_EQ(NormalizePath("/../a"), "/a");
  EXPECT_EQ(NormalizePath("a/../../b"), "../b");
  EXPECT_EQ(NormalizePath(""), "");
}

TEST(SlangPreset, ResolvesPassPathsFromWindowsBaseDir)
{
  const std::string text = "shaders = \"1\"\n"
                           "shader0 = \"../misc/x.slang\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "E:\\Sys\\Shaders\\crt", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  ASSERT_EQ(cfg->passes.size(), 1u);
  EXPECT_EQ(cfg->passes[0].shader_path, "E:/Sys/Shaders/misc/x.slang");
}

TEST(SlangPreset, ResolvesAbsoluteShaderPathIgnoringBaseDir)
{
  const std::string text = "shaders = \"1\"\n"
                           "shader0 = \"/abs/x.slang\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "/root/preset", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  EXPECT_EQ(cfg->passes[0].shader_path, "/abs/x.slang");
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='SlangPreset.*'
```

Expected: `NormalizePathAcceptsWindowsSeparators`, `…WindowsSeparators`, `…DriveRoot`, `…UncRoot`, `ResolvesPassPathsFromWindowsBaseDir` and `ResolvesAbsoluteShaderPathIgnoringBaseDir` FAIL; `NormalizePathPreservesPosixBehavior` and all pre-existing `SlangPreset.*` tests PASS.

- [ ] **Step 3: Add the root-splitting helper**

In `Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp`, insert **above** `NormalizePath` (i.e. after the `namespace VideoCommon {` on line 15):

```cpp
namespace
{
constexpr bool IsSeparator(char c)
{
  return c == '/' || c == '\\';
}

// The leading part of a path that ".." must never escape.
struct PathRoot
{
  size_t length = 0;  // bytes of the input consumed
  std::string text;   // normalized rendering: "", "/", "//" or "X:/"
  bool absolute = false;
};

// Recognizes a POSIX root ("/"), a UNC root ("//host" -> "//" + "host" segment) and a Windows
// drive root ("X:" / "X:\"). A drive-relative path ("X:foo") is promoted to drive-absolute:
// preset paths are always fully qualified, and promoting keeps ".." off the drive designator.
PathRoot SplitRoot(std::string_view path)
{
  if (path.size() >= 2 && IsSeparator(path[0]) && IsSeparator(path[1]))
    return {2, "//", true};
  if (path.size() >= 2 && path[1] == ':' &&
      ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
  {
    const size_t length = (path.size() >= 3 && IsSeparator(path[2])) ? 3 : 2;
    return {length, std::string(path.substr(0, 2)) + '/', true};
  }
  if (!path.empty() && IsSeparator(path[0]))
    return {1, "/", true};
  return {};
}
}  // namespace
```

- [ ] **Step 4: Rewrite `NormalizePath`**

Replace the whole body of `NormalizePath` (`SlangPreset.cpp:17-58`, comment included) with:

```cpp
// Lexically normalizes a path, collapsing "." and ".." segments without touching the filesystem.
// Both '/' and '\' are accepted as separators and the result always uses '/', which every
// platform's file APIs understand -- this is what makes Windows preset paths (backslashes, drive
// letters) resolve correctly. A leading "/", "//" (UNC) or "X:/" (drive) is preserved, and ".."
// can never escape it.
std::string NormalizePath(const std::string& path)
{
  const PathRoot root = SplitRoot(path);
  const std::string_view view = std::string_view(path).substr(root.length);
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (start <= view.size())
  {
    size_t end = start;
    while (end < view.size() && !IsSeparator(view[end]))
      ++end;
    const std::string_view seg = view.substr(start, end - start);
    if (seg.empty() || seg == ".")
    {
      // skip empty and current-dir segments
    }
    else if (seg == "..")
    {
      if (!parts.empty() && parts.back() != "..")
        parts.pop_back();
      else if (!root.absolute)
        parts.push_back(seg);
    }
    else
    {
      parts.push_back(seg);
    }
    if (end == view.size())
      break;
    start = end + 1;
  }

  std::string result = root.text;
  for (size_t i = 0; i < parts.size(); ++i)
  {
    if (i != 0)
      result += '/';
    result += parts[i];
  }
  return result;
}
```

- [ ] **Step 5: Make `ResolvePath` respect absolute values, and fix the local `DirectoryOf`**

`SlangPreset.cpp:90-93` becomes:

```cpp
std::string ResolvePath(const std::string& base_dir, const std::string& value)
{
  // An absolute shader/reference path stands on its own.
  if (SplitRoot(value).absolute)
    return NormalizePath(value);
  return NormalizePath(base_dir + "/" + value);
}
```

`SlangPreset.cpp:159` — accept both separators (it only ever sees normalized paths today, but the
trap costs nothing to disarm):

```cpp
  const auto slash = path.find_last_of("/\\");
```

- [ ] **Step 6: Update the header comment**

`Source/Core/VideoCommon/PostProcessing/SlangPreset.h`, replacing the `NormalizePath` comment:

```cpp
// Lexically normalizes a path, collapsing '.' and '..'. Accepts '/' and '\' as separators and
// always emits '/'. Preserves a leading '/', '//' (UNC) or 'X:/' (Windows drive) root.
std::string NormalizePath(const std::string& path);
```

- [ ] **Step 7: Run the tests to verify they pass**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='SlangPreset.*:RetroCrisis*'
```

Expected: all PASS.

- [ ] **Step 8: Write the failing include-expansion test**

`SlangShader.cpp`'s private `JoinPath`/`DirectoryOf` are still `/`-only, so a Windows shader
directory breaks `#include` resolution. Append to
`Source/UnitTests/VideoCommon/PostProcessing/SlangShaderTest.cpp` (match the file's existing
reader-stub style — read the top of the file first and reuse its stub rather than inventing one):

```cpp
TEST(SlangShader, ExpandsIncludesFromWindowsDirectory)
{
  const std::map<std::string, std::string> files = {
      {"E:/shaders/inc/common.h", "float common_value = 1.0;\n"},
  };
  const auto reader = [&files](const std::string& path, std::string* out) {
    const auto it = files.find(path);
    if (it == files.end())
      return false;
    *out = it->second;
    return true;
  };
  const std::string expanded =
      ExpandSlangIncludes("#include \"inc/common.h\"\n", "E:\\shaders", reader);
  EXPECT_NE(expanded.find("common_value"), std::string::npos);
}
```

If `ExpandSlangIncludes`'s real signature or reader type differs, adapt the call — do **not** change the production signature.

- [ ] **Step 9: Run it to verify it fails**

```
./build-qt/Binaries/Tests/tests --gtest_filter='SlangShader.*'
```

Expected: FAIL — the include is left in place because `DirectoryOf("E:\\shaders")` returns `""` and the join produces `inc/common.h`.

- [ ] **Step 10: Fix `SlangShader.cpp`'s path helpers**

```cpp
std::string JoinPath(const std::string& dir, const std::string& name)
{
  if (dir.empty())
    return name;
  return NormalizePath(dir + "/" + name);
}

std::string DirectoryOf(const std::string& path)
{
  const auto slash = path.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : path.substr(0, slash);
}
```

`NormalizePath` comes from `SlangPreset.h`; add `#include "VideoCommon/PostProcessing/SlangPreset.h"` if it is not already included.

- [ ] **Step 11: Run the full suite**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

Expected: 87 tests, 80+ passed, 1 skipped, **0 failed**.

- [ ] **Step 12: Commit** (show the message in chat first)

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp \
        Source/Core/VideoCommon/PostProcessing/SlangPreset.h \
        Source/Core/VideoCommon/PostProcessing/SlangShader.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/SlangPresetTest.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/SlangShaderTest.cpp
git commit
```

---

### Task 2: Decide the clip-space Y flip from the API type

`SlangTranslator` emits the Y negation under `#ifdef API_VULKAN`. The OGL backend defines **no**
`API_*` macro in its GLSL header, so OpenGL never flips and renders the whole chain upside down.
Dolphin's own precedent (`FramebufferShaderGen.cpp:191`) flips for Vulkan **and** OpenGL.

**Files:**
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangTranslator.h` (add helper, extend `TranslateSlangPass`)
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangTranslator.cpp:259` (signature), `:429-440` (the inject)
- Modify: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp:342` (call site), `:511-513` (passthrough pipeline)
- Test: `Source/UnitTests/VideoCommon/PostProcessing/SlangTranslatorTest.cpp` (+ mechanical call-site updates in `SlangCompileTest.cpp:121,154,218`)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces:
  - `constexpr bool VideoCommon::SlangNeedsClipYFlip(APIType api_type)` — used again by Task 5's `MipChainBuilder`.
  - `TranslatedPass TranslateSlangPass(const SlangShaderSource& shader, const std::vector<std::string>& known_aliases, const std::vector<std::string>& lut_names, bool flip_clip_y)` — Task 10 calls this with each backend's flip value.

- [ ] **Step 1: Write the failing tests**

Append to `Source/UnitTests/VideoCommon/PostProcessing/SlangTranslatorTest.cpp`:

```cpp
TEST(SlangTranslator, ClipYFlipMatchesFramebufferShaderGen)
{
  // Same rule as FramebufferShaderGen::GenerateScreenQuadVertexShader.
  EXPECT_TRUE(SlangNeedsClipYFlip(APIType::Vulkan));
  EXPECT_TRUE(SlangNeedsClipYFlip(APIType::OpenGL));
  EXPECT_FALSE(SlangNeedsClipYFlip(APIType::D3D));
  EXPECT_FALSE(SlangNeedsClipYFlip(APIType::Metal));
}

TEST(SlangTranslator, EmitsClipYFlipWhenRequested)
{
  const auto result = TranslateSlangPass(MakeShader(""), {}, {}, /*flip_clip_y=*/true);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_NE(result.vertex_glsl.find("Position.y = -Position.y;"), std::string::npos);
  // The flip is now decided at translate time, not by a backend shader macro.
  EXPECT_EQ(result.vertex_glsl.find("API_VULKAN"), std::string::npos);
}

TEST(SlangTranslator, OmitsClipYFlipWhenNotRequested)
{
  const auto result = TranslateSlangPass(MakeShader(""), {}, {}, /*flip_clip_y=*/false);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.vertex_glsl.find("Position.y = -Position.y;"), std::string::npos);
  EXPECT_EQ(result.vertex_glsl.find("API_VULKAN"), std::string::npos);
}
```

- [ ] **Step 2: Run them to verify they fail**

```
ninja -C build-qt unittests
```

Expected: **compile error** — `SlangNeedsClipYFlip` undeclared and `TranslateSlangPass` takes 3 arguments. That is the failing state for this task.

- [ ] **Step 3: Add the helper and extend the signature**

In `Source/Core/VideoCommon/PostProcessing/SlangTranslator.h`, add `#include "VideoCommon/VideoCommon.h"` (for `APIType`) and, above `TranslateSlangPass`:

```cpp
// True when the injected fullscreen-triangle vertex shader must negate clip-space Y. NDC Y is
// flipped in Vulkan; we also flip on OpenGL so that (0,0) is the lower-left. Mirrors
// FramebufferShaderGen::GenerateScreenQuadVertexShader -- keep the two in sync.
constexpr bool SlangNeedsClipYFlip(APIType api_type)
{
  return api_type == APIType::Vulkan || api_type == APIType::OpenGL;
}
static_assert(SlangNeedsClipYFlip(APIType::Vulkan));
static_assert(SlangNeedsClipYFlip(APIType::OpenGL));
static_assert(!SlangNeedsClipYFlip(APIType::D3D));
static_assert(!SlangNeedsClipYFlip(APIType::Metal));
```

Then extend the declaration:

```cpp
// flip_clip_y: see SlangNeedsClipYFlip. Callers pass SlangNeedsClipYFlip(g_backend_info.api_type).
TranslatedPass TranslateSlangPass(const SlangShaderSource& shader,
                                  const std::vector<std::string>& known_aliases,
                                  const std::vector<std::string>& lut_names, bool flip_clip_y);
```

- [ ] **Step 4: Emit the flip unconditionally or not at all**

`SlangTranslator.cpp:259` — mirror the new parameter onto the definition. Then replace the inject
(`:429-440`):

```cpp
  const std::string flip = flip_clip_y ? "  Position.y = -Position.y;\n" : "";
  const std::string inject =
      "  vec2 dolphin_fsq = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
      "  vec4 Position = vec4(dolphin_fsq * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);\n"
      "  vec2 TexCoord = dolphin_fsq;\n" +
      flip;
```

- [ ] **Step 5: Update the production call site and the passthrough pipeline**

`MultipassPostProcessing.cpp:342`:

```cpp
    TranslatedPass translated = TranslateSlangPass(*parsed, known_aliases, lut_names,
                                                   SlangNeedsClipYFlip(g_backend_info.api_type));
```

`MultipassPostProcessing.cpp:508-513` — the same bug in the passthrough blit; replace the comment
and the `flip_y` initializer:

```cpp
  // Fullscreen-triangle vertex shader + a plain textured copy. Uses Dolphin's per-backend
  // shader macros (defined by the backend header CreateShaderFromSource prepends), so it works
  // for any backbuffer format -- unlike ScaleTexture, which only supports RGBA8 targets.
  const std::string flip_y = SlangNeedsClipYFlip(g_backend_info.api_type) ?
                                 "  gl_Position.y = -gl_Position.y;\n" :
                                 "";
```

- [ ] **Step 6: Update the remaining test call sites (mechanical)**

`SlangCompileTest.cpp:121,154` → append `, /*flip_clip_y=*/true` (that file's oracle is the Vulkan
header). `:218` → same. `SlangTranslatorTest.cpp:30,46,76,92,171,179` → append
`, /*flip_clip_y=*/false` (those tests assert on sampler/uniform layout, not on the flip).

- [ ] **Step 7: Run the tests to verify they pass**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='SlangTranslator.*:SlangCompile.*:SlangSamplers.*'
```

Expected: all PASS.

- [ ] **Step 8: Commit** (show the message in chat first)

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangTranslator.h \
        Source/Core/VideoCommon/PostProcessing/SlangTranslator.cpp \
        Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/SlangTranslatorTest.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/SlangCompileTest.cpp
git commit
```

---

### Task 3: Mip level math + a backend capability flag

Turn the hardcoded `api_type == APIType::Vulkan` mip gate into a capability flag and move the level
arithmetic into a tested pure helper. Behavior is deliberately **unchanged** by this task (only
Vulkan sets the flag); Tasks 4 and 5 light up the other backends.

**Files:**
- Modify: `Source/Core/VideoCommon/PostProcessing/MipGen.h` (add `MipLevelCount`, `MipLevelSize`)
- Modify: `Source/Core/VideoCommon/VideoConfig.h:190` (add `bSupportsGPUMipGeneration`)
- Modify: `Source/Core/VideoBackends/Vulkan/VulkanContext.cpp:440` area (set the flag)
- Modify: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp:412-441`
- Test: `Source/UnitTests/VideoCommon/PostProcessing/MipGenTest.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `constexpr u32 VideoCommon::MipLevelCount(u32 width, u32 height)` — full chain length down to 1×1.
  - `constexpr u32 VideoCommon::MipLevelSize(u32 size, u32 level)` — one dimension at `level`, floor-1.
  - `bool BackendInfo::bSupportsGPUMipGeneration` — Task 4 sets it on Metal/OGL/D3D11.
  Task 5 uses both helpers.

- [ ] **Step 1: Write the failing test**

Append to `Source/UnitTests/VideoCommon/PostProcessing/MipGenTest.cpp`:

```cpp
TEST(MipGen, MipLevelCountCountsDownToOneByOne)
{
  EXPECT_EQ(MipLevelCount(1, 1), 1u);
  EXPECT_EQ(MipLevelCount(2, 1), 2u);
  EXPECT_EQ(MipLevelCount(256, 256), 9u);
  EXPECT_EQ(MipLevelCount(640, 480), 10u);
  // Degenerate sizes still describe a single level.
  EXPECT_EQ(MipLevelCount(0, 0), 1u);
}

TEST(MipGen, MipLevelSizeHalvesAndClampsToOne)
{
  EXPECT_EQ(MipLevelSize(640, 0), 640u);
  EXPECT_EQ(MipLevelSize(640, 1), 320u);
  EXPECT_EQ(MipLevelSize(480, 1), 240u);
  EXPECT_EQ(MipLevelSize(640, 10), 1u);
  EXPECT_EQ(MipLevelSize(1, 5), 1u);
}
```

- [ ] **Step 2: Run it to verify it fails**

```
ninja -C build-qt unittests
```

Expected: **compile error** — `MipLevelCount` / `MipLevelSize` undeclared.

- [ ] **Step 3: Add the helpers**

In `Source/Core/VideoCommon/PostProcessing/MipGen.h`, add `#include <algorithm>` and, above
`GenerateBoxMips`:

```cpp
// Number of mip levels in a full chain for a width x height level-0 image, i.e. down to 1x1.
constexpr u32 MipLevelCount(u32 width, u32 height)
{
  u32 levels = 1;
  for (u32 dim = std::max(width, height); dim > 1; dim >>= 1)
    ++levels;
  return levels;
}
static_assert(MipLevelCount(1, 1) == 1);
static_assert(MipLevelCount(2, 1) == 2);
static_assert(MipLevelCount(256, 256) == 9);
static_assert(MipLevelCount(640, 480) == 10);

// One dimension of `level`, halving each step and clamping at 1 (the usual GPU convention).
constexpr u32 MipLevelSize(u32 size, u32 level)
{
  const u32 shifted = size >> level;
  return shifted > 1 ? shifted : 1;
}
static_assert(MipLevelSize(640, 0) == 640);
static_assert(MipLevelSize(640, 1) == 320);
static_assert(MipLevelSize(640, 10) == 1);
static_assert(MipLevelSize(1, 5) == 1);
```

- [ ] **Step 4: Run it to verify it passes**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='MipGen.*'
```

Expected: all PASS.

- [ ] **Step 5: Add the capability flag**

`Source/Core/VideoCommon/VideoConfig.h`, after `bSupportsUnrestrictedDepthRange` (line 190):

```cpp
  // AbstractTexture::GenerateMipmaps() actually fills the chain on this backend. When false,
  // VideoCommon falls back to MipChainBuilder's draw-based path.
  bool bSupportsGPUMipGeneration = false;
```

`Source/Core/VideoBackends/Vulkan/VulkanContext.cpp` — next to the existing
`backend_info->bSupportsPostProcessing = true;`:

```cpp
  backend_info->bSupportsGPUMipGeneration = true;  // VKTexture::GenerateMipmaps blits the chain.
```

- [ ] **Step 6: Switch the gate and the level count (mechanical — no new test)**

This is a mechanical substitution guarded by the Step 1 tests plus the existing `PassSizing` suite;
the gate itself needs a live `g_backend_info` and a GPU, so it has no unit test. Called out
explicitly per the TDD policy.

`MultipassPostProcessing.cpp:412-415`:

```cpp
  // Passes that a later pass samples with mipmapping need a real mip chain. Backends that can
  // generate one on the GPU do so in AbstractTexture::GenerateMipmaps(); the rest go through
  // MipChainBuilder (see the mip generation call site below).
  const bool mips_supported = g_backend_info.bSupportsGPUMipGeneration;
```

`MultipassPostProcessing.cpp:436-441`:

```cpp
  const u32 levels =
      (pass.generate_mips && mips_supported) ? VideoCommon::MipLevelCount(out_w, out_h) : 1;
```

Add `#include "VideoCommon/PostProcessing/MipGen.h"` if absent.

- [ ] **Step 7: Verify no regressions**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

Expected: 0 failed. Then build the app to catch backend compile breaks:
`ninja -C build-qt dolphin-emu`.

- [ ] **Step 8: Commit** (show the message in chat first)

```bash
git add Source/Core/VideoCommon/PostProcessing/MipGen.h \
        Source/Core/VideoCommon/VideoConfig.h \
        Source/Core/VideoBackends/Vulkan/VulkanContext.cpp \
        Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/MipGenTest.cpp
git commit
```

---

### Task 4: Native `GenerateMipmaps` on Metal, OpenGL and D3D11

Each of these three backends generates a mip chain with a single API call. D3D11 additionally needs
the resource created with `D3D11_RESOURCE_MISC_GENERATE_MIPS`.

**No unit tests are possible** — these are GPU driver calls behind virtual overrides, and the test
binary has no device. TDD is skipped deliberately; verification is the on-hardware check in Step 8
and Task 11. Called out explicitly per the TDD policy.

**Files:**
- Modify: `Source/Core/VideoBackends/Metal/MTLTexture.h` / `MTLTexture.mm`
- Modify: `Source/Core/VideoBackends/OGL/OGLTexture.h` / `OGLTexture.cpp`
- Modify: `Source/Core/VideoBackends/D3D/DXTexture.h` / `DXTexture.cpp:44-51`
- Modify: `Source/Core/VideoBackends/Metal/MTLUtil.mm:54` area, `OGL/OGLMain.cpp:122` area, `D3D/D3DMain.cpp:87` area (set `bSupportsGPUMipGeneration`)

**Interfaces:**
- Consumes: `BackendInfo::bSupportsGPUMipGeneration` (Task 3).
- Produces: nothing new; three `AbstractTexture::GenerateMipmaps()` overrides.

- [ ] **Step 1: Metal override**

`MTLTexture.h`, in `class Texture`, next to the other overrides:

```cpp
  void GenerateMipmaps() override;
```

`MTLTexture.mm` — follow the file's existing blit idiom (see `Texture::CopyRectangleFromTexture`):

```cpp
void Metal::Texture::GenerateMipmaps()
{
  if (GetLevels() <= 1)
    return;
  @autoreleasepool
  {
    g_state_tracker->EndRenderPass();
    id<MTLBlitCommandEncoder> blit = [g_state_tracker->GetRenderCmdBuf() blitCommandEncoder];
    [blit setLabel:@"Generate Mipmaps"];
    [blit generateMipmapsForTexture:m_tex];
    [blit endEncoding];
  }
}
```

- [ ] **Step 2: OpenGL override**

`OGLTexture.h`:

```cpp
  void GenerateMipmaps() override;
```

`OGLTexture.cpp` — bind to the scratch unit the rest of the file uses:

```cpp
void OGLTexture::GenerateMipmaps()
{
  if (GetLevels() <= 1)
    return;
  glActiveTexture(GL_MUTABLE_TEXTURE_INDEX);
  glBindTexture(m_target, m_texId);
  glGenerateMipmap(m_target);
}
```

Check the member names against the file (it exposes `GetGLTarget()` / `GetGLTextureId()`; use
whichever spelling the neighbouring methods use).

- [ ] **Step 3: D3D11 — request the mip-generation capability at create time**

`DXTexture.cpp:44-51`, replacing the inline misc-flags ternary:

```cpp
  UINT misc_flags =
      config.type == AbstractTextureType::Texture_CubeMap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
  // ID3D11DeviceContext::GenerateMips only works on resources created with this flag; it needs
  // both RENDER_TARGET and SHADER_RESOURCE bind flags, which render targets already have.
  if (config.IsRenderTarget() && config.levels > 1)
    misc_flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
  CD3D11_TEXTURE2D_DESC desc(tex_format, config.width, config.height, config.layers, config.levels,
                             bindflags, D3D11_USAGE_DEFAULT, 0, config.samples, 0, misc_flags);
```

- [ ] **Step 4: D3D11 override**

`DXTexture.h`:

```cpp
  void GenerateMipmaps() override;
```

`DXTexture.cpp`:

```cpp
void DXTexture::GenerateMipmaps()
{
  if (GetLevels() <= 1)
    return;
  D3D::context->GenerateMips(m_srv.Get());
}
```

The SRV already spans every level (`CreateSRV` passes `m_config.levels`), so nothing else changes.

- [ ] **Step 5: Advertise the capability**

Add next to each backend's existing `bSupportsPostProcessing = true;`:

- `Source/Core/VideoBackends/Metal/MTLUtil.mm`: `backend_info->bSupportsGPUMipGeneration = true;`
- `Source/Core/VideoBackends/OGL/OGLMain.cpp`: `g_backend_info.bSupportsGPUMipGeneration = true;`
- `Source/Core/VideoBackends/D3D/D3DMain.cpp`: `g_backend_info.bSupportsGPUMipGeneration = true;`

Match each file's existing spelling (`backend_info->` vs `g_backend_info.`).

- [ ] **Step 6: Build**

```
ninja -C build-qt dolphin-emu && ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

Expected: builds clean, 0 test failures. (macOS builds Metal + OGL + Vulkan; D3D11 is compiled on
the Windows host in Task 11.)

- [ ] **Step 7: Verify on Metal and OpenGL**

Requires a game and the libretro pack installed (Graphics → Enhancements → Download…).

```
./build-qt/Binaries/Dolphin.app/Contents/MacOS/Dolphin -v Metal \
  -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
./build-qt/Binaries/Dolphin.app/Contents/MacOS/Dolphin -v OGL \
  -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
```

Expected: crt-royale renders with visible halation/bloom (soft glow around bright scanlines) and is
**right side up** on both — the latter is Task 2's fix. Compare against `-v Vulkan`, which was
already correct. If the mip chain were still empty the glow would be missing or blocky.

Optional verification aid (do **not** commit): temporarily add
`texture->Save("/tmp/mip3.png", 3);` after the `GenerateMipmaps()` call in
`MultipassPostProcessing` and confirm the file is a plausible ⅛-scale image rather than black.

- [ ] **Step 8: Commit** (show the message in chat first)

```bash
git add Source/Core/VideoBackends/Metal/MTLTexture.h Source/Core/VideoBackends/Metal/MTLTexture.mm \
        Source/Core/VideoBackends/Metal/MTLUtil.mm \
        Source/Core/VideoBackends/OGL/OGLTexture.h Source/Core/VideoBackends/OGL/OGLTexture.cpp \
        Source/Core/VideoBackends/OGL/OGLMain.cpp \
        Source/Core/VideoBackends/D3D/DXTexture.h Source/Core/VideoBackends/D3D/DXTexture.cpp \
        Source/Core/VideoBackends/D3D/D3DMain.cpp
git commit
```

---

### Task 5: Portable draw-based mip fallback (covers D3D12)

D3D12 has no built-in mip generator, and a native one would need per-mip RTVs/SRVs plus
per-subresource barriers that `DXTexture::TransitionToState` cannot express. Instead put a portable
builder in VideoCommon: for level *k*, draw `textureLod(src, uv, k-1)` into a scratch single-level
render target, then copy the scratch's top-left `w_k × h_k` sub-rect 1:1 into the real mip level.
Source and destination of the draw are always different resources, so there is no hazard. **D3D12
itself needs no changes.**

**Files:**
- Create: `Source/Core/VideoCommon/MipChainBuilder.h`, `Source/Core/VideoCommon/MipChainBuilder.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt` (add both files)
- Modify: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.h` (own a builder), `.cpp:412-415` (gate), `:848-853` (call site)

**Interfaces:**
- Consumes: `VideoCommon::MipLevelCount`, `VideoCommon::MipLevelSize` (Task 3); `VideoCommon::SlangNeedsClipYFlip` (Task 2); `BackendInfo::bSupportsGPUMipGeneration` (Task 3).
- Produces: `class VideoCommon::MipChainBuilder` with `bool Generate(AbstractTexture* texture)`.

- [ ] **Step 1: Write the header**

`Source/Core/VideoCommon/MipChainBuilder.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTexture.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;

namespace VideoCommon
{
// Fills mip levels 1..N-1 of a render-target texture with successive half-size filtered copies,
// for backends where AbstractTexture::GenerateMipmaps() is a no-op (see
// BackendInfo::bSupportsGPUMipGeneration). Each level is drawn into a scratch single-level target
// and then copied 1:1 into the real level, so a draw never reads and writes the same resource --
// which is what keeps this free of per-subresource barriers on D3D12.
//
// Resources are created lazily and reused across frames; one scratch target sized to level 1
// (a quarter of level 0) serves every level.
class MipChainBuilder
{
public:
  MipChainBuilder();
  ~MipChainBuilder();

  // Returns false if the pipeline or scratch target could not be created, in which case the
  // texture's mip levels are left as they were. Safe to call with a single-level texture (no-op).
  bool Generate(AbstractTexture* texture);

private:
  bool EnsurePipeline(AbstractTextureFormat format);
  bool EnsureScratch(u32 width, u32 height, AbstractTextureFormat format);

  std::unique_ptr<AbstractShader> m_vertex_shader;
  std::unique_ptr<AbstractShader> m_pixel_shader;
  std::unique_ptr<AbstractPipeline> m_pipeline;
  AbstractTextureFormat m_pipeline_format = AbstractTextureFormat::Undefined;
  std::unique_ptr<AbstractTexture> m_scratch;
  std::unique_ptr<AbstractFramebuffer> m_scratch_framebuffer;
  bool m_failed = false;  // set after a creation failure so we don't retry every frame
};
}  // namespace VideoCommon
```

- [ ] **Step 2: Write the implementation**

`Source/Core/VideoCommon/MipChainBuilder.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/MipChainBuilder.h"

#include <array>
#include <string>

#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/PostProcessing/MipGen.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
MipChainBuilder::MipChainBuilder() = default;
MipChainBuilder::~MipChainBuilder() = default;

bool MipChainBuilder::EnsurePipeline(AbstractTextureFormat format)
{
  if (m_pipeline && m_pipeline_format == format)
    return true;

  // Same fullscreen-triangle idiom as MultipassPostProcessing::BuildPassthroughPipeline, so it
  // works for any render-target format (ScaleTexture is RGBA8-only).
  const std::string flip_y = SlangNeedsClipYFlip(g_backend_info.api_type) ?
                                 "  gl_Position.y = -gl_Position.y;\n" :
                                 "";
  const std::string vertex_source =
      "VARYING_LOCATION(0) out float2 v_tex0;\n"
      "void main() {\n"
      "  v_tex0 = float2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
      "  gl_Position = float4(v_tex0 * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n" +
      flip_y + "}\n";
  const char* const pixel_source =
      "UBO_BINDING(std140, 1) uniform PSBlock { float4 src_lod; };\n"
      "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
      "VARYING_LOCATION(0) in float2 v_tex0;\n"
      "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n"
      "void main() {\n"
      "  ocol0 = textureLod(samp0, float3(v_tex0, 0.0), src_lod.x);\n"
      "}\n";

  m_vertex_shader = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, vertex_source, nullptr,
                                                  "mip chain vertex");
  m_pixel_shader =
      g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pixel_source, nullptr, "mip chain pixel");
  m_pipeline.reset();
  if (!m_vertex_shader || !m_pixel_shader)
    return false;

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_vertex_shader.get();
  config.pixel_shader = m_pixel_shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(format);
  config.usage = AbstractPipelineUsage::Utility;
  m_pipeline = g_gfx->CreatePipeline(config);
  m_pipeline_format = format;
  return m_pipeline != nullptr;
}

bool MipChainBuilder::EnsureScratch(u32 width, u32 height, AbstractTextureFormat format)
{
  if (m_scratch && m_scratch->GetWidth() >= width && m_scratch->GetHeight() >= height &&
      m_scratch->GetFormat() == format)
  {
    return true;
  }

  m_scratch_framebuffer.reset();
  const TextureConfig config(width, height, 1, 1, 1, format, AbstractTextureFlag_RenderTarget,
                             AbstractTextureType::Texture_2DArray);
  m_scratch = g_gfx->CreateTexture(config, "mip chain scratch");
  if (!m_scratch)
    return false;
  m_scratch_framebuffer = g_gfx->CreateFramebuffer(m_scratch.get(), nullptr);
  return m_scratch_framebuffer != nullptr;
}

bool MipChainBuilder::Generate(AbstractTexture* texture)
{
  const u32 levels = texture->GetLevels();
  if (levels <= 1)
    return true;
  if (m_failed)
    return false;

  const AbstractTextureFormat format = texture->GetFormat();
  if (!EnsurePipeline(format) ||
      !EnsureScratch(MipLevelSize(texture->GetWidth(), 1), MipLevelSize(texture->GetHeight(), 1),
                     format))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create mip chain builder resources; mipmapped post-processing "
                         "passes will sample an incomplete chain.");
    m_failed = true;
    return false;
  }

  g_gfx->BeginUtilityDrawing();
  for (u32 level = 1; level < levels; ++level)
  {
    const u32 width = MipLevelSize(texture->GetWidth(), level);
    const u32 height = MipLevelSize(texture->GetHeight(), level);
    const MathUtil::Rectangle<int> rect(0, 0, static_cast<int>(width), static_cast<int>(height));

    // Read the level we just produced; it must be sampleable before we bind it.
    texture->FinishedRendering();

    const std::array<float, 4> uniforms = {static_cast<float>(level - 1), 0.0f, 0.0f, 0.0f};
    g_vertex_manager->UploadUtilityUniforms(uniforms.data(), sizeof(uniforms));

    g_gfx->SetFramebuffer(m_scratch_framebuffer.get());
    g_gfx->SetViewportAndScissor(rect);
    g_gfx->SetPipeline(m_pipeline.get());
    g_gfx->SetTexture(0, texture);
    g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
    g_gfx->Draw(0, 3);

    m_scratch->FinishedRendering();
    texture->CopyRectangleFromTexture(m_scratch.get(), rect, 0, 0, rect, 0, level);
  }
  g_gfx->EndUtilityDrawing();
  texture->FinishedRendering();
  return true;
}
}  // namespace VideoCommon
```

Notes for the implementer:
- `textureLod` with an explicit LOD makes the sampler's mip filter irrelevant, so the linear
  sampler gives a clean 2×2 box average when the destination is exactly half-size.
- Sampling `texture` while writing `m_scratch` is safe; copying scratch → `texture` level *k* then
  sampling level *k* next iteration is a plain write-then-read the backend already serializes.
- If `SetTexture`/`SetSamplerState`/`Draw` are not the exact `AbstractGfx` spellings in this tree,
  mirror `AbstractGfx::ScaleTexture` (`AbstractGfx.cpp:110-150`) verbatim.

- [ ] **Step 3: Register with CMake**

`Source/Core/VideoCommon/CMakeLists.txt` — add `MipChainBuilder.cpp` and `MipChainBuilder.h`
alongside the other VideoCommon sources, keeping the list's alphabetical order.

- [ ] **Step 4: Wire it into `MultipassPostProcessing`**

`MultipassPostProcessing.h` — add the include (`#include "VideoCommon/MipChainBuilder.h"`) and a
member next to the other owned resources:

```cpp
  // Used on backends where AbstractTexture::GenerateMipmaps() is a no-op.
  VideoCommon::MipChainBuilder m_mip_builder;
```

`MultipassPostProcessing.cpp:412-415` — both paths now exist, so mipmapped passes always get a
chain:

```cpp
  // Passes sampled with mipmapping always get a real chain: backends that can generate one on the
  // GPU do it in AbstractTexture::GenerateMipmaps(), the rest go through MipChainBuilder.
  constexpr bool mips_supported = true;
```

`MultipassPostProcessing.cpp:848-853`:

```cpp
    // A later pass samples this output with mipmapping: build its mip chain now, from the level-0
    // content just rendered.
    if (pass.generate_mips && pass.output_texture)
    {
      if (g_backend_info.bSupportsGPUMipGeneration)
        pass.output_texture->GenerateMipmaps();
      else
        m_mip_builder.Generate(pass.output_texture.get());
    }
```

If `constexpr bool mips_supported = true;` trips an "unused variable"/"condition is always true"
warning, drop the variable and the `&& mips_supported` in the level calculation instead.

- [ ] **Step 5: Build and test**

```
ninja -C build-qt dolphin-emu && ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

Expected: builds clean, 0 failures.

- [ ] **Step 6: Exercise the fallback path on Metal**

Temporarily force the fallback so it can be verified without a D3D12 device: in
`Source/Core/VideoBackends/Metal/MTLUtil.mm`, flip the Task 4 line to
`backend_info->bSupportsGPUMipGeneration = false;`, rebuild, and run:

```
./build-qt/Binaries/Dolphin.app/Contents/MacOS/Dolphin -v Metal \
  -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
```

Expected: visually indistinguishable from the native path in Task 4 Step 7 — same halation, no
vertical mirroring of the glow, no flicker. **Revert the forced `false` before committing** and
re-run once to confirm the native path is back.

- [ ] **Step 7: Commit** (show the message in chat first)

```bash
git add Source/Core/VideoCommon/MipChainBuilder.h Source/Core/VideoCommon/MipChainBuilder.cpp \
        Source/Core/VideoCommon/CMakeLists.txt \
        Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.h \
        Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp
git commit
```

---

### Task 6: Remove the dead graphics controls

Output resampling and colour correction have no consumers left anywhere in the engine, and
`StereoMode::Anaglyph` / `StereoMode::Passive` now cost a second render layer for no visible effect.
**The HDR checkbox stays** — `g_Config.bHDR` still selects the swapchain colour space on D3D,
Vulkan and Metal (see the spec's B4). The `Config::Info` entries themselves stay too; only the UI
and the now-unreachable dialog go, so existing INI files keep round-tripping.

Removing entries from the stereo combo changes the index→value mapping that plain `ConfigChoice`
relies on (index 3 would become "HDMI 3D" but still save `3` = `Anaglyph`), so the combo must move
to `ConfigChoiceMap<StereoMode>`, which stores explicit values.

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.h` (drop `m_output_resampling_combo`, `m_configure_color_correction`, `ConfigureColorCorrection`; retype `m_3d_mode`)
- Modify: `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.cpp` (creation, layout, connect, `OnBackendChanged`, descriptions, titles, `ConfigureColorCorrection`)
- Delete: `Source/Core/DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.{h,cpp}`
- Modify: `Source/Core/DolphinQt/CMakeLists.txt:110-111`
- Modify: `Source/Core/DolphinQt/HotkeyScheduler.cpp:593-604` (dead anaglyph hotkey handler)

**Interfaces:**
- Consumes: nothing.
- Produces: `m_3d_mode` is now `ConfigChoiceMap<StereoMode>*`. Task 8 touches the same widget's post-processing row, so land Task 6 first.

- [ ] **Step 1: Confirm the dialog has no other callers**

```
grep -rn "ColorCorrectionConfigWindow" Source/
```

Expected: only `EnhancementsWidget.cpp`, `CMakeLists.txt`, and the two files themselves. If anything
else appears, stop and report instead of deleting.

- [ ] **Step 2: Remove the widgets and their layout rows**

In `EnhancementsWidget.cpp::CreateWidgets`, delete the `m_output_resampling_combo` construction
(lines 141-144) and the `m_configure_color_correction` construction (~line 146). In the layout
block (~186-197), delete the two rows:

```cpp
  enhancements_layout->addWidget(new QLabel(tr("Output Resampling:")), row, 0);
  enhancements_layout->addWidget(m_output_resampling_combo, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Color Correction:")), row, 0);
  enhancements_layout->addWidget(m_configure_color_correction, row, 1, 1, -1);
  ++row;
```

(the exact label text/spans are in the file — remove whichever rows reference the two widgets, and
keep the surrounding `++row` bookkeeping consistent so no row index is skipped).

- [ ] **Step 3: Retype the stereo combo and drop the dead modes**

`EnhancementsWidget.h`: `ConfigChoice* m_3d_mode;` → `ConfigChoiceMap<StereoMode>* m_3d_mode;`, and
add `#include "VideoCommon/VideoConfig.h"` for `StereoMode` if it is not already reachable.

`EnhancementsWidget.cpp:220-222`:

```cpp
  // Anaglyph and Passive were implemented by the old post-processing shader, which no longer
  // exists; selecting them renders a second layer for no visible effect. ConfigChoiceMap stores
  // explicit values, so the remaining entries keep their StereoMode meanings.
  m_3d_mode = new ConfigChoiceMap<StereoMode>({{tr("Off"), StereoMode::Off},
                                               {tr("Side-by-Side"), StereoMode::SideBySide},
                                               {tr("Top-and-Bottom"), StereoMode::TopAndBottom},
                                               {tr("HDMI 3D"), StereoMode::QuadBuffer}},
                                              Config::GFX_STEREO_MODE, m_game_layer);
```

- [ ] **Step 4: Fix `ConnectWidgets`, `OnBackendChanged`, `AddDescriptions` and the titles**

- `ConnectWidgets` (~263-293): delete the `m_configure_color_correction` → `ConfigureColorCorrection`
  connection. Keep the `m_3d_mode` `currentIndexChanged` connection as-is.
- `OnBackendChanged` (~326-330): delete these two lines; keep the `m_hdr` line.

```cpp
  m_output_resampling_combo->setEnabled(g_backend_info.bSupportsPostProcessing);
  m_configure_color_correction->setEnabled(g_backend_info.bSupportsPostProcessing);
```

- `AddDescriptions`: delete `TR_OUTPUT_RESAMPLING_DESCRIPTION` (~430-460) and
  `TR_COLOR_CORRECTION_DESCRIPTION` (~461-463) and their `SetTitle`/`SetDescription` calls
  (~546-547 and the colour-correction pair). Keep `TR_HDR_DESCRIPTION` and
  `TR_POSTPROCESSING_DESCRIPTION`.
- In `TR_3D_MODE_DESCRIPTION` (~493), drop the "Anaglyph is used for Red-Cyan colored glasses."
  sentence so the help text matches the offered modes.
- Delete `ConfigureColorCorrection()` (~585-589) and its declaration, plus the
  `#include "DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.h"`.

- [ ] **Step 5: Delete the dialog and deregister it**

```bash
git rm Source/Core/DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.cpp \
       Source/Core/DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.h
```

Then remove lines 110-111 from `Source/Core/DolphinQt/CMakeLists.txt`.

- [ ] **Step 6: Remove the dead anaglyph hotkey handler**

`HotkeyScheduler.cpp` — delete the whole `if (IsHotkey(HK_TOGGLE_STEREO_ANAGLYPH))` block
(~593-604). Leave the `HK_TOGGLE_STEREO_ANAGLYPH` enumerator and its `HotkeyManager.cpp` string in
place: removing an enumerator renumbers every later hotkey and would silently rebind users'
existing configs. Add a comment where the block was:

```cpp
      // HK_TOGGLE_STEREO_ANAGLYPH intentionally has no handler: StereoMode::Anaglyph was
      // implemented by the removed post-processing shader. The enumerator stays so hotkey IDs
      // in existing user configs keep their meaning.
```

Also check whether `DUBOIS_ALGORITHM_SHADER` still has a reference after the deletion (the
top-and-bottom block above uses it); if it is now unused, delete the constant too.

- [ ] **Step 7: Build and eyeball the UI**

```
ninja -C build-qt dolphin-emu && ./build-qt/Binaries/Dolphin.app/Contents/MacOS/Dolphin
```

Expected: Graphics → Enhancements shows no Output Resampling row and no Color Correction row; HDR
Post-Processing is still present and still enabled on Vulkan/Metal; the Stereoscopic 3D Mode combo
lists exactly Off / Side-by-Side / Top-and-Bottom / HDMI 3D. Set Top-and-Bottom, close and reopen
the window: the selection must persist (proves the `ConfigChoiceMap` value mapping).

- [ ] **Step 8: Run the tests**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

Expected: 0 failures (no test covers Qt; this is a regression guard).

- [ ] **Step 9: Commit** (show the message in chat first)

```bash
git add -A Source/Core/DolphinQt
git commit
```

---

### Task 7: Shared chain/category logic and the RetroCrisis profile list

The Android picker's semantics — split a `;`-separated chain, join it with arrows, derive categories
from the leading directory, append vs replace — are currently Kotlin-only. Task 8's Qt dialog needs
the same rules, so extract them into a pure, tested VideoCommon helper rather than reimplementing
them in Qt. Also give the RetroCrisis profile names a C++ home so Qt does not need to copy Android's
`strings.xml` array.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.h`, `ShaderChainSpec.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`
- Modify: `Source/Core/VideoCommon/PostProcessing/RetroCrisisInstall.h`, `RetroCrisisInstall.cpp` (add `GetRetroCrisisProfiles`)
- Create: `Source/UnitTests/VideoCommon/PostProcessing/ShaderChainSpecTest.cpp`
- Modify: `Source/UnitTests/VideoCommon/CMakeLists.txt` (add `ShaderChainSpecTest`)
- Modify: `Source/UnitTests/VideoCommon/PostProcessing/RetroCrisisInstallTest.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces (all used by Task 8):
  - `std::vector<std::string> SplitChainSpec(std::string_view spec)`
  - `std::string JoinChainSpec(const std::vector<std::string>& presets)`
  - `std::string DescribeChainSpec(std::string_view spec)` — arrow-joined, `""` when empty
  - `std::string AppendToChainSpec(std::string_view spec, std::string_view preset)`
  - `std::string ChainCategoryOf(std::string_view preset)`
  - `std::vector<std::string> ChainCategories(const std::vector<std::string>& presets)`
  - `std::vector<std::string> PresetsInCategory(const std::vector<std::string>& presets, std::string_view category)`
  - `const std::vector<std::string>& GetRetroCrisisProfiles()` (in `RetroCrisisInstall.h`)

- [ ] **Step 1: Write the failing test**

`Source/UnitTests/VideoCommon/PostProcessing/ShaderChainSpecTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/ShaderChainSpec.h"

using namespace VideoCommon;

TEST(ShaderChainSpec, SplitsAndIgnoresEmptyEntries)
{
  EXPECT_EQ(SplitChainSpec(""), (std::vector<std::string>{}));
  EXPECT_EQ(SplitChainSpec("crt/a.slangp"), (std::vector<std::string>{"crt/a.slangp"}));
  EXPECT_EQ(SplitChainSpec("crt/a.slangp;misc/b.slangp"),
            (std::vector<std::string>{"crt/a.slangp", "misc/b.slangp"}));
  EXPECT_EQ(SplitChainSpec(";crt/a.slangp;;"), (std::vector<std::string>{"crt/a.slangp"}));
}

TEST(ShaderChainSpec, JoinsWithSemicolons)
{
  EXPECT_EQ(JoinChainSpec({}), "");
  EXPECT_EQ(JoinChainSpec({"a", "b"}), "a;b");
}

TEST(ShaderChainSpec, DescribesChainWithArrows)
{
  EXPECT_EQ(DescribeChainSpec(""), "");
  EXPECT_EQ(DescribeChainSpec("crt/a.slangp"), "crt/a.slangp");
  EXPECT_EQ(DescribeChainSpec("crt/a.slangp;misc/b.slangp"), "crt/a.slangp \xE2\x86\x92 misc/b.slangp");
}

TEST(ShaderChainSpec, AppendsToChain)
{
  EXPECT_EQ(AppendToChainSpec("", "a"), "a");
  EXPECT_EQ(AppendToChainSpec("a", "b"), "a;b");
  EXPECT_EQ(AppendToChainSpec("a;b", "c"), "a;b;c");
  // Appending nothing is a no-op rather than a trailing separator.
  EXPECT_EQ(AppendToChainSpec("a", ""), "a");
}

TEST(ShaderChainSpec, DerivesCategoryFromLeadingDirectory)
{
  EXPECT_EQ(ChainCategoryOf("crt/crt-royale.slangp"), "crt");
  EXPECT_EQ(ChainCategoryOf("crt/nested/x.slangp"), "crt");
  // A preset at the root has no category.
  EXPECT_EQ(ChainCategoryOf("plain.slangp"), "");
}

TEST(ShaderChainSpec, ListsDistinctSortedCategories)
{
  const std::vector<std::string> presets = {"misc/b.slangp", "crt/a.slangp", "crt/c.slangp",
                                            "root.slangp"};
  EXPECT_EQ(ChainCategories(presets), (std::vector<std::string>{"crt", "misc"}));
}

TEST(ShaderChainSpec, FiltersPresetsByCategory)
{
  const std::vector<std::string> presets = {"misc/b.slangp", "crt/a.slangp", "root.slangp"};
  EXPECT_EQ(PresetsInCategory(presets, "crt"), (std::vector<std::string>{"crt/a.slangp"}));
  // An empty category means "all".
  EXPECT_EQ(PresetsInCategory(presets, ""), presets);
}
```

Register it in `Source/UnitTests/VideoCommon/CMakeLists.txt` next to the other
`PostProcessing/…Test` entries:

```cmake
add_dolphin_test(ShaderChainSpecTest PostProcessing/ShaderChainSpecTest.cpp)
```

(copy the exact macro form used by the neighbouring lines).

- [ ] **Step 2: Run it to verify it fails**

```
ninja -C build-qt unittests
```

Expected: **compile error** — `ShaderChainSpec.h` does not exist.

- [ ] **Step 3: Write the header**

`Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace VideoCommon
{
// GFX_ENHANCE_POST_SHADER holds a chain of preset paths separated by ';'. These helpers are the
// single source of truth for how that string is built, displayed and filtered, shared by the Qt
// and Android front ends.
constexpr char CHAIN_SEPARATOR = ';';

// The chain's entries, in order, with empty entries dropped.
std::vector<std::string> SplitChainSpec(std::string_view spec);

// Inverse of SplitChainSpec.
std::string JoinChainSpec(const std::vector<std::string>& presets);

// Human-readable rendering: entries joined with " -> " (U+2192). Empty for an empty chain; the
// caller supplies its own localized "off" label.
std::string DescribeChainSpec(std::string_view spec);

// Appends one preset to the chain. Appending an empty preset returns the chain unchanged.
std::string AppendToChainSpec(std::string_view spec, std::string_view preset);

// The leading directory of a preset path, or "" for a preset at the shaders root.
std::string ChainCategoryOf(std::string_view preset);

// Distinct, sorted, non-empty categories present in `presets`.
std::vector<std::string> ChainCategories(const std::vector<std::string>& presets);

// The subset of `presets` in `category`, preserving input order. An empty category returns
// everything.
std::vector<std::string> PresetsInCategory(const std::vector<std::string>& presets,
                                           std::string_view category);
}  // namespace VideoCommon
```

- [ ] **Step 4: Write the implementation**

`Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/ShaderChainSpec.h"

#include <algorithm>
#include <set>

namespace VideoCommon
{
namespace
{
constexpr std::string_view ARROW = " \xE2\x86\x92 ";  // U+2192 RIGHTWARDS ARROW
}  // namespace

std::vector<std::string> SplitChainSpec(std::string_view spec)
{
  std::vector<std::string> presets;
  size_t start = 0;
  while (start <= spec.size())
  {
    const auto sep = spec.find(CHAIN_SEPARATOR, start);
    const auto end = sep == std::string_view::npos ? spec.size() : sep;
    if (end > start)
      presets.emplace_back(spec.substr(start, end - start));
    if (sep == std::string_view::npos)
      break;
    start = end + 1;
  }
  return presets;
}

std::string JoinChainSpec(const std::vector<std::string>& presets)
{
  std::string spec;
  for (const std::string& preset : presets)
  {
    if (!spec.empty())
      spec += CHAIN_SEPARATOR;
    spec += preset;
  }
  return spec;
}

std::string DescribeChainSpec(std::string_view spec)
{
  std::string description;
  for (const std::string& preset : SplitChainSpec(spec))
  {
    if (!description.empty())
      description += ARROW;
    description += preset;
  }
  return description;
}

std::string AppendToChainSpec(std::string_view spec, std::string_view preset)
{
  if (preset.empty())
    return std::string(spec);
  std::vector<std::string> presets = SplitChainSpec(spec);
  presets.emplace_back(preset);
  return JoinChainSpec(presets);
}

std::string ChainCategoryOf(std::string_view preset)
{
  const auto slash = preset.find('/');
  return slash == std::string_view::npos ? std::string() : std::string(preset.substr(0, slash));
}

std::vector<std::string> ChainCategories(const std::vector<std::string>& presets)
{
  std::set<std::string> categories;
  for (const std::string& preset : presets)
  {
    std::string category = ChainCategoryOf(preset);
    if (!category.empty())
      categories.insert(std::move(category));
  }
  return {categories.begin(), categories.end()};
}

std::vector<std::string> PresetsInCategory(const std::vector<std::string>& presets,
                                           std::string_view category)
{
  if (category.empty())
    return presets;
  std::vector<std::string> filtered;
  for (const std::string& preset : presets)
  {
    if (ChainCategoryOf(preset) == category)
      filtered.push_back(preset);
  }
  return filtered;
}
}  // namespace VideoCommon
```

Add `PostProcessing/ShaderChainSpec.cpp` and `.h` to `Source/Core/VideoCommon/CMakeLists.txt`.

- [ ] **Step 5: Run it to verify it passes**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='ShaderChainSpec.*'
```

Expected: 7 tests PASS.

- [ ] **Step 6: Write the failing RetroCrisis profile test**

Append to `Source/UnitTests/VideoCommon/PostProcessing/RetroCrisisInstallTest.cpp`:

```cpp
TEST(RetroCrisisInstall, ProfileListMatchesPackFolders)
{
  const auto& profiles = GetRetroCrisisProfiles();
  ASSERT_EQ(profiles.size(), 7u);
  EXPECT_EQ(profiles.front(), "1080p Flat");
  EXPECT_EQ(profiles.back(), "720p Steam Deck");
  // Every profile must be a name RetroCrisisProfileOf can recover from a preset path.
  for (const std::string& profile : profiles)
    EXPECT_EQ(RetroCrisisProfileOf(profile + "/x.slangp"), profile);
}
```

- [ ] **Step 7: Run it to verify it fails, then implement**

```
ninja -C build-qt unittests
```

Expected: compile error — `GetRetroCrisisProfiles` undeclared.

`RetroCrisisInstall.h`:

```cpp
// The display profiles the RetroCrisis pack ships, in menu order. These are literal folder names
// inside the pack, so they must match InstallRetroCrisisProfile's expectations exactly. Android's
// R.array.post_processing_retrocrisis_profiles mirrors this list.
const std::vector<std::string>& GetRetroCrisisProfiles();
```

`RetroCrisisInstall.cpp`:

```cpp
const std::vector<std::string>& GetRetroCrisisProfiles()
{
  static const std::vector<std::string> profiles = {
      "1080p Flat",   "1440p Flat",   "4K Flat",         "1080p Curved",
      "1440p Curved", "4K Curved",    "720p Steam Deck",
  };
  return profiles;
}
```

Before writing the list, confirm the exact folder names by checking how
`InstallRetroCrisisProfile`/`RetroCrisisProfileOf` match them and by cross-checking
`Source/Android/app/src/main/res/values/strings.xml:249-257`. If they disagree, the pack's own
folder names win — fix the test expectation, not the pack.

- [ ] **Step 8: Run the tests**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

Expected: 0 failures.

- [ ] **Step 9: Commit** (show the message in chat first)

```bash
git add Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.h \
        Source/Core/VideoCommon/PostProcessing/ShaderChainSpec.cpp \
        Source/Core/VideoCommon/PostProcessing/RetroCrisisInstall.h \
        Source/Core/VideoCommon/PostProcessing/RetroCrisisInstall.cpp \
        Source/Core/VideoCommon/CMakeLists.txt \
        Source/UnitTests/VideoCommon/CMakeLists.txt \
        Source/UnitTests/VideoCommon/PostProcessing/ShaderChainSpecTest.cpp \
        Source/UnitTests/VideoCommon/PostProcessing/RetroCrisisInstallTest.cpp
git commit
```

---

### Task 8: Qt post-processing parity — renderer toggle, chain picker, pack registry

Android offers a Builtin/librashader renderer choice, a category → preset picker with **Select** and
**Add to Chain**, an arrow-joined chain summary, and three registry-driven downloads. Qt has one flat
combo and one hardcoded URL. All the backing logic already exists in VideoCommon
(`GetPresetList`, `GetShaderPackSources`, `DownloadShaderPackById`, plus Task 7's helpers), so this
task is Qt wiring only.

Qt has no test harness in this repo, so there are no unit tests here; the logic that *can* be tested
was extracted in Task 7. Called out explicitly per the TDD policy. Verification is Step 7.

**Files:**
- Create: `Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.h`, `.cpp`
- Modify: `Source/Core/DolphinQt/CMakeLists.txt` (register both)
- Modify: `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.h`, `.cpp`

**Interfaces:**
- Consumes: Task 7's `ShaderChainSpec` helpers and `GetRetroCrisisProfiles()`; Task 6's cleaned-up widget.
- Produces: `PostProcessingChainDialog(QWidget* parent, QString chain_spec)` with `QString ChainSpec() const`.

- [ ] **Step 1: Write the dialog header**

`Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

// Picks post-processing presets by category and either replaces the chain or appends to it,
// mirroring the Android picker (SettingsFragmentPresenter.showPostShaderList).
class PostProcessingChainDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit PostProcessingChainDialog(QWidget* parent, QString chain_spec);

  // The chain as edited. Only meaningful after the dialog was accepted.
  QString ChainSpec() const { return m_chain_spec; }

private:
  void CreateWidgets();
  void ConnectWidgets();
  void RefreshCategories();
  void RefreshPresets();
  void UpdateChainLabel();
  void UpdateButtons();
  std::string SelectedPreset() const;

  QComboBox* m_category_combo;
  QListWidget* m_preset_list;
  QLabel* m_chain_label;
  QPushButton* m_select_button;
  QPushButton* m_add_button;

  std::vector<std::string> m_presets;  // every preset, unfiltered
  QString m_chain_spec;
};
```

- [ ] **Step 2: Write the dialog implementation**

`Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/PostProcessingChainDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"
#include "VideoCommon/PostProcessing/ShaderChainSpec.h"

PostProcessingChainDialog::PostProcessingChainDialog(QWidget* parent, QString chain_spec)
    : QDialog(parent), m_chain_spec(std::move(chain_spec))
{
  setWindowTitle(tr("Post-Processing Chain"));
  m_presets = MultipassPostProcessing::GetPresetList();
  CreateWidgets();
  ConnectWidgets();
  RefreshCategories();
  RefreshPresets();
  UpdateChainLabel();
  UpdateButtons();
}

void PostProcessingChainDialog::CreateWidgets()
{
  m_category_combo = new QComboBox(this);
  m_preset_list = new QListWidget(this);
  m_chain_label = new QLabel(this);
  m_chain_label->setWordWrap(true);
  m_select_button = new QPushButton(tr("Select"), this);
  m_add_button = new QPushButton(tr("Add to Chain"), this);

  auto* const buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
  buttons->addButton(m_select_button, QDialogButtonBox::AcceptRole);
  buttons->addButton(m_add_button, QDialogButtonBox::ActionRole);

  auto* const layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(tr("Category:"), this));
  layout->addWidget(m_category_combo);
  layout->addWidget(m_preset_list);
  layout->addWidget(m_chain_label);
  layout->addWidget(buttons);
  setLayout(layout);

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PostProcessingChainDialog::ConnectWidgets()
{
  connect(m_category_combo, &QComboBox::currentIndexChanged, this, [this] {
    RefreshPresets();
    UpdateButtons();
  });
  connect(m_preset_list, &QListWidget::currentRowChanged, this,
          &PostProcessingChainDialog::UpdateButtons);
  connect(m_preset_list, &QListWidget::itemDoubleClicked, this, [this] { m_select_button->click(); });
  connect(m_select_button, &QPushButton::clicked, this, [this] {
    // "Select" replaces the whole chain, including with (off).
    m_chain_spec = QString::fromStdString(SelectedPreset());
    accept();
  });
  connect(m_add_button, &QPushButton::clicked, this, [this] {
    m_chain_spec = QString::fromStdString(VideoCommon::AppendToChainSpec(
        m_chain_spec.toStdString(), SelectedPreset()));
    accept();
  });
}

void PostProcessingChainDialog::RefreshCategories()
{
  const QSignalBlocker blocker(m_category_combo);
  m_category_combo->clear();
  m_category_combo->addItem(tr("All"), QString{});
  for (const std::string& category : VideoCommon::ChainCategories(m_presets))
  {
    const QString text = QString::fromStdString(category);
    m_category_combo->addItem(text, text);
  }
}

void PostProcessingChainDialog::RefreshPresets()
{
  const std::string category = m_category_combo->currentData().toString().toStdString();
  const QSignalBlocker blocker(m_preset_list);
  m_preset_list->clear();

  auto* const off = new QListWidgetItem(tr("(off)"), m_preset_list);
  off->setData(Qt::UserRole, QString{});

  // Highlight the chain's last entry, matching Android.
  const auto chain = VideoCommon::SplitChainSpec(m_chain_spec.toStdString());
  const std::string current = chain.empty() ? std::string() : chain.back();
  int current_row = 0;
  for (const std::string& preset : VideoCommon::PresetsInCategory(m_presets, category))
  {
    auto* const item = new QListWidgetItem(QString::fromStdString(preset), m_preset_list);
    item->setData(Qt::UserRole, QString::fromStdString(preset));
    if (preset == current)
      current_row = m_preset_list->row(item);
  }
  m_preset_list->setCurrentRow(current_row);
}

void PostProcessingChainDialog::UpdateChainLabel()
{
  const std::string description = VideoCommon::DescribeChainSpec(m_chain_spec.toStdString());
  m_chain_label->setText(tr("Current chain: %1")
                             .arg(description.empty() ? tr("(off)") :
                                                        QString::fromStdString(description)));
}

void PostProcessingChainDialog::UpdateButtons()
{
  // Appending "(off)" is meaningless, and there is nothing to append to an empty chain.
  m_add_button->setEnabled(!SelectedPreset().empty() && !m_chain_spec.isEmpty());
}

std::string PostProcessingChainDialog::SelectedPreset() const
{
  const QListWidgetItem* const item = m_preset_list->currentItem();
  return item ? item->data(Qt::UserRole).toString().toStdString() : std::string();
}
```

Check `MultipassPostProcessing::GetPresetList()`'s exact qualification and return type before
compiling (it may live in namespace `VideoCommon`); adapt the call rather than the definition.

- [ ] **Step 3: Register with CMake**

Add `Config/Graphics/PostProcessingChainDialog.cpp` and `.h` to
`Source/Core/DolphinQt/CMakeLists.txt`, in the slot the deleted `ColorCorrectionConfigWindow` lines
vacated (keeps the list sorted).

- [ ] **Step 4: Add the renderer combo and the chain button**

`EnhancementsWidget.h` — new members and slots:

```cpp
  ConfigChoiceMap<PostProcessRenderer>* m_post_process_renderer;
  QPushButton* m_configure_post_chain;
```

```cpp
  void ConfigurePostProcessingChain();
  void DownloadShaderPack(const std::string& pack_id, const std::string& profile);
```

`EnhancementsWidget.cpp::CreateWidgets` — next to the post-processing combo (~148-154):

```cpp
  m_post_process_renderer = new ConfigChoiceMap<PostProcessRenderer>(
      {{tr("Builtin"), PostProcessRenderer::Builtin},
       {tr("librashader"), PostProcessRenderer::Librashader}},
      Config::GFX_ENHANCE_POST_PROCESS_RENDERER, m_game_layer);
  m_configure_post_chain = new NonDefaultQPushButton(tr("Chain…"));
```

Layout: put `Post-Processing Renderer:` + `m_post_process_renderer` on the row above the effect
combo, and add `m_configure_post_chain` to the effect row (the row currently holding the label at
col 0, combo at col 1, `m_download_shader_pack` at col 2 → chain button at col 2, download at col 3).

- [ ] **Step 5: Show chains in the combo, and open the dialog**

`LoadPostProcessingShaders()` (~295-324) — after the flat list is populated, before
`m_post_processing_effect->Load();`, add the synthetic entry so a multi-preset chain is not shown as
a blank combo:

```cpp
  // A chain is not one of the listed presets; add it so the combo shows the current value.
  const std::string chain = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  if (chain.find(VideoCommon::CHAIN_SEPARATOR) != std::string::npos)
  {
    m_post_processing_effect->addItem(
        QString::fromStdString(VideoCommon::DescribeChainSpec(chain)),
        QString::fromStdString(chain));
  }
```

`ConnectWidgets()`:

```cpp
  connect(m_configure_post_chain, &QPushButton::clicked, this,
          &EnhancementsWidget::ConfigurePostProcessingChain);
```

New slot:

```cpp
void EnhancementsWidget::ConfigurePostProcessingChain()
{
  PostProcessingChainDialog dialog(
      this, QString::fromStdString(Config::Get(Config::GFX_ENHANCE_POST_SHADER)));
  if (dialog.exec() != QDialog::Accepted)
    return;
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, dialog.ChainSpec().toStdString());
  LoadPostProcessingShaders();
}
```

Use whichever setter `ShaderChanged()` already uses for this key (it handles the game-layer case);
match it rather than inventing a new path.

- [ ] **Step 6: Drive the downloads from the pack registry**

Turn `m_download_shader_pack` into a menu button and generalise the existing worker
(`DownloadShaderPack`, ~591-630) to take a pack id and profile:

```cpp
  auto* const menu = new QMenu(this);
  for (const VideoCommon::ShaderPackSource& source : VideoCommon::GetShaderPackSources())
  {
    const std::string id = source.id;
    QAction* const action = menu->addAction(QString::fromStdString(source.display_name));
    if (id == "retrocrisis")
    {
      connect(action, &QAction::triggered, this, [this, id] {
        QStringList profiles;
        for (const std::string& profile : VideoCommon::GetRetroCrisisProfiles())
          profiles << QString::fromStdString(profile);
        bool ok = false;
        const QString profile = QInputDialog::getItem(this, tr("Choose a display profile"),
                                                      tr("Profile:"), profiles, 0, false, &ok);
        if (ok)
          DownloadShaderPack(id, profile.toStdString());
      });
    }
    else
    {
      connect(action, &QAction::triggered, this, [this, id] { DownloadShaderPack(id, ""); });
    }
  }
  m_download_shader_pack->setMenu(menu);
```

In `DownloadShaderPack(pack_id, profile)`, replace the hardcoded
`DownloadAndInstallShaderPack(VideoCommon::SLANG_SHADER_PACK_URL, …)` call with

```cpp
    return VideoCommon::DownloadShaderPackById(pack_id, shaders_root, progress, profile);
```

keeping the surrounding `QProgressDialog` + `QtConcurrent::run` + `QFutureWatcher` + `QEventLoop`
structure and the closing `QMessageBox::information(...)` / `LoadPostProcessingShaders()` untouched.
Add the includes: `<QInputDialog>`, `<QMenu>`,
`"DolphinQt/Config/Graphics/PostProcessingChainDialog.h"`,
`"VideoCommon/PostProcessing/ShaderChainSpec.h"`,
`"VideoCommon/PostProcessing/ShaderPackSource.h"`,
`"VideoCommon/PostProcessing/RetroCrisisInstall.h"`.

- [ ] **Step 7: Gate the renderer combo in `OnBackendChanged`**

```cpp
  // librashader is loaded through the Vulkan backend only.
  const bool librashader_possible = g_backend_info.api_type == APIType::Vulkan;
  m_post_process_renderer->setEnabled(g_backend_info.bSupportsPostProcessing &&
                                      librashader_possible);
```

Add a description in `AddDescriptions()`:

```cpp
  static const char TR_POST_PROCESS_RENDERER_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which engine runs slang post-processing presets."
      "<br><br><b>Builtin</b>: Dolphin's own multipass renderer, available on every backend."
      "<br><b>librashader</b>: the upstream RetroArch shader runtime; Vulkan only."
      "<br><br><dolphin_emphasis>If unsure, select Builtin.</dolphin_emphasis>");
```

and wire `SetTitle`/`SetDescription` for it next to the other post-processing entries.

- [ ] **Step 8: Build and verify the UI**

```
ninja -C build-qt dolphin-emu && ./build-qt/Binaries/Dolphin.app/Contents/MacOS/Dolphin
```

Expected, in Graphics → Enhancements:
1. A **Post-Processing Renderer** combo showing Builtin/librashader, disabled unless the backend is Vulkan.
2. **Download…** is now a menu with one entry per `GetShaderPackSources()` entry; picking
   RetroCrisis prompts for one of the 7 profiles and then downloads. Each finishes with the
   "Installed N shader presets." box and the effect combo repopulates.
3. **Chain…** opens the dialog: the category combo lists "All" plus the pack's directories;
   selecting a category filters the list; the current chain's last entry is preselected;
   **Select** replaces the chain; **Add to Chain** is disabled when the chain is empty or "(off)"
   is highlighted, and otherwise appends.
4. After building a 2-preset chain, the effect combo shows `a → b` rather than going blank, and the
   value survives closing and reopening the window.
5. Launch a game with the chain active and confirm both presets render.

- [ ] **Step 9: Run the tests and commit** (show the message in chat first)

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests
```

```bash
git add Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.h \
        Source/Core/DolphinQt/Config/Graphics/PostProcessingChainDialog.cpp \
        Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.h \
        Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.cpp \
        Source/Core/DolphinQt/CMakeLists.txt
git commit
```

---

### Task 9: librashader desktop artifacts

`Externals/librashader/` ships headers plus an Android `arm64-v8a` `.so`. Desktop needs a macOS
arm64 dylib and a Windows x64 DLL, both `runtime-vulkan` only. `librashader_ld.h` loads by **bare
name**, which cannot find a dylib inside a macOS bundle, so the load macro must be overridable.

This is build/packaging work with no unit-testable logic; TDD is skipped deliberately (called out per
the TDD policy) and verified by the load check in Step 7.

**Files:**
- Create: `Externals/librashader/lib/macos-arm64/librashader.dylib`, `Externals/librashader/lib/windows-x64/librashader.dll`
- Modify: `Externals/librashader/include/librashader_ld.h:55,66,77` (guard the load macros)
- Modify: `Externals/librashader/README.md` (record the desktop build commands + the local patch)
- Create: `Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.h`, `.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`
- Modify: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp:1-40`
- Modify: `Source/Core/DolphinQt/CMakeLists.txt` (bundle/copy the artifact)

**Interfaces:**
- Consumes: nothing.
- Produces: `std::string VideoCommon::LibrashaderLibraryPath()` — absolute path on macOS/Windows, the bare file name elsewhere.

- [ ] **Step 1: Build the macOS arm64 dylib**

```bash
rustup target add aarch64-apple-darwin
git clone --branch librashader-cache-v0.12.0 --depth 1 \
    https://github.com/SnowflakePowered/librashader /tmp/librashader
cd /tmp/librashader
cargo build -p librashader-capi --release --target aarch64-apple-darwin \
    --no-default-features --features runtime-vulkan
```

Copy `target/aarch64-apple-darwin/release/liblibrashader_capi.dylib` to
`Externals/librashader/lib/macos-arm64/librashader.dylib`. Confirm the version and feature set match
the Android build recorded in `Externals/librashader/README.md` (ABI 2 / API 5) — a mismatch between
the vendored header and the binary is a silent-corruption bug, not a load failure. Verify:

```bash
file Externals/librashader/lib/macos-arm64/librashader.dylib   # arm64
otool -L Externals/librashader/lib/macos-arm64/librashader.dylib  # no unexpected deps
nm -gU Externals/librashader/lib/macos-arm64/librashader.dylib | grep libra_instance_create
```

- [ ] **Step 2: Build the Windows x64 DLL**

On `pcsx2-win`, inside a VS 18 developer prompt:

```
rustup target add x86_64-pc-windows-msvc
git clone --branch librashader-cache-v0.12.0 --depth 1 https://github.com/SnowflakePowered/librashader C:\src\librashader
cd /d C:\src\librashader
cargo build -p librashader-capi --release --target x86_64-pc-windows-msvc --no-default-features --features runtime-vulkan
```

Copy `target\x86_64-pc-windows-msvc\release\librashader_capi.dll` back to
`Externals/librashader/lib/windows-x64/librashader.dll` (via `scp` from the mac side). Record the
byte size of both artifacts in the commit message — they are ~13 MB each, so this commit
deliberately grows the repo by ~26 MB, consistent with the already-vendored Android `.so`.

- [ ] **Step 3: Make the load macro overridable**

In `Externals/librashader/include/librashader_ld.h`, wrap each of the three definitions
(lines 55, 66, 77):

```c
#ifndef _LIBRASHADER_LOAD
#define _LIBRASHADER_LOAD LoadLibraryW(L"librashader.dll")
#endif
```

…and likewise for the `dlopen("librashader.dylib", …)` and `dlopen("librashader.so", …)` variants.
Append a "Local modifications" section to `Externals/librashader/README.md` documenting this patch,
the two new desktop build commands, and the artifact paths — the file is the record of how these
binaries were produced.

- [ ] **Step 4: Add the path helper**

`Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace VideoCommon
{
// Where the librashader dynamic library is packaged for this platform. librashader_ld.h loads by
// bare name, which cannot find a library inside a macOS .app bundle, so we hand it an absolute
// path -- the same treatment VulkanLoader gives libMoltenVK. On platforms where the loader's
// default search already works (Android, Linux) this returns the bare file name.
std::string LibrashaderLibraryPath();
}  // namespace VideoCommon
```

`Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/LibrashaderLibrary.h"

#include "Common/FileUtil.h"

namespace VideoCommon
{
std::string LibrashaderLibraryPath()
{
#if defined(__APPLE__) && !defined(ANDROID)
  return File::GetBundleDirectory() + "/Contents/Frameworks/librashader.dylib";
#elif defined(_WIN32)
  return File::GetExeDirectory() + "\\librashader.dll";
#elif defined(ANDROID)
  return "librashader.so";
#else
  return "librashader.so";
#endif
}
}  // namespace VideoCommon
```

Both helpers are declared in `Common/FileUtil.h` (`GetBundleDirectory():247`,
`GetExeDirectory():251`). Register both new files in `Source/Core/VideoCommon/CMakeLists.txt`.

- [ ] **Step 5: Point the loader at it**

`Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp`, before the
`#include <librashader_ld.h>`:

```cpp
#include "VideoCommon/PostProcessing/LibrashaderLibrary.h"

// Override librashader_ld.h's bare-name load with the absolute packaged path. Android and Linux
// keep the header's default, which their loaders resolve correctly.
#if defined(_WIN32)
#include <windows.h>
#include "Common/StringUtil.h"
#define _LIBRASHADER_LOAD LoadLibraryW(UTF8ToWString(VideoCommon::LibrashaderLibraryPath()).c_str())
#elif defined(__APPLE__) && !defined(ANDROID)
#include <dlfcn.h>
#define _LIBRASHADER_LOAD dlopen(VideoCommon::LibrashaderLibraryPath().c_str(), RTLD_LAZY)
#endif

#define LIBRA_RUNTIME_VULKAN
#include <librashader_ld.h>
```

Keep the existing `#define LIBRA_RUNTIME_VULKAN` exactly where the file already has it — the macro
must precede the include. `UTF8ToWString(std::string_view)` is declared at `Common/StringUtil.h:246`
inside the `_WIN32` block, which is the only branch that uses it.

- [ ] **Step 6: Package the artifacts**

`Source/Core/DolphinQt/CMakeLists.txt` — immediately after the existing "Copy MoltenVK into the
bundle" block:

```cmake
  # Copy librashader into the bundle, next to MoltenVK (see LibrashaderLibraryPath).
  if(ENABLE_VULKAN AND APPLE)
    set(LIBRASHADER_DYLIB "${CMAKE_SOURCE_DIR}/Externals/librashader/lib/macos-arm64/librashader.dylib")
    if(EXISTS "${LIBRASHADER_DYLIB}")
      target_sources(dolphin-emu PRIVATE "${LIBRASHADER_DYLIB}")
      set_source_files_properties("${LIBRASHADER_DYLIB}" PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)
    else()
      message(STATUS "librashader.dylib not found; the librashader post-processing renderer will be unavailable.")
    endif()
  endif()
```

and, for Windows, next to the executable:

```cmake
  if(ENABLE_VULKAN AND WIN32)
    set(LIBRASHADER_DLL "${CMAKE_SOURCE_DIR}/Externals/librashader/lib/windows-x64/librashader.dll")
    if(EXISTS "${LIBRASHADER_DLL}")
      add_custom_command(TARGET dolphin-emu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBRASHADER_DLL}" "$<TARGET_FILE_DIR:dolphin-emu>")
      install(FILES "${LIBRASHADER_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
      message(STATUS "librashader.dll not found; the librashader post-processing renderer will be unavailable.")
    endif()
  endif()
```

The `EXISTS` guards keep a source checkout without the binaries buildable. Match the surrounding
block's indentation and the `install(...)` destination variable the file already uses.

- [ ] **Step 7: Verify the library actually loads**

macOS:

```bash
ninja -C build-qt dolphin-emu
ls -l build-qt/Binaries/Dolphin.app/Contents/Frameworks/librashader.dylib
./build-qt/Binaries/Dolphin.app/Contents/MacOS/Dolphin -v Vulkan \
  -C GFX.Enhancements.PostProcessRenderer=1 \
  -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game> 2>&1 | tee /tmp/librashader.log
```

Expected: the dylib is inside `Contents/Frameworks`, and the log shows librashader initialising with
no "failed to load"/"instance not loaded" error. Confirm the preset renders (compare against
`PostProcessRenderer=0`). Repeat on Windows in Task 11.

If `librashader_load_instance()` returns an instance whose function pointers are null, the header/ABI
and the binary disagree — rebuild at the exact tag in Step 1 rather than patching around it.

- [ ] **Step 8: Commit** (show the message in chat first; note the ~26 MB of binaries)

```bash
git add Externals/librashader Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.h \
        Source/Core/VideoCommon/PostProcessing/LibrashaderLibrary.cpp \
        Source/Core/VideoCommon/CMakeLists.txt \
        Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp \
        Source/Core/DolphinQt/CMakeLists.txt
git commit
```

---

### Task 10: Compile the translator's output against every backend's shader header

`SlangCompileTest` only ever compiled with the Vulkan GLSL header, so nothing caught that the D3D,
Metal and OGL paths even parse — which is exactly what Task 2 changed. glslang accepts
`APIType::OpenGL` (`Spirv.cpp:172` only adds Vulkan rules for Vulkan/Metal), so all four backends can
be exercised in the test binary with no GPU.

**Files:**
- Modify: `Source/UnitTests/VideoCommon/PostProcessing/SlangCompileTest.cpp`

**Interfaces:**
- Consumes: Task 2's `TranslateSlangPass(..., bool flip_clip_y)` and `SlangNeedsClipYFlip`.
- Produces: nothing.

- [ ] **Step 1: Replace the Vulkan-only oracle with a parameterized one**

In `SlangCompileTest.cpp`, keep `VULKAN_SHADER_HEADER` and add three more. Two are copied
**verbatim** from the backends' `SHADER_HEADER` string literals (both are GLSL — the D3D backend
compiles GLSL through SPIRV-Cross to HLSL, so its header is a GLSL preamble, not HLSL):

- `D3D_SHADER_HEADER` ← `Source/Core/VideoBackends/D3DCommon/Shader.cpp:36-66` (the
  `constexpr std::string_view SHADER_HEADER`, **not** `COMPUTE_SHADER_HEADER`)
- `METAL_SHADER_HEADER` ← `Source/Core/VideoBackends/Metal/MTLUtil.mm:385-425` (likewise the
  non-compute `SHADER_HEADER`)

OGL's header cannot be copied: `s_glsl_header` is assembled at backend init by the `fmt::format` at
`ProgramShaderCache.cpp:808` from runtime-capability-dependent pieces. Hand-write the modern-desktop
variant — explicit binding layout (`ProgramShaderCache.cpp:754-763`), the empty
`VARYING_LOCATION` (`:790`), the "silly differences" block (`:840-851`), no subgroup block:

```cpp
constexpr std::string_view OGL_SHADER_HEADER = R"(
  #version 450 core

  #extension GL_ARB_explicit_attrib_location : enable
  #define ATTRIBUTE_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
  #define UBO_BINDING(packing, x) layout(packing, binding = x)
  #define SAMPLER_BINDING(x) layout(binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(binding = x)
  #define SSBO_BINDING(x) layout(std430, binding = x)
  #define IMAGE_BINDING(format, x) layout(format, binding = x)

  // OGL deliberately leaves varying locations implicit; they match by name.
  #define VARYING_LOCATION(x)

  #define API_OPENGL 1
  #define float2 vec2
  #define float3 vec3
  #define float4 vec4
  #define uint2 uvec2
  #define uint3 uvec3
  #define uint4 uvec4
  #define int2 ivec2
  #define int3 ivec3
  #define int4 ivec4
  #define frac fract
  #define lerp mix
)";
```

The empty `VARYING_LOCATION` is the point of the OGL case: it proves the translator's varying names
match across stages without explicit locations. Add a comment above it recording that this header is
a hand-maintained mirror of `ProgramShaderCache.cpp`, so a future reader knows to re-check it.

Then:

```cpp
struct BackendShaderHeader
{
  const char* name;
  APIType api_type;
  const char* header;
};

constexpr BackendShaderHeader BACKEND_HEADERS[] = {
    {"Vulkan", APIType::Vulkan, VULKAN_SHADER_HEADER},
    {"D3D", APIType::D3D, D3D_SHADER_HEADER},
    {"Metal", APIType::Metal, METAL_SHADER_HEADER},
    {"OpenGL", APIType::OpenGL, OGL_SHADER_HEADER},
};

// Compiles both stages of a translated pass with one backend's GLSL preamble; returns true only if
// both succeed, and on failure *which_failed names the backend and stage.
bool CompilesOnBackend(const TranslatedPass& pass, const BackendShaderHeader& backend,
                       std::string* which_failed)
{
  const auto lang = glslang::EShTargetSpv_1_0;
  const std::string vs_src = std::string(backend.header) + "\n" + pass.vertex_glsl;
  const std::string fs_src = std::string(backend.header) + "\n" + pass.fragment_glsl;
  const auto vs = SPIRV::CompileVertexShader(vs_src, backend.api_type, lang, nullptr);
  if (!vs)
  {
    *which_failed = std::string(backend.name) + " vertex";
    return false;
  }
  const auto fs = SPIRV::CompileFragmentShader(fs_src, backend.api_type, lang, nullptr);
  if (!fs)
  {
    *which_failed = std::string(backend.name) + " fragment";
    return false;
  }
  return true;
}
```

This is `CompilesOnVulkan` with the two hardcoded `APIType::Vulkan` arguments and the header replaced
by `backend`; `SPIRV::CompileVertexShader`/`CompileFragmentShader` return
`std::optional<CodeVector>`, so the truthiness checks stay as they are today. Delete
`CompilesOnVulkan` once all three tests use the new oracle.

- [ ] **Step 2: Rewrite the three tests to loop over the backends**

Each existing test currently ends with a translate-and-compile tail. Wrap that tail — not the shader
text, which stays a single `const std::string text = ...` above the loop — in a backend loop.
`StockShaderCompilesOnVulkan` becomes:

```cpp
TEST(SlangCompile, StockShaderCompilesOnAllBackends)
{
  const std::string text = /* unchanged stock .slang text */;

  std::string error;
  const auto parsed = ParseSlangShader(text, &error);
  ASSERT_TRUE(parsed.has_value()) << error;

  for (const BackendShaderHeader& backend : BACKEND_HEADERS)
  {
    SCOPED_TRACE(backend.name);
    const auto translated =
        TranslateSlangPass(*parsed, {}, {}, SlangNeedsClipYFlip(backend.api_type));
    ASSERT_TRUE(translated.ok) << translated.error;

    std::string which;
    EXPECT_TRUE(CompilesOnBackend(translated, backend, &which))
        << which << " stage failed to compile:\nVS:\n"
        << translated.vertex_glsl << "\nFS:\n"
        << translated.fragment_glsl;
  }
}
```

Note the translate call is *inside* the loop: the flip differs per backend, so each backend gets its
own translation. Parsing is backend-independent and stays outside.

Apply the same transformation to `CompatMacrosCompileOnVulkan` →
`CompatMacrosCompileOnAllBackends`, and to the `SLANG_PRESET`-gated `RealPresetCompilesAllPasses` —
there the loop goes *inside* the existing per-pass loop, around the
`TranslateSlangPass`/`CompilesOnVulkan` tail, so it covers every pass × every backend. Keep each
test's existing shader text, `ExpandSlangIncludes` call and skip logic untouched. Also fix that
file's local `DirName` (`SlangCompileTest.cpp:173`) to `find_last_of("/\\")` so the env-var path
works on Windows.

- [ ] **Step 3: Run them**

```
ninja -C build-qt unittests && ./build-qt/Binaries/Tests/tests --gtest_filter='SlangCompile.*'
```

Expected: PASS on all four backends. If a header genuinely cannot compile the translated GLSL, that
is a real finding — investigate with `superpowers:systematic-debugging` and report before changing
the test to accommodate it.

- [ ] **Step 4: Run the real-preset sweep**

```
SLANG_PRESET="$HOME/Library/Application Support/Dolphin/Load/Shaders/shaders_slang/crt/crt-royale.slangp" \
  ./build-qt/Binaries/Tests/tests --gtest_filter='SlangCompile.RealPreset*'
```

Expected: PASS (no longer skipped). Fix the path to wherever the libretro pack installed.

- [ ] **Step 5: Commit** (show the message in chat first)

```bash
git add Source/UnitTests/VideoCommon/PostProcessing/SlangCompileTest.cpp
git commit
```

---

### Task 11: Cross-platform verification

Everything above is verified per-task on macOS. This task closes the loop on Windows — where the
whole point of Task 1 lives, and where D3D11/D3D12 first get compiled — and produces the results
document.

**Files:**
- Create: `docs/superpowers/specs/2026-09-16-desktop-postprocessing-parity-results.md`

- [ ] **Step 1: Sync the branch to the Windows host**

```bash
git push origin feature/desktop-postprocessing-parity
ssh pcsx2-win "cmd /c cd /d E:\work\dolphin && git fetch origin && git checkout feature/desktop-postprocessing-parity && git pull && git submodule update --init --recursive"
```

- [ ] **Step 2: Configure and build on Windows**

```bash
ssh pcsx2-win "cmd /c \"call \"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat\" && cd /d E:\work\dolphin && \"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe\" -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release && \"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe\" --build build-win\""
```

Remember the remote shell is `cmd.exe`: no `head`, `grep` or pipes-to-POSIX-tools. Use
`type`/`findstr` if you need to inspect output remotely.

- [ ] **Step 3: Run the unit tests on Windows**

```bash
ssh pcsx2-win "cmd /c cd /d E:\work\dolphin && build-win\Binaries\Tests\tests.exe"
```

Expected: same counts as macOS, 0 failures. This is the first execution of the Task 1 path
normalization on a real Windows filesystem.

- [ ] **Step 4: Verify presets load on all four Windows backends**

Install the libretro pack through the UI once, then for each of `D3D`, `D3D12`, `Vulkan`, `OGL`:

```
build-win\Binaries\Dolphin.exe -v <backend> -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
```

Expected for each: the preset loads (no "failed to parse"/"missing shader" panic — the pre-fix
failure mode), renders right side up, and shows halation/bloom. D3D12 exercises Task 5's
`MipChainBuilder`; D3D11 exercises Task 4's `GenerateMips`. Confirm the log has no
`Failed to create mip chain builder resources`.

- [ ] **Step 5: Verify librashader on Windows**

```
build-win\Binaries\Dolphin.exe -v Vulkan -C GFX.Enhancements.PostProcessRenderer=1 -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
```

Expected: `librashader.dll` sits next to `Dolphin.exe`, the instance loads, and the preset renders.

- [ ] **Step 6: Verify the Qt UI on Windows**

Spot-check Task 6 and Task 8 on Windows: no Output Resampling / Color Correction rows, four stereo
modes, the renderer combo enabled only on Vulkan, the download menu (including a RetroCrisis profile
install), and a 2-preset chain built through the dialog rendering correctly.

- [ ] **Step 7: Write the results document**

`docs/superpowers/specs/2026-09-16-desktop-postprocessing-parity-results.md`, following the shape of
`2026-09-13-thor-driver-and-postfx-perf-results.md`: one section per spec success criterion, the
platform/backend matrix actually exercised, measured frame-time impact of the mip path on D3D12 vs
D3D11 if available, and anything left open (expected: HDR paper-white scaling, librashader Metal
runtime, Android's profile list still duplicated in `strings.xml`).

- [ ] **Step 8: Full verification sweep and finish**

Use `superpowers:verification-before-completion`: macOS `ninja -C build-qt unittests` +
`./build-qt/Binaries/Tests/tests` green, Windows `tests.exe` green, and every spec success criterion
either demonstrated or explicitly listed as open. Then `superpowers:finishing-a-development-branch`
and `superpowers:requesting-code-review`.

- [ ] **Step 9: Commit** (show the message in chat first)

```bash
git add docs/superpowers/specs/2026-09-16-desktop-postprocessing-parity-results.md
git commit
```

---

## Self-Review

**Spec coverage:**

| Spec item | Task |
| --- | --- |
| B1 Windows path resolution | 1 |
| B2 mipmapped passes Vulkan-only | 3 (capability + math), 4 (Metal/OGL/D3D11), 5 (D3D12 fallback) |
| B3 OpenGL vertical flip | 2 (translator + passthrough pipeline) |
| B4 dead Qt controls | 6 |
| B5 librashader unavailable on desktop | 9 |
| B6 Qt UI behind Android | 7 (shared logic), 8 (UI) |
| §3 verification gap (Vulkan-only compile oracle) | 10 |
| §5 all four backends incl. D3D12 | 4 + 5 |
| §5 macOS dylib + Windows DLL, Vulkan runtime | 9 |
| §6 D1 normalize in `NormalizePath` | 1 Steps 3-6 |
| §6 D2 flip from `APIType` | 2 |
| §6 D3 native where cheap, portable fallback | 4, 5 |
| §6 D4 reuse pack registry / preset list | 7, 8 |
| §6 D5 absolute-path loading | 9 Steps 3-5 |
| §7.1-§7.6 success criteria | 11 |

**Corrections applied during self-review** (each verified against the tree, both documents updated):
- The spec's B3 claimed the OGL GLSL header defines no `API_OPENGL` macro. It does
  (`ProgramShaderCache.cpp:840`), so a widened `#ifdef` *would* have compiled. D2's rationale was
  rewritten to the real one — translate-time is unit-testable and matches `FramebufferShaderGen` —
  and the design is unchanged. `SlangTranslator.cpp:436` is confirmed to be the translator's only
  API-macro use, so Task 2 has exactly one injection site to change.
- Task 10's oracle originally called `SPIRV::CompileVertexShader` with three arguments and treated
  the result as a bool. The real signature is
  `std::optional<CodeVector> CompileVertexShader(std::string_view, APIType, EShTargetLanguageVersion, const CustomShaderContents*)`
  (`Spirv.h:23`), and `ParseSlangShader` takes `(const std::string&, std::string* error)`
  (`SlangShader.h:50`) rather than a filename. Both blocks now mirror the file's existing code.
- Task 10's OGL header cannot be copied verbatim (it is `fmt::format`-assembled at runtime), so the
  plan now spells out the hand-written desktop variant instead of pointing at line numbers.

**Known deviations from the spec, deliberate:**
- The spec's `MipChainBuilder` sketch had it called from the backend's texture; the plan calls it from
  `MultipassPostProcessing` instead, which removes the resource-lifetime problem and means D3D12
  needs no changes at all.
- The spec proposed an options struct for the translator's flip; the plan uses a single explicit
  `bool flip_clip_y` parameter — one flag does not warrant a struct.
- HDR stays (spec §2 B4, amended during recon).

**Type consistency:** `SlangNeedsClipYFlip` (Task 2) is consumed by Tasks 5 and 10;
`MipLevelCount`/`MipLevelSize` (Task 3) by Task 5; `bSupportsGPUMipGeneration` (Task 3) by Tasks 4
and 5; the `ShaderChainSpec` free functions and `GetRetroCrisisProfiles` (Task 7) by Task 8;
`LibrashaderLibraryPath` (Task 9) only by the Vulkan post-processor. Names are spelled identically at
every definition and use site above.

**Ordering constraints:** 1 and 2 are independent. 3 must precede 4 and 5. 6 must precede 8 (both
edit `EnhancementsWidget`). 7 must precede 8. 2 must precede 10 (signature) and 5 (flip helper).
9 and 10 are independent of everything except 2. 11 is last.
