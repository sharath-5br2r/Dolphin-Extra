# Multi-Pass Slang Post-Processor Replacement — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Dolphin's bespoke single-pass `.glsl` post-processor with a RetroArch-style multi-pass `.slangp` pipeline, with the milestone of rendering `crt-royale.slangp` correctly on the Vulkan backend.

**Architecture:** A new `VideoCommon/PostProcessing/` module parses `.slangp` presets and `.slang` shaders, translates each `.slang` (Vulkan-dialect GLSL, `#pragma stage`-delimited) into Dolphin's shader conventions, and executes an N-pass render-to-texture graph on top of the existing `AbstractGfx` primitives. It reuses the already-bundled glslang→SPIR-V path (`SPIRV::Compile*`) and, on Vulkan, feeds SPIR-V straight to the backend. The old `PostProcessingConfiguration`/`BlitFromTexture` single-pass code is removed and `Presenter` is repointed at the new executor. Backwards compatibility with the old `.glsl` dialect is explicitly **not** required.

**Tech Stack:** C++17/20 (VideoCommon), glslang (bundled, `Externals/glslang`), SPIRV-Cross (bundled, `Externals/spirv_cross`), `AbstractGfx`/`AbstractPipeline`/`AbstractTexture` render abstraction, libspng (bundled, via `Common::LoadPNG`), gtest (`add_dolphin_test`).

## Global Constraints

- **Scope: Vulkan backend only for this plan.** Cross-backend breadth (D3D11/D3D12/Metal via SPIRV-Cross; OpenGL via `CompilerGLSL`) and Android SAF directory-tree import are **separate follow-on plans** (see "Out of Scope" below). On non-Vulkan backends the new post-processor loads as a pass-through (no user shader) until those plans land.
- **No backwards compatibility with the old `.glsl` post-processing dialect.** The `Sample()`/`SetOutput()`/`GetOption()` macro dialect, `PostProcessingConfiguration`, and the 2-pass `BlitFromTexture` are removed, not preserved.
- License header on every new file: `// Copyright 2026 Dolphin Emulator Project` + `// SPDX-License-Identifier: GPL-2.0-or-later`.
- Preset & shader paths resolve **relative to the preset file**, including `../` parent traversal.
- Per-pass input sampler binding ceiling is **8 combined image samplers** (`AbstractPipeline.h:26-27`). Presets whose pass exceeds this must fail validation with a clear error, not crash.
- Only `RGBA16F` float render-target format is available; `float_framebuffer`/`srgb_framebuffer` passes use `RGBA16F` and sRGB is handled **in the generated shader wrapper**, not via new texture formats.
- `WrapMode` supports only `Clamp`/`Repeat`/`Mirror` (`BPMemory.h:886-890`). `clamp_to_border` maps to `Clamp` with a logged warning; crt-royale uses only `repeat`/`clamp_to_edge`.
- Existing config key `GFX_ENHANCE_POST_SHADER` (`Core/Config/GraphicsSettings.cpp:148`) is reused verbatim; its value becomes a preset name (a `.slangp` discovered under the Shaders dir) instead of a `.glsl` basename.
- New enums/structs used across tasks must match the signatures declared in each task's **Interfaces / Produces** block exactly.

## Out of Scope (tracked as follow-on plans, do not attempt here)

- Cross-backend translation beyond Vulkan (D3D11/D3D12/Metal/OpenGL).
- Runtime mipmap **generation** beyond the CPU box-filter added in Task 8 (a backend-blit mip path is future work).
- `OriginalHistoryN` / per-pass `feedback` cross-frame textures (crt-royale needs neither).
- Android SAF `ACTION_OPEN_DOCUMENT_TREE` directory-tree import + `ValidatePreset` JNI (separate plan, builds on `2026-07-12-android-custom-post-processing-shader-import.md`). Note: Task 12 adds the cross-platform zip-extraction core that the Android SAF picker in that plan calls; the Android UI wiring itself remains out of scope here.
- `#reference` nested-preset inclusion (single-file presets only for MVP).

## File Structure

**New files (all under `Source/Core/VideoCommon/PostProcessing/`):**
- `SlangPreset.h` / `SlangPreset.cpp` — parse `.slangp` into a `SlangPresetConfig` (passes + LUTs + parameters); resolve preset-relative paths.
- `SlangShader.h` / `SlangShader.cpp` — parse a `.slang` file into stage sources + pragmas (`name`/`format`/`parameter`); split VS/PS.
- `SlangTranslator.h` / `SlangTranslator.cpp` — turn parsed `.slang` stages into Dolphin-injectable GLSL (binding + semantic-uniform rewrite) and compile via `g_gfx->CreateShaderFromSource`.
- `PassSizing.h` / `PassSizing.cpp` — compute per-pass render-target sizes from `scale_type`/`scale`.
- `MultipassPostProcessing.h` / `MultipassPostProcessing.cpp` — the executor: owns the pass chain, LUTs, per-frame draw loop; the new replacement for `PostProcessing`.
- `LutTexture.h` / `LutTexture.cpp` — load LUT PNGs into `AbstractTexture` with wrap/filter/mipmap.
- `PresetArchive.h` / `PresetArchive.cpp` — extract a `.zip` shader bundle into the Shaders dir, preserving relative structure, and locate the contained `.slangp` (Task 12).

**Modified files:**
- `Source/Core/VideoCommon/Present.h` / `Present.cpp` — swap `PostProcessing` for `MultipassPostProcessing`.
- `Source/Core/VideoCommon/CMakeLists.txt` — remove `PostProcessing.{cpp,h}`, add the new module files.
- `Source/Core/DolphinLib.props` (MSVC project) — mirror the CMake source list change.
- `Source/UnitTests/VideoCommon/CMakeLists.txt` — register new unit tests.

**Deleted files (final task):**
- `Source/Core/VideoCommon/PostProcessing.cpp` / `PostProcessing.h`.

---

## Task 1: `.slangp` preset parser

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/SlangPreset.h`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangPresetTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt` (add the two module files)
- Modify: `Source/UnitTests/VideoCommon/CMakeLists.txt` (register test)

**Interfaces:**
- Produces:
  ```cpp
  namespace VideoCommon {
  enum class ScaleType { Source, Viewport, Absolute };
  enum class SlangWrapMode { ClampToEdge, ClampToBorder, Repeat, MirroredRepeat };

  struct SlangPassConfig {
    std::string shader_path;   // absolute, preset-relative resolved
    std::string alias;         // may be empty
    bool filter_linear = false;
    SlangWrapMode wrap_mode = SlangWrapMode::ClampToBorder;
    bool mipmap_input = false;
    bool srgb_framebuffer = false;
    bool float_framebuffer = false;
    ScaleType scale_type_x = ScaleType::Source;
    ScaleType scale_type_y = ScaleType::Source;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
  };
  struct SlangLutConfig {
    std::string name;          // sampler name referenced by shaders
    std::string path;          // absolute, preset-relative resolved
    SlangWrapMode wrap_mode = SlangWrapMode::ClampToBorder;
    bool linear = false;
    bool mipmap = false;
  };
  struct SlangPresetConfig {
    std::vector<SlangPassConfig> passes;
    std::vector<SlangLutConfig> luts;
  };
  // Parses preset text. base_dir is the directory containing the preset (for path resolution).
  // Returns std::nullopt with *error set on malformed input.
  std::optional<SlangPresetConfig> ParseSlangPreset(const std::string& text,
                                                    const std::string& base_dir,
                                                    std::string* error);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

Create `Source/Core/VideoCommon/PostProcessing/SlangPresetTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/SlangPreset.h"

using namespace VideoCommon;

TEST(SlangPreset, ParsesPassCountAndPaths)
{
  const std::string text =
      "shaders = \"2\"\n"
      "shader0 = \"a.slang\"\n"
      "alias0 = \"FIRST\"\n"
      "filter_linear0 = \"true\"\n"
      "scale_type0 = \"source\"\n"
      "scale0 = \"1.0\"\n"
      "shader1 = \"../b.slang\"\n"
      "scale_type_x1 = \"viewport\"\n"
      "scale_x1 = \"0.5\"\n"
      "scale_type_y1 = \"absolute\"\n"
      "scale_y1 = \"240\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "/root/preset", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  ASSERT_EQ(cfg->passes.size(), 2u);
  EXPECT_EQ(cfg->passes[0].shader_path, "/root/preset/a.slang");
  EXPECT_EQ(cfg->passes[0].alias, "FIRST");
  EXPECT_TRUE(cfg->passes[0].filter_linear);
  EXPECT_EQ(cfg->passes[0].scale_type_x, ScaleType::Source);
  EXPECT_EQ(cfg->passes[0].scale_type_y, ScaleType::Source);
  // "../b.slang" resolves against the preset dir's parent.
  EXPECT_EQ(cfg->passes[1].shader_path, "/root/b.slang");
  EXPECT_EQ(cfg->passes[1].scale_type_x, ScaleType::Viewport);
  EXPECT_FLOAT_EQ(cfg->passes[1].scale_x, 0.5f);
  EXPECT_EQ(cfg->passes[1].scale_type_y, ScaleType::Absolute);
  EXPECT_FLOAT_EQ(cfg->passes[1].scale_y, 240.0f);
}

TEST(SlangPreset, ParsesTextures)
{
  const std::string text =
      "shaders = \"1\"\n"
      "shader0 = \"a.slang\"\n"
      "textures = \"MASK;GRID\"\n"
      "MASK = \"masks/m.png\"\n"
      "MASK_wrap_mode = \"repeat\"\n"
      "MASK_linear = \"true\"\n"
      "MASK_mipmap = \"true\"\n"
      "GRID = \"g.png\"\n";
  std::string error;
  const auto cfg = ParseSlangPreset(text, "/root", &error);
  ASSERT_TRUE(cfg.has_value()) << error;
  ASSERT_EQ(cfg->luts.size(), 2u);
  EXPECT_EQ(cfg->luts[0].name, "MASK");
  EXPECT_EQ(cfg->luts[0].path, "/root/masks/m.png");
  EXPECT_EQ(cfg->luts[0].wrap_mode, SlangWrapMode::Repeat);
  EXPECT_TRUE(cfg->luts[0].linear);
  EXPECT_TRUE(cfg->luts[0].mipmap);
  EXPECT_EQ(cfg->luts[1].name, "GRID");
}

TEST(SlangPreset, RejectsMissingShaderCount)
{
  std::string error;
  const auto cfg = ParseSlangPreset("shader0 = \"a.slang\"\n", "/root", &error);
  EXPECT_FALSE(cfg.has_value());
  EXPECT_FALSE(error.empty());
}
```

Register in `Source/Core/VideoCommon/CMakeLists.txt` — after the line `PostProcessing.h` (line 146), add:

```cmake
  PostProcessing/SlangPreset.cpp
  PostProcessing/SlangPreset.h
```

Register the test in `Source/Core/VideoCommon/PostProcessing/` build; add to `Source/UnitTests/VideoCommon/CMakeLists.txt` (currently only `VertexLoaderTest`):

```cmake
add_dolphin_test(SlangPresetTest PostProcessing/SlangPresetTest.cpp)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target SlangPresetTest 2>&1 | tail -20`
Expected: FAIL — `SlangPreset.h` not found / `ParseSlangPreset` undefined.

- [ ] **Step 3: Write the header**

Create `Source/Core/VideoCommon/PostProcessing/SlangPreset.h` with the exact declarations from the **Produces** block above (include `<optional>`, `<string>`, `<vector>`).

- [ ] **Step 4: Write the implementation**

Create `Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp`. Parse line-by-line (reuse the same tokenizing approach as `PostProcessingConfiguration::LoadOptions`, `PostProcessing.cpp:144-255`: split on `=`, strip quotes/whitespace, handle CRLF). Implementation notes:
- Read `shaders = "N"`; if absent or non-numeric, set `*error = "missing 'shaders' count"` and return `std::nullopt`.
- For each index `0..N-1`, read `shaderN` (required — error if missing), `aliasN`, `filter_linearN`, `wrap_modeN`, `mipmap_inputN`, `srgb_framebufferN`, `float_framebufferN`.
- Scale: prefer axis-specific `scale_type_xN`/`scale_type_yN`/`scale_xN`/`scale_yN`; fall back to combined `scale_typeN`/`scaleN` for both axes. Map strings: `"source"→Source`, `"viewport"→Viewport`, `"absolute"→Absolute`.
- `wrap_mode` strings: `"clamp_to_edge"→ClampToEdge`, `"clamp_to_border"→ClampToBorder`, `"repeat"→Repeat`, `"mirrored_repeat"→MirroredRepeat`.
- Booleans: `"true"`/`"1"` → true.
- Path resolution helper: join `base_dir + "/" + value`, then normalize `.`/`..` segments lexically (do NOT touch the filesystem — keeps the parser unit-testable). Implement a small `NormalizePath(std::string)` that splits on `/`, pops on `..`, and rejoins.
- Textures: read `textures = "A;B;C"`, split on `;`; for each name read `<name>`, `<name>_wrap_mode`, `<name>_linear`, `<name>_mipmap`.

- [ ] **Step 5: Run test to verify it passes**

Run: `cd /Users/ilya.lissoboi/work/dolphin && cmake --build build --target SlangPresetTest 2>&1 | tail -20 && ./build/Binaries/SlangPresetTest`
Expected: PASS — all three tests green.

- [ ] **Step 6: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangPreset.h Source/Core/VideoCommon/PostProcessing/SlangPreset.cpp Source/Core/VideoCommon/PostProcessing/SlangPresetTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add .slangp preset parser"
```

---

## Task 2: `.slang` shader parser (pragma + stage split)

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/SlangShader.h`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangShader.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangShaderTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`
- Modify: `Source/UnitTests/VideoCommon/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from prior tasks (pure text).
- Produces:
  ```cpp
  namespace VideoCommon {
  struct SlangParameter {
    std::string id;
    std::string label;
    float default_value = 0.0f;
    float min_value = 0.0f;
    float max_value = 0.0f;
    float step = 0.0f;
  };
  struct SlangShaderSource {
    std::string name;                    // from #pragma name (may be empty)
    std::string format;                  // from #pragma format (may be empty)
    std::string vertex_source;           // code selected by #pragma stage vertex + common prologue
    std::string fragment_source;         // code selected by #pragma stage fragment + common prologue
    std::vector<SlangParameter> parameters;
  };
  // Parses a .slang file's raw text. Lines before the first "#pragma stage" are
  // common to both stages. Returns nullopt with *error on malformed input.
  std::optional<SlangShaderSource> ParseSlangShader(const std::string& text, std::string* error);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

Create `Source/Core/VideoCommon/PostProcessing/SlangShaderTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/SlangShader.h"

using namespace VideoCommon;

TEST(SlangShader, SplitsStagesAndSharesPrologue)
{
  const std::string text =
      "#pragma name FIRST_PASS\n"
      "#pragma format R16G16B16A16_SFLOAT\n"
      "layout(std140) uniform UBO { vec4 SourceSize; };\n"  // common prologue
      "#pragma stage vertex\n"
      "void main() { gl_Position = vec4(0); }\n"
      "#pragma stage fragment\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "void main() { FragColor = vec4(1); }\n";
  std::string error;
  const auto shader = ParseSlangShader(text, &error);
  ASSERT_TRUE(shader.has_value()) << error;
  EXPECT_EQ(shader->name, "FIRST_PASS");
  EXPECT_EQ(shader->format, "R16G16B16A16_SFLOAT");
  // Common prologue appears in both stages.
  EXPECT_NE(shader->vertex_source.find("uniform UBO"), std::string::npos);
  EXPECT_NE(shader->fragment_source.find("uniform UBO"), std::string::npos);
  // Stage-specific bodies land in the right stage only.
  EXPECT_NE(shader->vertex_source.find("gl_Position"), std::string::npos);
  EXPECT_EQ(shader->vertex_source.find("FragColor"), std::string::npos);
  EXPECT_NE(shader->fragment_source.find("FragColor"), std::string::npos);
}

TEST(SlangShader, ParsesParameters)
{
  const std::string text =
      "#pragma parameter BRIGHTNESS \"Brightness\" 1.0 0.0 2.0 0.05\n"
      "#pragma stage vertex\n"
      "void main() {}\n"
      "#pragma stage fragment\n"
      "void main() {}\n";
  std::string error;
  const auto shader = ParseSlangShader(text, &error);
  ASSERT_TRUE(shader.has_value()) << error;
  ASSERT_EQ(shader->parameters.size(), 1u);
  EXPECT_EQ(shader->parameters[0].id, "BRIGHTNESS");
  EXPECT_EQ(shader->parameters[0].label, "Brightness");
  EXPECT_FLOAT_EQ(shader->parameters[0].default_value, 1.0f);
  EXPECT_FLOAT_EQ(shader->parameters[0].max_value, 2.0f);
}

TEST(SlangShader, RejectsMissingStages)
{
  std::string error;
  const auto shader = ParseSlangShader("void main() {}\n", &error);
  EXPECT_FALSE(shader.has_value());
  EXPECT_FALSE(error.empty());
}
```

Add to the CMake source list and register `add_dolphin_test(SlangShaderTest PostProcessing/SlangShaderTest.cpp)`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target SlangShaderTest 2>&1 | tail -20`
Expected: FAIL — header/symbol missing.

- [ ] **Step 3: Write header + implementation**

Create `SlangShader.h` (declarations from **Produces**) and `SlangShader.cpp`. Logic:
- Scan lines. Lines starting with `#pragma name ` / `#pragma format ` set `name`/`format` (rest of line, trimmed).
- `#pragma parameter <id> "<label>" <def> <min> <max> <step>` → push a `SlangParameter` (parse the quoted label, then whitespace-split the four floats; tolerate a missing step → 0).
- Track current stage: lines before the first `#pragma stage` accumulate into `prologue`. `#pragma stage vertex` / `#pragma stage fragment` switch the active buffer. `#pragma name/format/parameter` lines are consumed (not emitted into any stage).
- After scanning, `vertex_source = prologue + vertex_body`, `fragment_source = prologue + fragment_body`. If either body is empty, set `*error` and return `std::nullopt`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target SlangShaderTest 2>&1 | tail -20 && ./build/Binaries/SlangShaderTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangShader.h Source/Core/VideoCommon/PostProcessing/SlangShader.cpp Source/Core/VideoCommon/PostProcessing/SlangShaderTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add .slang shader parser (pragma + stage split)"
```

---

## Task 3: Per-pass render-target sizing

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/PassSizing.h`
- Create: `Source/Core/VideoCommon/PostProcessing/PassSizing.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/PassSizingTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/UnitTests/VideoCommon/CMakeLists.txt`

**Interfaces:**
- Consumes: `ScaleType` (Task 1, `SlangPreset.h`).
- Produces:
  ```cpp
  namespace VideoCommon {
  struct PassSizeInputs {
    u32 source_width;    // previous pass output (or pipeline input for pass 0)
    u32 source_height;
    u32 viewport_width;  // final on-screen target
    u32 viewport_height;
  };
  // Computes one axis. scale_type Source: round(source * scale); Viewport: round(viewport * scale);
  // Absolute: round(scale). Result is clamped to >= 1.
  u32 ComputePassAxisSize(ScaleType type, float scale, u32 source, u32 viewport);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

Create `PassSizingTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/PassSizing.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"

using namespace VideoCommon;

TEST(PassSizing, Source)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Source, 1.0f, 640, 1920), 640u);
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Source, 0.5f, 640, 1920), 320u);
}
TEST(PassSizing, Viewport)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Viewport, 1.0f, 640, 1920), 1920u);
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Viewport, 0.0625f, 640, 1920), 120u);
}
TEST(PassSizing, Absolute)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Absolute, 320.0f, 640, 1920), 320u);
}
TEST(PassSizing, ClampsToOne)
{
  EXPECT_EQ(ComputePassAxisSize(ScaleType::Absolute, 0.0f, 640, 1920), 1u);
}
```

Register `add_dolphin_test(PassSizingTest PostProcessing/PassSizingTest.cpp)`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target PassSizingTest 2>&1 | tail -20`
Expected: FAIL — undefined symbol.

- [ ] **Step 3: Implement**

`PassSizing.h` declares `ComputePassAxisSize`. `PassSizing.cpp`:

```cpp
#include "VideoCommon/PostProcessing/PassSizing.h"
#include <algorithm>
#include <cmath>

namespace VideoCommon
{
u32 ComputePassAxisSize(ScaleType type, float scale, u32 source, u32 viewport)
{
  float base = 0.0f;
  switch (type)
  {
  case ScaleType::Source:   base = static_cast<float>(source) * scale; break;
  case ScaleType::Viewport: base = static_cast<float>(viewport) * scale; break;
  case ScaleType::Absolute: base = scale; break;
  }
  const long rounded = std::lround(base);
  return static_cast<u32>(std::max<long>(1, rounded));
}
}  // namespace VideoCommon
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target PassSizingTest 2>&1 | tail -20 && ./build/Binaries/PassSizingTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/PassSizing.* Source/Core/VideoCommon/PostProcessing/PassSizingTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add per-pass render-target sizing"
```

---

## Task 4: Slang → Dolphin GLSL translation (text rewrite)

This task is text-only (no GPU), so it is unit-testable. It rewrites a parsed `.slang` stage into GLSL that Dolphin's per-backend headers accept, mapping slang's semantic UBO + sampler bindings onto Dolphin's `UBO_BINDING`/`SAMPLER_BINDING` macro conventions (see `PostProcessing.cpp:612,688` for the target macro style).

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/SlangTranslator.h`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangTranslator.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangTranslatorTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/UnitTests/VideoCommon/CMakeLists.txt`

**Interfaces:**
- Consumes: `SlangShaderSource` (Task 2), `SlangPassConfig`/`SlangLutConfig` (Task 1).
- Produces:
  ```cpp
  namespace VideoCommon {
  // The set of texture samplers a pass reads, in binding order (index 0..N-1).
  // Includes "Source", "Original", each referenced alias, and each referenced LUT.
  struct TranslatedPass {
    std::string vertex_glsl;    // ready for g_gfx->CreateShaderFromSource(Vertex, ...)
    std::string fragment_glsl;  // ready for CreateShaderFromSource(Pixel, ...)
    std::vector<std::string> sampler_names;  // binding index -> semantic/alias/LUT name
    bool ok = false;
    std::string error;          // set when ok == false (e.g. > 8 samplers)
  };
  // known_aliases: names produced by earlier passes; lut_names: declared LUTs.
  TranslatedPass TranslateSlangPass(const SlangShaderSource& shader,
                                    const std::vector<std::string>& known_aliases,
                                    const std::vector<std::string>& lut_names);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

Create `SlangTranslatorTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/SlangShader.h"
#include "VideoCommon/PostProcessing/SlangTranslator.h"

using namespace VideoCommon;

static SlangShaderSource MakeShader(const std::string& frag_extra)
{
  SlangShaderSource s;
  s.vertex_source =
      "layout(std140) uniform UBO { mat4 MVP; vec4 SourceSize; };\n"
      "void main() { gl_Position = MVP * vec4(0); }\n";
  s.fragment_source =
      "layout(std140) uniform UBO { mat4 MVP; vec4 SourceSize; };\n"
      "layout(binding = 1) uniform sampler2D Source;\n"
      "layout(location = 0) out vec4 FragColor;\n" +
      frag_extra +
      "void main() { FragColor = texture(Source, vec2(0.5)); }\n";
  return s;
}

TEST(SlangTranslator, EmitsBindingMacrosAndSourceSampler)
{
  const auto result = TranslateSlangPass(MakeShader(""), {}, {});
  ASSERT_TRUE(result.ok) << result.error;
  // Source is always sampler binding 0.
  ASSERT_FALSE(result.sampler_names.empty());
  EXPECT_EQ(result.sampler_names[0], "Source");
  // The translator injects Dolphin's sampler macro rather than raw layout(binding=).
  EXPECT_NE(result.fragment_glsl.find("SAMPLER_BINDING(0)"), std::string::npos);
  // The UBO uses Dolphin's UBO_BINDING macro.
  EXPECT_NE(result.fragment_glsl.find("UBO_BINDING"), std::string::npos);
}

TEST(SlangTranslator, AssignsBindingsToAliasesAndLuts)
{
  auto shader = MakeShader(
      "layout(binding = 2) uniform sampler2D BLOOM_APPROX;\n"
      "layout(binding = 3) uniform sampler2D MASK;\n");
  shader.fragment_source +=
      "";  // samplers referenced above
  const auto result = TranslateSlangPass(shader, {"BLOOM_APPROX"}, {"MASK"});
  ASSERT_TRUE(result.ok) << result.error;
  // Source at 0, then referenced aliases/LUTs get subsequent binding indices.
  EXPECT_EQ(result.sampler_names[0], "Source");
  const auto& names = result.sampler_names;
  EXPECT_NE(std::find(names.begin(), names.end(), "BLOOM_APPROX"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "MASK"), names.end());
}

TEST(SlangTranslator, RejectsTooManySamplers)
{
  // Build a shader referencing 9 distinct samplers (Source + 8) -> over the 8 limit.
  SlangShaderSource shader = MakeShader("");
  std::vector<std::string> luts;
  for (int i = 0; i < 8; ++i)
  {
    const std::string name = "LUT" + std::to_string(i);
    shader.fragment_source +=
        "layout(binding = " + std::to_string(i + 2) + ") uniform sampler2D " + name + ";\n";
    luts.push_back(name);
  }
  const auto result = TranslateSlangPass(shader, {}, luts);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.empty());
}
```

Register `add_dolphin_test(SlangTranslatorTest PostProcessing/SlangTranslatorTest.cpp)` and add module files to CMake.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target SlangTranslatorTest 2>&1 | tail -20`
Expected: FAIL — undefined symbol.

- [ ] **Step 3: Implement the translator**

Create `SlangTranslator.h`/`.cpp`. The translator does a **line-level rewrite** (not a full GLSL parser — sufficient for the slang subset crt-royale uses):
- **Sampler discovery & binding assignment:** scan for `uniform sampler2D <Name>` declarations. Assign binding 0 to `Source` (always present as the first sampler even if the shader also lists it), then assign subsequent indices to each distinct referenced sampler in first-seen order. Build `sampler_names[index] = name`. If total > 8, set `ok=false`, `error="pass references N samplers; max is 8"`, return.
- **Sampler rewrite:** replace each `layout(binding = K) uniform sampler2D <Name>;` (and any bare `uniform sampler2D <Name>;`) with `SAMPLER_BINDING(<assigned_index>) uniform sampler2D <Name>;`.
- **UBO rewrite:** replace the `layout(std140) uniform UBO {` (or `layout(std140, ...) uniform ...`) opener with `UBO_BINDING(std140, 1) uniform PSBlock {`. Keep the member list as-is (semantic members `MVP`, `SourceSize`, `OutputSize`, `FrameCount`, `<Alias>Size`, params) — the executor (Task 6/7) fills them.
- **Output variable:** slang uses `layout(location = 0) out vec4 FragColor;` — rewrite to `FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;` and replace `FragColor` occurrences with `ocol0`. (Dolphin's HLSL→GLSL alias macros already map `vec4`↔`float4` etc., so leave slang's `vec*`/`mat*` intact.)
- **Varyings:** slang VS writes `layout(location = 0) out vec2 vTexCoord;`, FS reads the matching `in`. Rewrite `layout(location = N) out/in` to `VARYING_LOCATION(N) out/in`.
- Return `vertex_glsl`/`fragment_glsl` as the rewritten strings. Do **not** prepend the per-backend header here — `g_gfx->CreateShaderFromSource` prepends it (that is where `SAMPLER_BINDING`/`UBO_BINDING`/`VARYING_LOCATION` get defined per backend, e.g. `VideoBackends/Vulkan/ShaderCompiler.cpp:22-59`).

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target SlangTranslatorTest 2>&1 | tail -20 && ./build/Binaries/SlangTranslatorTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangTranslator.* Source/Core/VideoCommon/PostProcessing/SlangTranslatorTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add slang->Dolphin GLSL translation"
```

---

## Task 5: Compile a translated pass on Vulkan (integration point)

This task adds the thin bridge from translated GLSL to backend shader objects. It has no GPU-free unit test (needs `g_gfx`), so it is verified by compiling the app and by the executor's runtime path in Task 9. Keep the function small and side-effect-free so Task 9's manual verification exercises it.

> **Verified against checked-out submodules** (glslang `a57276bf`, SPIRV-Cross `ebe2aa0c`): the `ShaderIncluder` this task constructs derives from `glslang::TShader::Includer` (`glslang/Public/ShaderLang.h:599`), and `SPIRV::Compile{Vertex,Fragment}Shader` (the path `CreateShaderFromSource` uses on Vulkan) is referenced today in `VideoBackends/{Vulkan,D3DCommon,Metal}` but **not** OpenGL — matching this plan's Vulkan-only scope. See analysis §2.2 for the full verification stamp.

**Files:**
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangTranslator.h`
- Modify: `Source/Core/VideoCommon/PostProcessing/SlangTranslator.cpp`

**Interfaces:**
- Consumes: `TranslatedPass` (Task 4); `g_gfx->CreateShaderFromSource(ShaderStage, std::string_view, ShaderIncluder*, name)` (`AbstractGfx.h:114`); `ShaderStage::{Vertex,Pixel}` (`AbstractShader.h:20-25`); `VideoCommon::ShaderIncluder` (`ShaderCompileUtils.h`).
- Produces:
  ```cpp
  namespace VideoCommon {
  struct CompiledPassShaders {
    std::unique_ptr<AbstractShader> vertex;
    std::unique_ptr<AbstractShader> pixel;
  };
  // Compiles a translated pass. include_user_dir/include_system_dir root the #include resolver
  // (the directory of the .slang file, and the Sys shaders dir). Returns empty uniques on failure.
  CompiledPassShaders CompileTranslatedPass(const TranslatedPass& pass,
                                            const std::string& include_dir);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Add the declaration**

In `SlangTranslator.h` add the `CompiledPassShaders` struct and `CompileTranslatedPass` declaration (include `<memory>`, forward-declare `class AbstractShader;`).

- [ ] **Step 2: Implement**

In `SlangTranslator.cpp`:

```cpp
CompiledPassShaders CompileTranslatedPass(const TranslatedPass& pass,
                                          const std::string& include_dir)
{
  CompiledPassShaders out;
  if (!pass.ok)
    return out;

  // #include resolver rooted at the shader's own directory and the Sys shaders dir.
  ShaderIncluder includer(include_dir + DIR_SEP,
                          File::GetSysDirectory() + SHADERS_DIR DIR_SEP);

  out.vertex = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, pass.vertex_glsl, &includer,
                                             "slang post-process vertex");
  if (!out.vertex)
    return {};

  out.pixel = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pass.fragment_glsl, &includer,
                                            "slang post-process fragment");
  if (!out.pixel)
    return {};

  return out;
}
```

Add includes: `"VideoCommon/AbstractGfx.h"`, `"VideoCommon/AbstractShader.h"`, `"VideoCommon/ShaderCompileUtils.h"`, `"Common/CommonPaths.h"`, `"Common/FileUtil.h"`.

- [ ] **Step 3: Build to verify it compiles/links**

Run: `cmake --build build --target dolphin-emu 2>&1 | tail -20`
Expected: BUILD SUCCESS (the new symbol links; unused for now until Task 9 calls it).

- [ ] **Step 4: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangTranslator.h Source/Core/VideoCommon/PostProcessing/SlangTranslator.cpp
git commit -m "VideoCommon: compile translated slang pass via CreateShaderFromSource"
```

---

## Task 6: Slang wrap/filter → Dolphin `SamplerState`

Pure mapping, unit-testable.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/SlangSamplers.h`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangSamplers.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/SlangSamplersTest.cpp`
- Modify: CMake + UnitTests CMake

**Interfaces:**
- Consumes: `SlangWrapMode` (Task 1); `SamplerState`/`WrapMode`/`FilterMode` (`RenderState.h:163`, `BPMemory.h:886,912`).
- Produces:
  ```cpp
  namespace VideoCommon {
  // has_mips controls mipmap filtering + lod range; filter_linear controls min/mag filter.
  SamplerState MakeSlangSamplerState(SlangWrapMode wrap, bool filter_linear, bool has_mips);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

Create `SlangSamplersTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/PostProcessing/SlangPreset.h"
#include "VideoCommon/PostProcessing/SlangSamplers.h"
#include "VideoCommon/RenderState.h"

using namespace VideoCommon;

TEST(SlangSamplers, LinearRepeat)
{
  const SamplerState s = MakeSlangSamplerState(SlangWrapMode::Repeat, true, false);
  EXPECT_EQ(s.tm0.min_filter, FilterMode::Linear);
  EXPECT_EQ(s.tm0.mag_filter, FilterMode::Linear);
  EXPECT_EQ(s.tm0.wrap_u, WrapMode::Repeat);
  EXPECT_EQ(s.tm0.wrap_v, WrapMode::Repeat);
}

TEST(SlangSamplers, PointClampNoMips)
{
  const SamplerState s = MakeSlangSamplerState(SlangWrapMode::ClampToEdge, false, false);
  EXPECT_EQ(s.tm0.min_filter, FilterMode::Near);
  EXPECT_EQ(s.tm0.wrap_u, WrapMode::Clamp);
  // No mips -> lod range collapsed to 0.
  EXPECT_EQ(s.tm1.min_lod, 0u);
  EXPECT_EQ(s.tm1.max_lod, 0u);
}

TEST(SlangSamplers, MipmappedHasLodRange)
{
  const SamplerState s = MakeSlangSamplerState(SlangWrapMode::Repeat, true, true);
  EXPECT_GT(s.tm1.max_lod, 0u);
}

TEST(SlangSamplers, BorderMapsToClamp)
{
  const SamplerState s = MakeSlangSamplerState(SlangWrapMode::ClampToBorder, true, false);
  EXPECT_EQ(s.tm0.wrap_u, WrapMode::Clamp);  // border unsupported -> clamp
}

TEST(SlangSamplers, MirroredRepeatMapsToMirror)
{
  const SamplerState s = MakeSlangSamplerState(SlangWrapMode::MirroredRepeat, true, false);
  EXPECT_EQ(s.tm0.wrap_u, WrapMode::Mirror);
}
```

Register `add_dolphin_test(SlangSamplersTest PostProcessing/SlangSamplersTest.cpp)`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target SlangSamplersTest 2>&1 | tail -20`
Expected: FAIL.

- [ ] **Step 3: Implement**

`SlangSamplers.cpp`:

```cpp
#include "VideoCommon/PostProcessing/SlangSamplers.h"
#include "VideoCommon/BPMemory.h"

namespace VideoCommon
{
static WrapMode ToWrapMode(SlangWrapMode w)
{
  switch (w)
  {
  case SlangWrapMode::Repeat:         return WrapMode::Repeat;
  case SlangWrapMode::MirroredRepeat: return WrapMode::Mirror;
  case SlangWrapMode::ClampToEdge:
  case SlangWrapMode::ClampToBorder:  // border unsupported; approximate with clamp
  default:                            return WrapMode::Clamp;
  }
}

SamplerState MakeSlangSamplerState(SlangWrapMode wrap, bool filter_linear, bool has_mips)
{
  SamplerState s;
  s.tm0.hex = 0;
  s.tm1.hex = 0;
  const FilterMode f = filter_linear ? FilterMode::Linear : FilterMode::Near;
  s.tm0.min_filter = f;
  s.tm0.mag_filter = f;
  s.tm0.mipmap_filter = has_mips ? FilterMode::Linear : FilterMode::Near;
  s.tm0.wrap_u = ToWrapMode(wrap);
  s.tm0.wrap_v = ToWrapMode(wrap);
  s.tm1.min_lod = 0;
  // max_lod is multiplied by 16 (see RenderState.h:209-210). 13 mip levels * 16 covers 8K textures.
  s.tm1.max_lod = has_mips ? (13u * 16u) : 0u;
  return s;
}
}  // namespace VideoCommon
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target SlangSamplersTest 2>&1 | tail -20 && ./build/Binaries/SlangSamplersTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/SlangSamplers.* Source/Core/VideoCommon/PostProcessing/SlangSamplersTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: map slang wrap/filter to Dolphin SamplerState"
```

---

## Task 7: CPU box-filter mipmap generation

LUTs flagged `mipmap = true` (crt-royale's `_large` masks) need a mip chain; Dolphin has no runtime mip generator. This task adds a CPU box filter over RGBA8. Pure function, unit-testable.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/MipGen.h`
- Create: `Source/Core/VideoCommon/PostProcessing/MipGen.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/MipGenTest.cpp`
- Modify: CMake + UnitTests CMake

**Interfaces:**
- Produces:
  ```cpp
  namespace VideoCommon {
  struct MipLevel {
    u32 width;
    u32 height;
    std::vector<u8> rgba8;  // width*height*4
  };
  // level0 is the full-res RGBA8 image (width*height*4). Returns level0 plus successive
  // half-size box-filtered levels down to 1x1.
  std::vector<MipLevel> GenerateBoxMips(u32 width, u32 height, const u8* rgba8);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

Create `MipGenTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing/MipGen.h"

using namespace VideoCommon;

TEST(MipGen, HalvesDownToOne)
{
  // 2x2 solid white.
  std::vector<u8> img(2 * 2 * 4, 255);
  const auto mips = GenerateBoxMips(2, 2, img.data());
  ASSERT_EQ(mips.size(), 2u);           // 2x2 -> 1x1
  EXPECT_EQ(mips[0].width, 2u);
  EXPECT_EQ(mips[1].width, 1u);
  EXPECT_EQ(mips[1].height, 1u);
  // Average of four white texels is white.
  EXPECT_EQ(mips[1].rgba8[0], 255);
}

TEST(MipGen, AveragesColors)
{
  // 2x1 image: black and white side by side.
  std::vector<u8> img = {0, 0, 0, 255,  255, 255, 255, 255};
  const auto mips = GenerateBoxMips(2, 1, img.data());
  ASSERT_EQ(mips.size(), 2u);           // 2x1 -> 1x1
  // Averaged red channel ~127-128.
  EXPECT_NEAR(mips[1].rgba8[0], 127, 1);
}

TEST(MipGen, NonSquareChain)
{
  std::vector<u8> img(4 * 2 * 4, 128);
  const auto mips = GenerateBoxMips(4, 2, img.data());
  // 4x2 -> 2x1 -> 1x1
  ASSERT_EQ(mips.size(), 3u);
  EXPECT_EQ(mips[1].width, 2u);
  EXPECT_EQ(mips[1].height, 1u);
  EXPECT_EQ(mips[2].width, 1u);
}
```

Register `add_dolphin_test(MipGenTest PostProcessing/MipGenTest.cpp)`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target MipGenTest 2>&1 | tail -20`
Expected: FAIL.

- [ ] **Step 3: Implement**

`MipGen.cpp`: level 0 copies the input. Each next level has `w=max(1,prev.w/2)`, `h=max(1,prev.h/2)`; each output texel averages the 2x2 block of the previous level (clamp source coords when a dimension is 1). Stop after producing 1x1. For each channel, sum the four source bytes and divide by 4 (integer, `+2` for rounding).

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target MipGenTest 2>&1 | tail -20 && ./build/Binaries/MipGenTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/MipGen.* Source/Core/VideoCommon/PostProcessing/MipGenTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add CPU box-filter mipmap generation for LUTs"
```

---

## Task 8: LUT PNG loader (GPU upload)

Bridges `Common::LoadPNG` + `GenerateBoxMips` to an `AbstractTexture`. Needs `g_gfx`, so verified by app build + Task 9 runtime; keep it minimal.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/LutTexture.h`
- Create: `Source/Core/VideoCommon/PostProcessing/LutTexture.cpp`
- Modify: CMake

**Interfaces:**
- Consumes: `SlangLutConfig` (Task 1), `GenerateBoxMips` (Task 7); `Common::LoadPNG` (`Common/Image.h:14`); `g_gfx->CreateTexture` (`AbstractGfx.h:75`); `AbstractTexture::Load` (`AbstractTexture.h:32`); `Common::ReadFileToString`/`File::ReadFileToString` (`FileUtil.h`).
- Produces:
  ```cpp
  namespace VideoCommon {
  // Loads a LUT PNG into a sampled 2D texture (RGBA8), generating mips if lut.mipmap.
  // Returns nullptr on decode/read failure.
  std::unique_ptr<AbstractTexture> LoadLutTexture(const SlangLutConfig& lut);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Implement**

`LutTexture.cpp`:
- Read the file bytes: `File::ReadFileToString(lut.path, contents)` (content-URI-aware; `FileUtil.cpp:1034`). On failure log `ERROR_LOG_FMT(VIDEO, ...)` and return `nullptr`.
- Decode: `Common::LoadPNG({contents}, &buffer, &w, &h)` → RGBA8. On failure, log + return `nullptr`.
- Mips: if `lut.mipmap`, `mips = GenerateBoxMips(w, h, buffer.data())`; else a single level wrapping the decoded pixels.
- Create texture: `TextureConfig cfg(w, h, static_cast<u32>(mips.size()), 1, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2D);` then `auto tex = g_gfx->CreateTexture(cfg, "slang LUT: " + lut.name);`.
- Upload each level: `tex->Load(level, mips[level].width, mips[level].height, mips[level].width, mips[level].rgba8.data(), mips[level].rgba8.size());` (row_length == width for tightly packed RGBA8, matching `CustomTextureData.cpp:583`).
- Return `tex`.

Add includes: `"Common/Image.h"`, `"Common/FileUtil.h"`, `"VideoCommon/AbstractGfx.h"`, `"VideoCommon/AbstractTexture.h"`, `"VideoCommon/TextureConfig.h"`, `"VideoCommon/PostProcessing/MipGen.h"`, `"Common/Logging/Log.h"`.

- [ ] **Step 2: Build to verify it compiles**

Run: `cmake --build build --target dolphin-emu 2>&1 | tail -20`
Expected: BUILD SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/LutTexture.* Source/Core/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add LUT PNG loader with mipmap upload"
```

---

## Task 9: Multi-pass executor

The heart of the replacement: owns the pass chain + LUTs, builds per-pass RT textures/framebuffers/pipelines, and runs the per-frame draw loop. Needs `g_gfx`, so it is verified via the end-to-end run in Task 11; this task builds the structure and wires prior tasks together.

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.h`
- Create: `Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.cpp`
- Modify: CMake

**Interfaces:**
- Consumes: everything from Tasks 1–8; `AbstractPipelineConfig`/`RenderState` (`AbstractPipeline.h`, `RenderState.h`); `g_gfx` framebuffer/draw API (`AbstractGfx.h`); `g_vertex_manager->UploadUtilityUniforms` (see `PostProcessing.cpp:547`); `g_presenter->GetTargetRectangle()`.
- Produces:
  ```cpp
  namespace VideoCommon {
  class MultipassPostProcessing {
  public:
    MultipassPostProcessing();
    ~MultipassPostProcessing();
    // Loads the preset named by GFX_ENHANCE_POST_SHADER (a .slangp under the Shaders dir).
    // Empty/failed -> pass-through mode.
    bool Initialize(AbstractTextureFormat format);
    void RecompileShader();   // reload preset (config change)
    void RecompilePipeline(); // rebuild pipelines only (format change)
    // Runs the pass chain, presenting into the currently-bound framebuffer over dst.
    void BlitFromTexture(const MathUtil::Rectangle<int>& dst,
                         const MathUtil::Rectangle<int>& src,
                         const AbstractTexture* src_tex, int src_layer = -1);
  private:
    struct Pass {
      SlangPassConfig config;
      std::vector<std::string> sampler_names;   // binding index -> input name
      SamplerState input_sampler;               // sampler applied to this pass's inputs
      std::unique_ptr<AbstractShader> vertex_shader;
      std::unique_ptr<AbstractShader> pixel_shader;
      std::unique_ptr<AbstractPipeline> pipeline;
      std::unique_ptr<AbstractTexture> output_texture;      // null for final pass
      std::unique_ptr<AbstractFramebuffer> output_framebuffer;
      std::string alias;
    };
    struct Lut {
      std::string name;
      std::unique_ptr<AbstractTexture> texture;
      SamplerState sampler;
    };
    bool LoadPreset(const std::string& preset_name);
    void ClearChain();
    std::vector<Pass> m_passes;
    std::vector<Lut> m_luts;
    AbstractTextureFormat m_framebuffer_format = AbstractTextureFormat::Undefined;
    bool m_passthrough = true;
    u32 m_frame_count = 0;
  };
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write header + skeleton**

Create `MultipassPostProcessing.h` (declarations above) and a `.cpp` with all methods stubbed: constructor/destructor default; `Initialize` calls `LoadPreset(Config::Get(Config::GFX_ENHANCE_POST_SHADER))` and returns true; `BlitFromTexture` in pass-through mode does a single copy of `src_tex` to `dst` (reuse the trivial final-pass draw from `PostProcessing::BlitFromTexture`, `PostProcessing.cpp:592-605`). Build the app.

Run: `cmake --build build --target dolphin-emu 2>&1 | tail -20`
Expected: BUILD SUCCESS.

- [ ] **Step 2: Implement `LoadPreset` (build the chain)**

- Resolve preset path: `File::GetUserPath(D_SHADERS_IDX) + preset_name + ".slangp"`, fallback to `File::GetSysDirectory() + SHADERS_DIR DIR_SEP + preset_name + ".slangp"` (mirror `LoadShaderFromFile`, `PostProcessing.cpp:45-50`). If `preset_name` empty or file missing → `m_passthrough = true`, return true.
- Read file, `ParseSlangPreset(text, base_dir, &error)`; on failure `PanicAlertFmt("Failed to load shader preset {}: {}", preset_name, error)`, pass-through, return false.
- Load LUTs: for each `SlangLutConfig`, `LoadLutTexture(lut)` + `MakeSlangSamplerState(lut.wrap_mode, lut.linear, lut.mipmap)`.
- Build known-aliases list incrementally: for each pass index i, `ParseSlangShader(File::ReadFileToString(pass.shader_path))`, then `TranslateSlangPass(shader, known_aliases_so_far, lut_names)`. If `!ok` → panic with `result.error`, pass-through, return false. `CompileTranslatedPass(result, directory_of(pass.shader_path))`; null → panic, pass-through, return false. After building pass i, append its `alias` (if non-empty) to `known_aliases`.
- Store `sampler_names`, `MakeSlangSamplerState(pass.filter_linear ...)` as `input_sampler`, and the shaders. Pipelines are built in `RecompilePipeline` (needs the format).
- Set `m_passthrough = false`.

- [ ] **Step 3: Implement `RecompilePipeline` (per-pass pipelines + RTs)**

For each pass i (0..n-1):
- Determine output format: final pass (i == n-1) uses `m_framebuffer_format`; others use `AbstractTextureFormat::RGBA16F` (covers float/srgb per Global Constraints).
- Non-final passes: compute size via `ComputePassAxisSize` using source = previous pass output size (pass 0 source = input `src` rect size, resolved at blit time — for allocation use the current target rectangle as an estimate and reallocate in `BlitFromTexture` if it changes, mirroring the resize check at `PostProcessing.cpp:526-540`). Create `output_texture` (`AbstractTextureFlag_RenderTarget`, `Texture_2DArray`) + `output_framebuffer`.
- Build `AbstractPipelineConfig`: `vertex_shader`/`pixel_shader` from the pass, `usage = AbstractPipelineUsage::Utility`, no-cull raster, no-depth, no-blend (copy the state setup from `PostProcessing.cpp:1046-1051`), `framebuffer_state = RenderState::GetColorFramebufferState(output_format)`. `m_passes[i].pipeline = g_gfx->CreatePipeline(config)`.

- [ ] **Step 4: Implement `BlitFromTexture` (the draw loop)**

- If `m_passthrough`: single copy of `src_tex` → dst (as in Step 1), return.
- Detect `m_framebuffer_format` change vs `g_gfx->GetCurrentFramebuffer()->GetColorFormat()`; if changed, update + `RecompilePipeline()` (mirror `PostProcessing.cpp:477-481`).
- `++m_frame_count`.
- Track the current input texture/rect (`prev_tex = src_tex`, `prev_rect = src`). Keep a name→texture map: `"Original"`/`"Source"` → prev, each alias → that pass's `output_texture`, each LUT name → its texture.
- For each pass i:
  - Bind inputs: for `binding = 0..sampler_names.size()-1`, resolve `sampler_names[binding]` to a texture via the map (`"Source"`→prev output; `"Original"`→pipeline input; alias→named pass output; LUT→lut texture). `g_gfx->SetTexture(binding, tex)`; `g_gfx->SetSamplerState(binding, lut ? lut.sampler : pass.input_sampler)`. Call `tex->FinishedRendering()` on any texture that was a render target in an earlier pass this frame (critical for Vulkan layout).
  - Select target framebuffer: non-final → `pass.output_framebuffer`; final → the framebuffer bound on entry (saved via `g_gfx->GetCurrentFramebuffer()`).
  - Fill + upload the pass UBO: pack the semantic members the translator kept (`MVP` = identity/flip as in `GetVertexShaderBody`, `PostProcessing.cpp:790-804`; `SourceSize`/`OriginalSize`/`OutputSize` as `vec4(w, h, 1/w, 1/h)`; `FrameCount` = `m_frame_count`; each `<Alias>Size`; parameter values). `g_vertex_manager->UploadUtilityUniforms(data, size)`.
  - `g_gfx->SetFramebuffer(target)`; `g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(target_rect, target))`; `g_gfx->SetPipeline(pass.pipeline.get())`; `g_gfx->Draw(0, 3)`.
  - Advance: `prev_tex = pass.output_texture` (non-final); update the name map's `"Source"`.
- Restore the entry framebuffer for the final pass; the final pass draws into `dst`.

> NOTE for implementer: the UBO packing must match std140 layout the translated shader declares. Because the translator preserves the shader's own UBO member list, build the UBO bytes by reflecting that member order. For the MVP, assume crt-royale's member set and pack in declared order; a later task can generalize via SPIR-V reflection. Keep the packing in one helper so it is easy to revisit.

- [ ] **Step 5: Build**

Run: `cmake --build build --target dolphin-emu 2>&1 | tail -20`
Expected: BUILD SUCCESS.

- [ ] **Step 6: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/MultipassPostProcessing.* Source/Core/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add multi-pass slang post-processing executor"
```

---

## Task 10: Swap Presenter to the new executor and remove the old post-processor

**Files:**
- Modify: `Source/Core/VideoCommon/Present.h:91,165` and includes
- Modify: `Source/Core/VideoCommon/Present.cpp:123-124,375-390,883-902`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt:145-146` (drop `PostProcessing.{cpp,h}`)
- Modify: `Source/Core/DolphinLib.props` (drop old, add new files)
- Delete: `Source/Core/VideoCommon/PostProcessing.cpp`, `Source/Core/VideoCommon/PostProcessing.h`
- Modify: any remaining referencers of `VideoCommon::PostProcessing` (see Step 1 grep)

**Interfaces:**
- Consumes: `MultipassPostProcessing` (Task 9). Note the JNI enumerator `PostProcessing::GetShaderList` (`Source/Android/jni/Config/PostProcessing.cpp`) and Qt `EnhancementsWidget` still call the static shader-list API — see Step 4.

- [ ] **Step 1: Find all referencers**

Run:
```bash
grep -rn "VideoCommon::PostProcessing\|PostProcessing\.h\|GetPostProcessor\|PostProcessingConfiguration" Source/Core Source/Android/jni | grep -v "PostProcessing/" | sort
```
Record every hit; each must be repointed or removed. Expected referencers: `Present.{h,cpp}`, `Config/GraphicsSettings`, Qt `EnhancementsWidget.cpp` / `PostProcessingConfigWindow.cpp`, Android `jni/Config/PostProcessing.cpp`.

- [ ] **Step 2: Repoint Presenter**

In `Present.h`: replace `#include`/forward-ref and the member type `std::unique_ptr<VideoCommon::PostProcessing> m_post_processor;` (line 165) and `GetPostProcessor()` return type (line 91) with `MultipassPostProcessing`. In `Present.cpp`: change the include to `"VideoCommon/PostProcessing/MultipassPostProcessing.h"`, and `m_post_processor = std::make_unique<VideoCommon::MultipassPostProcessing>();` (line 123). The `RecompileShader`/`RecompilePipeline`/`BlitFromTexture` call sites (375-390, 883-902) keep the same signatures — no change needed.

- [ ] **Step 3: Provide a preset enumerator to replace `GetShaderList`**

The Android/Qt UIs call `VideoCommon::PostProcessing::GetShaderList()`. Add static enumerators to `MultipassPostProcessing` that scan for `.slangp`:

```cpp
static std::vector<std::string> GetPresetList();  // *.slangp under user+sys Shaders dirs
```

Implement by copying `GetShaders`/`DoFileSearch` logic (`PostProcessing.cpp:386-402`) with extension `.slangp`. Repoint `Source/Android/jni/Config/PostProcessing.cpp` and Qt `EnhancementsWidget.cpp` to call `MultipassPostProcessing::GetPresetList()`. Anaglyph/passive stereo shader lists are dropped (out of scope, no back-compat) — replace their combo population with the single preset list, and remove the now-dead `GetAnaglyphShaderList`/`GetPassiveShaderList` JNI entries + their Kotlin `external` getters (`Source/Android/app/.../model/PostProcessing.kt`).

- [ ] **Step 4: Remove the per-shader options window wiring**

`DolphinQt/Config/Graphics/PostProcessingConfigWindow.cpp` depends on `PostProcessingConfiguration`. For this MVP, remove the config-window launch (the gear/options button in `EnhancementsWidget`) and delete `PostProcessingConfigWindow.{cpp,h}` from the Qt CMake/props. (Parameter UI returns in a later task via the `#pragma parameter` → options mapping.)

- [ ] **Step 5: Delete old files and update build**

```bash
git rm Source/Core/VideoCommon/PostProcessing.cpp Source/Core/VideoCommon/PostProcessing.h
```
Remove lines 145-146 from `Source/Core/VideoCommon/CMakeLists.txt`; mirror in `Source/Core/DolphinLib.props`. Remove `PostProcessingValidationTest` references if the earlier import plan's test was landed (coordinate; otherwise skip).

- [ ] **Step 6: Build the full app + Qt**

Run: `cmake --build build 2>&1 | tail -30`
Expected: BUILD SUCCESS with no references to the removed symbols.

- [ ] **Step 7: Run the full unit-test suite**

Run: `cd /Users/ilya.lissoboi/work/dolphin && ctest --test-dir build 2>&1 | tail -20`
Expected: all tests PASS (the new Slang* tests + preexisting suite).

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "VideoCommon: replace single-pass post-processor with multi-pass slang pipeline"
```

---

## Task 11: End-to-end verification with crt-royale on Vulkan

**Files:** none (manual verification + asset staging).

- [ ] **Step 1: Stage the crt-royale preset**

Copy a crt-royale distribution (preset + `src/`, `../blurs/`, `../../include/`, and the mask PNGs) into the user Shaders dir, preserving relative structure so `../` and `#include` resolve:
```bash
# Example layout under <User>/Shaders/ :
#   crt-royale.slangp
#   shaders/crt-royale/... (.slang + .png)
#   blurs/shaders/royale/... (.slang)
```
Confirm the preset file is discoverable: it must appear in `MultipassPostProcessing::GetPresetList()`.

- [ ] **Step 2: Launch on Vulkan and select the preset**

Run Dolphin with the Vulkan backend, boot any game, then Graphics → Enhancements → Post-Processing Effect → `crt-royale`.
Expected: the CRT effect renders (scanlines, phosphor mask, bloom, curvature) with no `PanicAlert`. If a pass fails to compile, the panic message names the failing `.slang` — use it to debug the translator (Task 4) or UBO packing (Task 9 Step 4).

- [ ] **Step 3: Resize / resolution change**

Change the internal resolution and window size.
Expected: viewport-scaled passes (mask-resize, final) reallocate correctly; no crash, effect stays correct. This exercises the RT resize path (Task 9 Step 4).

- [ ] **Step 4: Pass-through and error paths**

- Set Post-Processing Effect to "(off)" → clean unmodified image.
- Temporarily corrupt one `.slang` (add a syntax error) and re-select the preset → a single clear panic naming that shader; the emulator falls back to pass-through, not a crash.

- [ ] **Step 5: Simpler preset sanity**

Test a trivial 1-pass `.slangp` (e.g. a passthrough or a single CRT-geom shader) to confirm the common path works independent of crt-royale's complexity.

- [ ] **Step 6: Regression — other backends load pass-through**

Launch on D3D11/D3D12/OpenGL/Metal (whichever the dev machine supports). Selecting a preset must not crash; per Global Constraints these backends run pass-through until the cross-backend follow-on plan lands. Confirm normal (unshaded) rendering is intact.

- [ ] **Step 7: Final full test run**

Run: `ctest --test-dir build 2>&1 | tail -20`
Expected: all PASS.

- [ ] **Step 8: Commit any fixups**

```bash
git add -A && git commit -m "VideoCommon: fixups from crt-royale end-to-end verification"
```

---

## Task 12: Zip-based preset bundle import

Slang shaders are distributed as a directory tree (preset + `.slang` sources + shared `#include` headers + texture PNGs), commonly transported as a `.zip`. This task adds a cross-platform core that extracts such a bundle into the Shaders dir **preserving relative structure** (so `../` references and `#include` chains resolve) and reports the contained `.slangp` so it can be selected. This is the platform-agnostic half of preset import; the Android SAF single-file picker that calls it lives in the Android follow-on plan (`2026-07-12-android-custom-post-processing-shader-import.md`).

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/PresetArchive.h`
- Create: `Source/Core/VideoCommon/PostProcessing/PresetArchive.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/PresetArchiveTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/Core/DolphinLib.props`, `Source/UnitTests/VideoCommon/CMakeLists.txt`

**Interfaces:**
- Consumes: minizip-ng reader API (`mz_zip_reader_create`/`_open_file`/`_goto_first_entry`/`_entry_get_info`/`_goto_next_entry`/`_delete`, as used in `Source/Core/UICommon/ResourcePack/ResourcePack.cpp:33-116`); `Common::ReadFileFromZip` (`Common/MinizipUtil.h:18`); `File::CreateFullPath`/`File::WriteStringToFile`/`File::GetUserPath(D_SHADERS_IDX)` (`Common/FileUtil.h`).
- Produces:
  ```cpp
  namespace VideoCommon {
  struct PresetImportResult {
    bool ok = false;
    std::string preset_name;   // basename without ".slangp", ready for GFX_ENHANCE_POST_SHADER
    std::string error;         // set when ok == false
  };
  // Extracts a .slangp bundle zip at zip_path into dest_root (typically the user Shaders dir),
  // preserving the archive's internal directory structure. Requires exactly one .slangp entry
  // in the archive; returns its extraction-relative name in preset_name.
  PresetImportResult ImportPresetArchive(const std::string& zip_path, const std::string& dest_root);

  // Shared, sanitized zip extractor used by ImportPresetArchive and by Task 13's buildbot
  // downloader. Extracts every non-directory entry of the archive at zip_path into dest_root,
  // preserving structure, after rejecting any absolute or "../"-containing entry (Zip-Slip).
  // Returns the extraction-relative names of all ".slangp" entries found (empty on failure;
  // *error set). Does NOT enforce a single-preset rule — callers decide (import wants exactly
  // one; the buildbot pack has hundreds).
  std::vector<std::string> ExtractSanitizedArchive(const std::string& zip_path,
                                                   const std::string& dest_root,
                                                   std::string* error);
  }  // namespace VideoCommon
  ```

  > Implementer note: implement `ExtractSanitizedArchive` first (the per-entry sanitize-scan +
  > extract loop described in Step 4), then make `ImportPresetArchive` a thin wrapper that calls
  > it and applies the single-`.slangp` rule. Task 13 reuses `ExtractSanitizedArchive` verbatim,
  > so keep it free of any single-preset assumption.

- [ ] **Step 1: Write the failing test**

The test builds a small zip on disk (via the minizip **writer**, mirroring how `ResourcePack` tests would), then imports it and asserts structure is preserved. Create `PresetArchiveTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <gtest/gtest.h>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/FileUtil.h"
#include "VideoCommon/PostProcessing/PresetArchive.h"

using namespace VideoCommon;

namespace
{
// Writes a zip containing the given {internal_path, contents} entries.
void WriteZip(const std::string& zip_path,
              const std::vector<std::pair<std::string, std::string>>& entries)
{
  void* writer = mz_zip_writer_create();
  ASSERT_EQ(mz_zip_writer_open_file(writer, zip_path.c_str(), 0, 0), MZ_OK);
  for (const auto& [name, data] : entries)
  {
    mz_zip_file file_info = {};
    file_info.filename = name.c_str();
    file_info.flag = MZ_ZIP_FLAG_UTF8;
    ASSERT_EQ(mz_zip_writer_add_buffer(writer, const_cast<char*>(data.data()),
                                       static_cast<int32_t>(data.size()), &file_info),
              MZ_OK);
  }
  mz_zip_writer_close(writer);
  mz_zip_writer_delete(&writer);
}
}  // namespace

TEST(PresetArchive, ExtractsPreservingStructureAndFindsPreset)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bundle.zip";
  const std::string dest = tmp + "/out";

  WriteZip(zip, {
      {"crt-royale.slangp", "shaders = \"1\"\nshader0 = \"src/a.slang\"\n"},
      {"src/a.slang", "#pragma stage vertex\nvoid main(){}\n#pragma stage fragment\nvoid main(){}\n"},
      {"src/masks/m.png", "\x89PNG\r\n"},  // content irrelevant to extraction
  });

  const auto result = ImportPresetArchive(zip, dest);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.preset_name, "crt-royale");
  EXPECT_TRUE(File::Exists(dest + "/crt-royale.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/src/a.slang"));
  EXPECT_TRUE(File::Exists(dest + "/src/masks/m.png"));

  File::DeleteDirRecursively(tmp);
}

TEST(PresetArchive, RejectsArchiveWithNoPreset)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bundle.zip";
  WriteZip(zip, {{"readme.txt", "no preset here"}});
  const auto result = ImportPresetArchive(zip, tmp + "/out");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.empty());
  File::DeleteDirRecursively(tmp);
}

TEST(PresetArchive, RejectsPathTraversalEntries)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bundle.zip";
  // A malicious entry escaping dest_root must be rejected, not written outside.
  WriteZip(zip, {
      {"crt.slangp", "shaders = \"0\"\n"},
      {"../evil.slang", "pwned"},
  });
  const auto result = ImportPresetArchive(zip, tmp + "/out");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(File::Exists(tmp + "/evil.slang"));
  File::DeleteDirRecursively(tmp);
}
```

Add the module files to `Source/Core/VideoCommon/CMakeLists.txt` and register `add_dolphin_test(PresetArchiveTest PostProcessing/PresetArchiveTest.cpp)`. No extra link line is needed: `add_dolphin_test` links `core`, and `MINIZIP::minizip-ng` is PUBLIC on `common` which `core` re-exports PUBLIC (`Source/Core/Common/CMakeLists.txt:186`, `Source/Core/Core/CMakeLists.txt:672-674`), so the minizip headers/symbols are available transitively — verified against the checked-out submodule.

Verified APIs (minizip-ng submodule `55db144`, headers `Externals/minizip-ng/minizip-ng/{mz.h,mz_zip.h,mz_zip_rw.h}`):
- `File::CreateTempDir()` / `File::DeleteDirRecursively()` / `File::CreateFullPath()` / `File::Exists()` all exist (`Common/FileUtil.h:220,199,165,142`).
- `mz_zip_writer_add_buffer(void*, void* buf, int32_t len, mz_zip_file*)` (`mz_zip_rw.h:200`), `mz_zip_writer_open_file(handle, path, int64_t disk_size, uint8_t append)` (`mz_zip_rw.h:169`).
- `mz_zip_file` fields: `const char* filename`, `int64_t uncompressed_size`, `uint16_t flag` (`mz_zip.h:43,35,28`); constants `MZ_OK` (0), `MZ_END_OF_LIST` (-100), `MZ_ZIP_FLAG_UTF8` (1<<11) (`mz.h:21,28,85`).

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target PresetArchiveTest 2>&1 | tail -20`
Expected: FAIL — `PresetArchive.h` / `ImportPresetArchive` undefined.

- [ ] **Step 3: Write the header**

Create `PresetArchive.h` with the `PresetImportResult` struct and `ImportPresetArchive` declaration from the **Produces** block (include `<string>`).

- [ ] **Step 4: Implement extraction**

Create `PresetArchive.cpp`. Follow the `ResourcePack.cpp` reader idiom exactly:
- `void* reader = mz_zip_reader_create();` with a `Common::ScopeGuard` calling `mz_zip_reader_delete(&reader)`.
- `mz_zip_reader_open_file(reader, zip_path.c_str())` — on non-`MZ_OK`, return `{false, "", "could not open archive"}`.
- Iterate entries with `mz_zip_reader_goto_first_entry` / `mz_zip_reader_goto_next_entry` until `MZ_END_OF_LIST`. For each, `mz_zip_reader_entry_get_info(reader, &info)` and take `std::string name(info->filename)`.
- **Security — path sanitization (the reason for the traversal test):** reject the whole import if any entry name, after normalizing separators to `/`, is absolute (starts with `/`) or contains a `..` path segment. This prevents Zip-Slip writes outside `dest_root`.
- Skip directory entries (name ends with `/`, or `info->uncompressed_size == 0` combined with a trailing slash).
- Track `.slangp` entries: collect names ending in `.slangp`. If zero found → `{false, "", "archive contains no .slangp preset"}`. If more than one, pick the shallowest (fewest `/`); if still ambiguous, error `"archive contains multiple .slangp presets"`.
- Extract each non-directory entry: compute `out_path = dest_root + "/" + name`, `File::CreateFullPath(out_path)` (creates parent dirs), read via `Common::ReadFileFromZip` into a `std::vector<u8>` sized to `info->uncompressed_size`, then write with `File::IOFile(out_path, "wb").WriteBytes(buffer.data(), buffer.size())` (`Common/IOFile.h:92`). On any write failure, return `{false, "", "failed to write " + name}`.
- After extraction, derive `preset_name` from the chosen `.slangp` entry: strip any directory prefix and the `.slangp` suffix (basename without extension), matching what `MultipassPostProcessing::GetPresetList()` returns and what `GFX_ENHANCE_POST_SHADER` stores. Return `{true, preset_name, ""}`.

Add includes: `<mz.h>`, `<mz_zip.h>`, `<mz_zip_rw.h>`, `"Common/MinizipUtil.h"`, `"Common/FileUtil.h"`, `"Common/IOFile.h"`, `"Common/ScopeGuard.h"`, `"Common/Logging/Log.h"`.

> NOTE for implementer: minizip-ng also offers `mz_zip_reader_save_all(reader, dest_dir)` which extracts everything preserving paths in one call. It is simpler but does its own path handling — if you use it, still perform the pre-scan sanitization pass above and reject before calling `save_all`, since we must not rely on the library's traversal policy. The manual per-entry loop is preferred here because it lets us both sanitize and locate the `.slangp` in a single pass.

- [ ] **Step 5: Run test to verify it passes**

Run: `cd /Users/ilya.lissoboi/work/dolphin && cmake --build build --target PresetArchiveTest 2>&1 | tail -20 && ./build/Binaries/PresetArchiveTest`
Expected: PASS — extraction preserves `src/masks/m.png`, missing-preset and traversal archives are rejected, nothing is written outside `dest_root`.

- [ ] **Step 6: Manual end-to-end (desktop)**

Zip a real crt-royale tree (`zip -r crt-royale.zip crt-royale.slangp shaders/ blurs/`), then from a small harness or a temporary debug menu call `ImportPresetArchive(zip, File::GetUserPath(D_SHADERS_IDX))` and confirm the preset appears in `MultipassPostProcessing::GetPresetList()` and renders per Task 11. (Wiring a desktop "Import preset…" button in Qt `EnhancementsWidget` is optional polish; the core is the reusable piece.)

- [ ] **Step 7: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/PresetArchive.h Source/Core/VideoCommon/PostProcessing/PresetArchive.cpp Source/Core/VideoCommon/PostProcessing/PresetArchiveTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/Core/DolphinLib.props Source/UnitTests/VideoCommon/CMakeLists.txt
git commit -m "VideoCommon: add zip-based slang preset bundle import"
```

---

## Task 13: Download the RetroArch slang shader pack from the libretro buildbot

RetroArch's entire slang shader library is published as a single zip on the libretro buildbot. This task adds a cross-platform downloader that fetches that zip over HTTP and extracts it into the user Shaders dir via Task 12's `ExtractSanitizedArchive`, so every bundled preset (crt-royale and hundreds more) becomes selectable through `MultipassPostProcessing::GetPresetList()` (Task 10) without the user hunting down individual bundles. This is the network-fetch counterpart to Task 12's local-file import; both share the same sanitized extractor.

> **Verified against the live buildbot (2026-07-12) and the checked-out tree:**
> - The pack URL is **`https://buildbot.libretro.com/assets/frontend/shaders_slang.zip`** (HTTP 200, `application/zip`, ~53 MB, supports range/redirects). Sibling assets confirm the naming: `shaders_glsl.zip`, `shaders_cg.zip` live in the same `/assets/frontend/` directory. The older `/nightly/shaders/…` path returns 404 — do **not** use it.
> - The archive is a **whole library tree**, not a single preset: top-level `crt/`, `blurs/`, `include/`, `bezel/`, `border/`, … with many `.slangp` files (e.g. `crt/crt-royale.slangp`, `border/sgb/sgb-crt-royale.slangp`). This is why the downloader must NOT go through `ImportPresetArchive`'s single-`.slangp` rule and instead uses `ExtractSanitizedArchive` (Task 12).
> - Dolphin already bundles an HTTP client: `Common::HttpRequest` (`Common/HttpRequest.h`), with `Response Get(url, headers, AllowedReturnCodes)` returning `std::optional<std::vector<u8>>`, plus `FollowRedirects(long)` and a `ProgressCallback` ctor arg. Existing download call sites to mirror: `Core/GeckoCodeConfig.cpp:22-29` (simple Get) and `UpdaterCommon/UpdaterCommon.cpp:64,212` (progress callback + long timeout).

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/ShaderPackDownload.h`
- Create: `Source/Core/VideoCommon/PostProcessing/ShaderPackDownload.cpp`
- Create: `Source/Core/VideoCommon/PostProcessing/ShaderPackDownloadTest.cpp`
- Modify: `Source/Core/VideoCommon/CMakeLists.txt`, `Source/Core/DolphinLib.props`, `Source/UnitTests/VideoCommon/CMakeLists.txt`

**Interfaces:**
- Consumes: `ExtractSanitizedArchive` (Task 12); `Common::HttpRequest` (`Common/HttpRequest.h`); `File::GetUserPath(D_SHADERS_IDX)`, `File::CreateTempDir`, `File::IOFile`, `File::DeleteDirRecursively` (`Common/FileUtil.h`, `Common/IOFile.h`).
- Produces:
  ```cpp
  namespace VideoCommon {
  // The canonical libretro slang shader pack on the buildbot (verified 2026-07-12).
  constexpr char SLANG_SHADER_PACK_URL[] =
      "https://buildbot.libretro.com/assets/frontend/shaders_slang.zip";

  struct ShaderPackDownloadResult {
    bool ok = false;
    u32 preset_count = 0;   // number of .slangp files extracted from the pack
    std::string error;      // set when ok == false
  };

  // Progress callback: (bytes_downloaded, bytes_total). bytes_total may be 0 if the server
  // does not report Content-Length. Return false to cancel the download. May be null.
  using DownloadProgress = std::function<bool(s64 downloaded, s64 total)>;

  // Downloads the slang shader pack from `url` and extracts it (sanitized) into `dest_root`
  // (typically File::GetUserPath(D_SHADERS_IDX)). Blocking; run off the UI thread. The zip is
  // written to a temp file, extracted via ExtractSanitizedArchive, then the temp file removed.
  ShaderPackDownloadResult DownloadAndInstallShaderPack(const std::string& url,
                                                        const std::string& dest_root,
                                                        DownloadProgress progress = nullptr);
  }  // namespace VideoCommon
  ```

- [ ] **Step 1: Write the failing test**

The download itself needs the network, so the unit test covers the **extraction-of-a-downloaded-zip** seam without hitting the buildbot: build a small multi-preset zip on disk (reuse the `WriteZip` helper pattern from `PresetArchiveTest.cpp`), then feed it to a `file://`-style local path through a seam. To keep the test hermetic and network-free, split the network fetch from the install so the install half is directly testable:

Add a second, testable entry point in the header:

```cpp
// Extracts an already-downloaded pack zip at local_zip_path into dest_root. This is the
// network-free half of DownloadAndInstallShaderPack (which fetches then calls this). Counts
// the .slangp files extracted.
ShaderPackDownloadResult InstallShaderPackFromZip(const std::string& local_zip_path,
                                                  const std::string& dest_root);
```

Create `ShaderPackDownloadTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/FileUtil.h"
#include "VideoCommon/PostProcessing/ShaderPackDownload.h"

using namespace VideoCommon;

namespace
{
void WriteZip(const std::string& zip_path,
              const std::vector<std::pair<std::string, std::string>>& entries)
{
  void* writer = mz_zip_writer_create();
  ASSERT_EQ(mz_zip_writer_open_file(writer, zip_path.c_str(), 0, 0), MZ_OK);
  for (const auto& [name, data] : entries)
  {
    mz_zip_file file_info = {};
    file_info.filename = name.c_str();
    file_info.flag = MZ_ZIP_FLAG_UTF8;
    ASSERT_EQ(mz_zip_writer_add_buffer(writer, const_cast<char*>(data.data()),
                                       static_cast<int32_t>(data.size()), &file_info),
              MZ_OK);
  }
  mz_zip_writer_close(writer);
  mz_zip_writer_delete(&writer);
}
}  // namespace

TEST(ShaderPackDownload, InstallsAllPresetsFromPackZip)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/shaders_slang.zip";
  const std::string dest = tmp + "/Shaders";

  // Mirrors the buildbot pack shape: many presets across subdirs sharing an include/ tree.
  WriteZip(zip, {
      {"crt/crt-royale.slangp", "shaders = \"1\"\nshader0 = \"../include/a.slang\"\n"},
      {"crt/crt-geom.slangp", "shaders = \"1\"\nshader0 = \"../include/a.slang\"\n"},
      {"include/a.slang",
       "#pragma stage vertex\nvoid main(){}\n#pragma stage fragment\nvoid main(){}\n"},
      {"README.md", "libretro slang shaders"},
  });

  const auto result = InstallShaderPackFromZip(zip, dest);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.preset_count, 2u);
  EXPECT_TRUE(File::Exists(dest + "/crt/crt-royale.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/crt/crt-geom.slangp"));
  EXPECT_TRUE(File::Exists(dest + "/include/a.slang"));  // shared include preserved

  File::DeleteDirRecursively(tmp);
}

TEST(ShaderPackDownload, RejectsCorruptZip)
{
  const std::string tmp = File::CreateTempDir();
  ASSERT_FALSE(tmp.empty());
  const std::string zip = tmp + "/bad.zip";
  File::IOFile(zip, "wb").WriteBytes("not a zip", 9);
  const auto result = InstallShaderPackFromZip(zip, tmp + "/Shaders");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.empty());
  File::DeleteDirRecursively(tmp);
}
```

Add the module files to `Source/Core/VideoCommon/CMakeLists.txt` and register `add_dolphin_test(ShaderPackDownloadTest PostProcessing/ShaderPackDownloadTest.cpp)`. `add_dolphin_test` links `core`, which re-exports `common` PUBLIC; `Common::HttpRequest` and `MINIZIP::minizip-ng` are both on `common`, so no extra link line is needed (same transitive path Task 12 verified).

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target ShaderPackDownloadTest 2>&1 | tail -20`
Expected: FAIL — `ShaderPackDownload.h` / `InstallShaderPackFromZip` undefined.

- [ ] **Step 3: Write the header**

Create `ShaderPackDownload.h` with `SLANG_SHADER_PACK_URL`, `ShaderPackDownloadResult`, `DownloadProgress`, `DownloadAndInstallShaderPack`, and the testable `InstallShaderPackFromZip` from the blocks above (include `<functional>`, `<string>`, `"Common/CommonTypes.h"`).

- [ ] **Step 4: Implement**

Create `ShaderPackDownload.cpp`:
- **`InstallShaderPackFromZip`** (network-free): call `ExtractSanitizedArchive(local_zip_path, dest_root, &error)` (Task 12). On empty return with a set error → `{false, 0, error}`. Otherwise `{true, static_cast<u32>(preset_names.size()), ""}`. Note the extractor already creates parent dirs and rejects Zip-Slip, so this is a thin adapter.
- **`DownloadAndInstallShaderPack`** (network): 
  - Construct `Common::HttpRequest http(std::chrono::seconds(60), curl_progress)`, where `curl_progress` adapts the caller's `DownloadProgress` to the `HttpRequest::ProgressCallback` signature `(s64 dltotal, s64 dlnow, s64, s64)` (return `progress ? progress(dlnow, dltotal) : true`). Call `http.FollowRedirects(10)` (the buildbot may 30x).
  - `const auto response = http.Get(url);` If `!response` → `{false, 0, "download failed (HTTP " + std::to_string(http.GetLastResponseCode()) + ")"}`.
  - Write the bytes to a temp file: `const std::string tmp_dir = File::CreateTempDir();` (guard empty), `const std::string zip_path = tmp_dir + DIR_SEP "shaders_slang.zip";` then `File::IOFile(zip_path, "wb").WriteBytes(response->data(), response->size())`. Use a `Common::ScopeGuard` to `File::DeleteDirRecursively(tmp_dir)` on exit so the temp zip never leaks even on early return.
  - `return InstallShaderPackFromZip(zip_path, dest_root);`
- Log start/finish + byte count with `INFO_LOG_FMT(VIDEO, ...)`; log failures with `ERROR_LOG_FMT`.

Add includes: `"VideoCommon/PostProcessing/ShaderPackDownload.h"`, `"VideoCommon/PostProcessing/PresetArchive.h"`, `"Common/HttpRequest.h"`, `"Common/FileUtil.h"`, `"Common/IOFile.h"`, `"Common/CommonPaths.h"`, `"Common/ScopeGuard.h"`, `"Common/Logging/Log.h"`, `<chrono>`.

- [ ] **Step 5: Run test to verify it passes**

Run: `cd /Users/ilya.lissoboi/work/dolphin && cmake --build build --target ShaderPackDownloadTest 2>&1 | tail -20 && ./build/Binaries/ShaderPackDownloadTest`
Expected: PASS — both presets extracted from the multi-preset pack, shared `include/` preserved, corrupt zip rejected.

- [ ] **Step 6: Manual end-to-end (desktop, real network)**

From a small harness or temporary debug menu, call
`DownloadAndInstallShaderPack(SLANG_SHADER_PACK_URL, File::GetUserPath(D_SHADERS_IDX), progress)`.
Expected: ~53 MB downloads, `preset_count` in the hundreds, and `crt-royale` now appears in `MultipassPostProcessing::GetPresetList()` and renders per Task 11 — no manual asset staging (Task 11 Step 1) required. Wiring a "Download shader pack…" button into Qt `EnhancementsWidget` is done in Step 7 below. Android UI wiring stays in the Android follow-on plan.

- [ ] **Step 7: Wire a "Download shader pack…" button into the Qt Enhancements UI**

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.h`
- Modify: `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.cpp`

> **Verified against the checked-out tree:** the post-processing row already occupies `enhancements_layout` row `row` with the combo at column 1 and the "Configure" button (`m_configure_post_processing_effect`) at column 2 (`EnhancementsWidget.cpp:188-190`). **Task 10 Step 4 removes that Configure button** (the old `PostProcessingConfigWindow` wiring), freeing column 2 for the download button. The layout uses `QGridLayout` (`EnhancementsWidget.cpp:66`), buttons are `NonDefaultQPushButton` (`.cpp:26,147`), and the established button pattern — construct → `addWidget(..., row, 2)` → `connect(&QPushButton::clicked, this, &EnhancementsWidget::Handler)` — is exactly how `m_configure_color_correction` is wired (`.cpp:185,278-279`).

- In `EnhancementsWidget.h`, add a member `QPushButton* m_download_shader_pack;` (next to the removed `m_configure_post_processing_effect`) and a private slot `void DownloadShaderPack();`.
- In `EnhancementsWidget.cpp` `CreateWidgets()`: construct `m_download_shader_pack = new NonDefaultQPushButton(tr("Download…"));` and place it in the freed slot: `enhancements_layout->addWidget(m_download_shader_pack, row, 2);` (the post-processing-effect row).
- In `ConnectWidgets()`: `connect(m_download_shader_pack, &QPushButton::clicked, this, &EnhancementsWidget::DownloadShaderPack);` (mirroring the `m_configure_color_correction` connect at `.cpp:278`).
- Implement `DownloadShaderPack()`:
  - `#include "VideoCommon/PostProcessing/ShaderPackDownload.h"`, plus `<QProgressDialog>`, `<QMessageBox>`, `<QtConcurrent>` (or `QThread`).
  - The download is blocking (~53 MB), so it MUST run off the UI thread. Show a modal `QProgressDialog` (range 0–100, cancel button), run `DownloadAndInstallShaderPack(SLANG_SHADER_PACK_URL, File::GetUserPath(D_SHADERS_IDX), progress)` on a worker (`QtConcurrent::run` / `QThread`), and marshal progress back to the dialog via a queued signal. The `DownloadProgress` lambda computes a percentage from `(downloaded, total)` (guard `total == 0`) and returns `!dialog.wasCanceled()` so cancel aborts the transfer.
  - On completion (back on the UI thread): if `result.ok`, `QMessageBox::information` reporting `result.preset_count` presets installed, then call `LoadPostProcessingShaders()` so the newly-downloaded presets populate the combo immediately; else `QMessageBox::warning` with `result.error`.
- Add the include `"Common/FileUtil.h"` for `File::GetUserPath` if not already present.

Run: `cmake --build build --target dolphin-emu-qt 2>&1 | tail -20`
Expected: BUILD SUCCESS. Manual check: click **Download…** → progress dialog advances → on success the preset combo repopulates and `crt-royale` is selectable.

- [ ] **Step 8: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing/ShaderPackDownload.h Source/Core/VideoCommon/PostProcessing/ShaderPackDownload.cpp Source/Core/VideoCommon/PostProcessing/ShaderPackDownloadTest.cpp Source/Core/VideoCommon/CMakeLists.txt Source/Core/DolphinLib.props Source/UnitTests/VideoCommon/CMakeLists.txt Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.h Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.cpp
git commit -m "VideoCommon/Qt: download slang shader pack from libretro buildbot"
```

---

## Self-Review Notes

- **Spec coverage — replacing the single-pass processor:** old dialect/classes removed (Task 10); new N-pass executor is the sole post-processor (Task 9); `Presenter` repointed (Task 10). Multi-pass `.slangp` support: parser (T1), shader/pragma parse (T2), sizing (T3), translation (T4) + Vulkan compile (T5), samplers (T6), mips (T7), LUTs (T8), executor (T9). crt-royale milestone verified (T11).
- **Scope discipline:** cross-backend breadth, history/feedback frames, `#reference`, Android tree-import, and parameter UI are explicitly deferred to follow-on plans (Out of Scope section) so this plan yields working software (crt-royale on Vulkan) on its own. Task 12 (zip bundle import) is the platform-agnostic extraction core matching the ecosystem's directory-tree-in-a-zip distribution convention; the Android UI that calls it stays in the Android follow-on plan.
- **Getting the shaders (Task 13):** rather than requiring users to manually stage a crt-royale tree (Task 11 Step 1), Task 13 downloads the entire libretro slang shader library from the buildbot (`https://buildbot.libretro.com/assets/frontend/shaders_slang.zip`, verified live 2026-07-12) via the already-bundled `Common::HttpRequest` and installs it through Task 12's shared `ExtractSanitizedArchive`. Because the pack is a whole-library tree with hundreds of presets, Task 12's single-`.slangp` `ImportPresetArchive` is refactored to sit on top of the reusable multi-preset extractor; Task 13 reuses that extractor directly. Network fetch and zip install are split so the install half stays unit-testable without touching the network. The Qt "Download…" button reuses the column-2 layout slot freed when Task 10 Step 4 removes the old Configure button, and runs the blocking download on a worker thread behind a cancelable progress dialog.
- **Distribution convention:** slang shaders ship as a directory tree (preset + sources + includes + PNGs) referenced by preset-relative paths incl. `../`, commonly zipped for transport. Task 12 extracts that tree preserving structure so path/`#include` resolution (Task 1 path resolver, Task 5 includer) works unchanged; single-file `.slang` import (the earlier `.glsl` flow) is insufficient for presets.
- **Type consistency:** `ScaleType`/`SlangWrapMode` defined in T1 and reused in T3/T6; `SlangPassConfig`/`SlangLutConfig` (T1) consumed by T4/T6/T8/T9; `SlangShaderSource` (T2) → `TranslateSlangPass` (T4) → `CompiledPassShaders`/`CompileTranslatedPass` (T5) → executor (T9); `MakeSlangSamplerState` (T6), `GenerateBoxMips`/`MipLevel` (T7), `LoadLutTexture` (T8) all consumed by T9. `MultipassPostProcessing` (T9) consumed by T10.
- **Known implementer confirmations (flagged inline, not placeholders):** UBO std140 packing must match each shader's declared member order — MVP assumes crt-royale's set, generalize via SPIR-V reflection later (T9 Step 4); the exact set of UI referencers to repoint is grep-driven (T10 Step 1); whether the earlier import plan's `PostProcessingValidationTest` has landed affects T10 Step 5. (T12's minizip-ng APIs, `mz_zip_file` fields, `File::` helpers, and transitive link path were verified against the checked-out submodule `55db144`.)
- **Risk callouts:** the two hardest pieces are T4 (slang→Dolphin binding/semantic rewrite compiling identically) and T9 Step 4 (UBO packing + Vulkan `FinishedRendering()` layout transitions between passes). Both are isolated behind unit tests (T4) or a single helper (T9) to contain iteration.
