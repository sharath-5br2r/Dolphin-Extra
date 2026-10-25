# Change-Delta Analysis: Multi-Pass `.slangp` Post-Processing Support

**Status:** Analysis / feasibility assessment (not an implementation plan).
**Date:** 2026-07-12
**Scope:** What it takes to run RetroArch-style multi-pass shader presets (e.g. `crt-royale.slangp`, 12 passes + 6 LUT textures) in Dolphin, on top of the arbitrary-location file-selection work analyzed separately (`2026-07-12-android-custom-post-processing-shader-import.md`).
**Explicit constraint from the requester:** backwards compatibility with the existing `.glsl` post-processing effects and pipeline is **not required**. This permits replacing Dolphin's bespoke single-pass dialect with a slang pipeline outright rather than supporting both.

---

## 1. What a `.slangp` preset demands (using `crt-royale.slangp` as the reference)

The reference preset exercises essentially every feature of the format:

- **Preset file (`.slangp`)** — INI-like `key = "value"`. Declares `shaders = "12"` and, per pass index `N`:
  - `shaderN` — path to a `.slang` file, **relative to the preset**, including parent traversal (`"../blurs/shaders/royale/blur9fast-vertical.slang"`).
  - `aliasN` — a name later passes reference as a texture (`ORIG_LINEARIZED`, `BLOOM_APPROX`, `MASK_RESIZE`, …).
  - `filter_linearN`, `wrap_modeN`, `mipmap_inputN` — per-pass input sampler traits.
  - `scale_typeN` / `scale_type_xN` / `scale_type_yN` ∈ {`source`, `viewport`, `absolute`} with `scaleN` / `scale_xN` / `scale_yN` — per-axis output size of the pass's render target.
  - `srgb_framebufferN` — the pass renders into an sRGB target.
  - `float_framebufferN` (not in this preset but in the format) — float render target.
- **Global LUT textures** — `textures = "a;b;c"` naming samplers, each with `<name> = "path.png"`, `<name>_wrap_mode`, `<name>_linear`, `<name>_mipmap`. crt-royale loads 6 phosphor-mask PNGs, some mipmapped (`_large`), some not (`_small`), all `repeat`-wrapped.
- **Semantic textures a pass can sample** (slang runtime contract): `Source` (previous pass output), `Original` (pipeline input), `OriginalHistory0..N` (prior frames of the input), each named alias (`ORIG_LINEARIZED`, etc.), `PassOutput`/`PassFeedback` (a pass's own previous-frame output), and every declared LUT.
- **`.slang` shader files** — Vulkan-dialect GLSL, one file containing both stages, delimited by `#pragma stage vertex` / `#pragma stage fragment`; `#pragma name <alias>`; `#pragma format <fmt>`; `#pragma parameter <id> "<label>" <default> <min> <max> <step>`; `#include` of shared headers (crt-royale has deep include trees under `src/` and `../../../include/`).
- **Semantic uniforms** in a `layout(std140) uniform UBO { mat4 MVP; vec4 SourceSize; vec4 OriginalSize; vec4 OutputSize; uint FrameCount; int FrameDirection; float <param>...; }` plus per-pass `<AliasName>Size` members, and push-constant blocks in newer presets.
- **`user-preset-constants.h`** — crt-royale's header comment shows some constants (mask sizes, scales, `geom_max_aspect_ratio`) must be kept in sync manually; a faithful importer either respects that header or derives the values.

None of this maps onto Dolphin's current post-processing dialect, which injects helper macros (`Sample()`, `SetOutput()`, `GetOption()`), exposes a single source sampler (`samp0`), uses one fixed UBO layout, and has a hardcoded 1-or-2-pass chain.

---

## 2. Capability audit — Dolphin today vs. what's needed

Grounded in the current code. "✅ reuse" = exists and is directly usable; "⚠️ gap" = partial; "❌ new" = must be built.

### 2.1 GPU orchestration primitives — ✅ almost entirely present

The low-level abstraction is fully capable of an N-pass render-to-texture chain:

- `AbstractGfx::CreateTexture(TextureConfig, name)` ([AbstractGfx.h:75](Source/Core/VideoCommon/AbstractGfx.h#L75)) with `flags = AbstractTextureFlag_RenderTarget` ([TextureConfig.h:41](Source/Core/VideoCommon/TextureConfig.h#L41)) → `CreateFramebuffer(color, depth, additional_color)` ([AbstractGfx.h:79-81](Source/Core/VideoCommon/AbstractGfx.h#L79-L81)) creates an arbitrary-size intermediate render target at a chosen format.
- Per-pass pipeline: `AbstractPipelineConfig` bakes the framebuffer format in via `framebuffer_state` ([AbstractPipeline.h:41-72](Source/Core/VideoCommon/AbstractPipeline.h#L41-L72)), so **each distinct RT format needs its own cached pipeline** — cheap, since configs are hashed. `RecompilePipeline` ([PostProcessing.cpp:1028-1067](Source/Core/VideoCommon/PostProcessing.cpp#L1028-L1067)) already builds two pipelines with different formats, exactly this pattern.
- Per-binding sampler state: `SetTexture(index, tex)` + `SetSamplerState(index, SamplerState)` ([AbstractGfx.h:63-64](Source/Core/VideoCommon/AbstractGfx.h#L63-L64)). The Utility pipeline exposes **8 combined image samplers** ([AbstractPipeline.h:26-27](Source/Core/VideoCommon/AbstractPipeline.h#L26-L27)) — the hard ceiling on inputs per pass. crt-royale passes stay under this, but it is a real limit to validate against.
- The full-screen draw loop already exists as a **hardcoded 2-pass chain** in `PostProcessing::BlitFromTexture` ([PostProcessing.cpp:473-606](Source/Core/VideoCommon/PostProcessing.cpp#L473-L606)): bind FB → set textures/samplers → `UploadUtilityUniforms` → `SetViewportAndScissor(ConvertFramebufferRectangle(...))` → `SetPipeline` → `Draw(0, 3)` → feed output forward. Generalizing to N passes is mechanical.
- `AbstractTexture::FinishedRendering()` ([AbstractTexture.h:37](Source/Core/VideoCommon/AbstractTexture.h#L37)) is the layout-transition hint that must be called between "render to X" and "sample X" — critical correctness detail on Vulkan.

**Verdict:** the render-graph executor is a new component, but it sits entirely on top of existing primitives. No backend GPU work is required for the orchestration itself.

### 2.2 Cross-backend shader compilation — ✅ mostly present, ⚠️ OpenGL gap

> **Verified against checked-out submodules** (glslang `a57276bf` = 11.1.0-1304, SPIRV-Cross `ebe2aa0c`): `glslang::TShader::Includer` base with `includeSystem`/`includeLocal`/`releaseInclude` virtuals + `IncludeResult` (`glslang/Public/ShaderLang.h:599-657`); `EShTargetLanguageVersion` enum incl. `EShTargetSpv_1_0..1_6` (`ShaderLang.h:166-173`); `GlslangToSpv` (`SPIRV/GlslangToSpv.h:62`); `CompilerGLSL`, `CompilerHLSL : CompilerGLSL`, `CompilerMSL : CompilerGLSL` with `compile()` (`spirv_{glsl,hlsl,msl}.hpp`). `spirv_glsl.cpp` **is compiled** into Dolphin's bundle (`Externals/spirv_cross/CMakeLists.txt:20`) yet `CompilerGLSL` has **zero references in `Source/`** — confirming the OpenGL SPIR-V→GLSL path is present-but-unused, while `CompilerHLSL` (D3DCommon) and `CompilerMSL` (Metal) are each referenced exactly once.

Dolphin already bundles **both** halves of the slang toolchain and runs single-source Vulkan-GLSL through them on 4 of 5 backends:

- glslang (`Externals/glslang/`) and SPIRV-Cross (`Externals/spirv_cross/`, including `spirv_glsl/hlsl/msl.cpp`) are submodules, compiled and linked.
- The shared GLSL→SPIR-V chokepoint is `SPIRV::CompileVertexShader/CompileFragmentShader(source, APIType, EShTargetLanguageVersion, Includer*)` ([Spirv.h:23-40](Source/Core/VideoCommon/Spirv.h#L23-L40), impl [Spirv.cpp:159-256](Source/Core/VideoCommon/Spirv.cpp#L159-L256)). Vulkan **and Metal** already parse with `EShMsgVulkanRules` ([Spirv.cpp:172](Source/Core/VideoCommon/Spirv.cpp#L172)) — the exact dialect slang uses.
- Per backend, `CreateShaderFromSource` already does:
  - **Vulkan** → SPIR-V → `vkCreateShaderModule` (`VideoBackends/Vulkan/ShaderCompiler.cpp`, `VKShader.cpp:109`).
  - **D3D11 / D3D12** → SPIR-V → `spirv_cross::CompilerHLSL` → HLSL → runtime `D3DCompile` (`VideoBackends/D3DCommon/Shader.cpp:92,242`).
  - **Metal** → SPIR-V → `spirv_cross::CompilerMSL` → MSL, with explicit resource-binding remap (`VideoBackends/Metal/MTLUtil.mm:492-598`).
  - **OpenGL** → **feeds GLSL straight to `glCompileShader`, never touches SPIR-V** (`VideoBackends/OGL/OGLShader.cpp:157`, `ProgramShaderCache.cpp:329`).

**Gaps:**
- **OpenGL** has no SPIR-V→GLSL path. `spirv_cross::CompilerGLSL` is compiled into the bundle but **unused anywhere in `Source/`**. Two options: (a) add a SPIRV-Cross GLSL emit specifically for slang on the GL backend, or (b) restrict slang presets to the SPIR-V-capable backends (Vulkan/D3D11/D3D12/Metal) and gray out the feature on GL. Given "no backwards-compat requirement," (b) is a legitimate MVP scope cut.
- **Per-backend header divergence** ([ShaderCompiler.cpp:22-59](Source/Core/VideoBackends/Vulkan/ShaderCompiler.cpp#L22-L59) vs [D3DCommon/Shader.cpp:36-66](Source/Core/VideoBackends/D3DCommon/Shader.cpp#L36-L66) vs Metal `SHADER_HEADER`): each backend prepends a different binding-layout preamble (`set/binding` descriptor sets for Vulkan/Metal, flat `binding=` for D3D). A slang front-end must normalize slang's `layout(set,binding)` to whatever the target backend expects before hitting `SPIRV::Compile*`.
- **D3D11 geometry-shader** path returns `nullptr` from the HLSL cross-compiler ([D3DCommon/Shader.cpp:126-130](Source/Core/VideoBackends/D3DCommon/Shader.cpp#L126-L130)). Not a slang blocker (slang presets are VS+PS only), but worth noting.
- There is **no shared cross-backend AST** — `ShaderGenCommon` is string-templating with `APIType` branches, not an IR. The glslang+SPIRV-Cross pipeline *is* the single-source mechanism to build on.

**Verdict:** the expensive machinery (glslang + SPIRV-Cross, wired per backend) is already there. Net-new compile work is one OpenGL path (or a scope cut) plus a slang front-end that rewrites bindings/uniforms.

### 2.3 LUT texture / image loading — ✅ decode+upload present, ⚠️ mipmaps & sRGB gaps

- PNG decode: `Common::LoadPNG(span, UniqueBuffer<u8>*, w, h)` ([Common/Image.h:14](Source/Core/Common/Image.h#L14), impl [Image.cpp:30-56](Source/Core/Common/Image.cpp#L30-L56)) via libspng, **always RGBA8**. Wrapped for texture use by `LoadPNGTexture` ([Assets/CustomTextureData.cpp:566,579](Source/Core/VideoCommon/Assets/CustomTextureData.cpp#L566)).
- CPU-pixels → sampled texture: `CreateTexture(TextureConfig)` + per-level `AbstractTexture::Load(level, w, h, row_length, buffer, size)` ([AbstractTexture.h:32](Source/Core/VideoCommon/AbstractTexture.h#L32)). Canonical example: `TextureCacheBase::CreateTextureEntry` ([TextureCacheBase.cpp:1614-1646](Source/Core/VideoCommon/TextureCacheBase.cpp#L1614-L1646)).
- Path-based asset loading with automatic `_mipN` sibling discovery: `LoadTextureDataFromFile` (`Assets/TextureAssetUtils.cpp:68`), and the directory-scan pattern in `HiresTextures.cpp:79-240`.
- Sampler control: `SamplerState` ([RenderState.h:163-216](Source/Core/VideoCommon/RenderState.h#L163-L216)) covers filter (Near/Linear), wrap (Clamp/Repeat/Mirror), mipmap filter, LOD clamp/bias. Populate the bitfield union directly from preset values and bind with `SetSamplerState`.

**Gaps:**
- **No runtime mipmap generation.** Dolphin only *consumes* pre-supplied mips (`_mipN` files); there is no `GenerateMipmaps`. A LUT with `_mipmap = true` (crt-royale's `_large` masks) needs either a CPU box-filter mip builder or a new backend blit-based mip generator. **New code.**
- **No sRGB texture formats.** `AbstractTextureFormat` ([TextureConfig.h:13-30](Source/Core/VideoCommon/TextureConfig.h#L13-L30)) has no `*_SRGB` variants; all 8-bit formats map to UNORM in backends. Backends *know* sRGB VkFormats ([VKTexture.cpp:170-189](Source/Core/VideoBackends/Vulkan/VKTexture.cpp#L170-L189)) but the abstract layer never requests them. `srgb_framebufferN` and hardware sRGB sampling are **not expressible** — must add sRGB formats or do sRGB↔linear conversion in-shader.
- **No `clamp_to_border` / `mirror_clamp` wrap.** `WrapMode` ([BPMemory.h:886-893](Source/Core/VideoCommon/BPMemory.h#L886-L893)) is Clamp/Repeat/Mirror only. crt-royale uses only `repeat`/`clamp_to_edge`, so this is not a blocker for the reference preset but is a general-compat gap.

### 2.4 Format availability — ⚠️ RGBA16F only float target

`RGBA16F` exists and is already used as the intermediary buffer ([PostProcessing.cpp:40](Source/Core/VideoCommon/PostProcessing.cpp#L40)), covering slang `float`/`FLOAT` framebuffers. There is no `R16F`/`RG16F`/`R11G11B10F`, so passes preferring a cheaper float target round up to RGBA16F (more bandwidth, functionally correct). Acceptable for an MVP.

### 2.5 History / feedback frames — ❌ new

The slang format supports `OriginalHistoryN` (prior input frames) and per-pass `feedback` (a pass sampling its own previous-frame output). Dolphin's intermediary buffer is per-frame and reallocated on resize — nothing keeps a cross-frame ring. crt-royale does **not** use feedback and uses only the current frame, so this can be **deferred / scoped out** for a crt-royale-first MVP, but the format contract needs it for full compatibility.

---

## 3. The change delta, by component

Ordered from foundational to peripheral. Each is a distinct buildable unit.

### C1. `.slangp` / `.slang` preset + shader parser (❌ new)
- New `VideoCommon/PostProcessing/` module (e.g. `SlangPreset.{h,cpp}`, `SlangShader.{h,cpp}`).
- Parse the preset INI (`shaders`, per-pass `shaderN`/`aliasN`/`scale*`/`filter_linearN`/`wrap_modeN`/`mipmap_inputN`/`srgb_framebufferN`/`float_framebufferN`; global `textures` list with per-texture traits; `#reference` nested-preset inclusion).
- Resolve **preset-relative paths including `../`** for `.slang` sources and PNG LUTs.
- Parse `.slang`: split `#pragma stage vertex|fragment`, collect `#pragma name/format/parameter`, resolve `#include` (reuse `VideoCommon::ShaderIncluder`, [ShaderCompileUtils.h](Source/Core/VideoCommon/ShaderCompileUtils.h)).
- Reuse the existing `Common::IniFile::ParseLine` tokenizer already used by `PostProcessingConfiguration::LoadOptions` ([PostProcessing.cpp:113-255](Source/Core/VideoCommon/PostProcessing.cpp#L113-L255)).

### C2. Slang → Dolphin shader front-end (❌ new, ⚠️ builds on existing compile path)
- Rewrite slang semantic uniforms (`MVP`, `SourceSize`, `OriginalSize`, `OutputSize`, `FrameCount`, `FrameDirection`, `<Alias>Size`, `#pragma parameter` values) into a synthesized UBO, analogous to `BuiltinUniforms` ([PostProcessing.cpp:839](Source/Core/VideoCommon/PostProcessing.cpp#L839)).
- Rewrite slang `layout(set,binding) sampler2D Source/Original/<Alias>/User#` bindings to Dolphin's `SAMPLER_BINDING`/`UBO_BINDING` macro conventions, per-backend (§2.2 divergence).
- Emit two sources (VS, PS) and feed each to `SPIRV::Compile*` / `g_gfx->CreateShaderFromSource` ([AbstractGfx.h:114](Source/Core/VideoCommon/AbstractGfx.h#L114)).
- **OpenGL:** add a `spirv_cross::CompilerGLSL` emit path, or restrict slang to SPIR-V backends (recommended MVP cut).

### C3. Multi-pass render-graph executor (❌ new, ✅ on existing primitives)
- Replace the fixed intermediary in `PostProcessing` with a `std::vector<Pass>` where `Pass = { AbstractTexture (RT + optional mips), AbstractFramebuffer, AbstractPipeline (VS+PS), SamplerState[inputs], input-binding table }`.
- Compute each pass's RT size from `scale_type{,_x,_y}` × {source size, viewport size, absolute} — replicate RetroArch's semantics exactly (crt-royale's mask-resize math in the preset comments is unforgiving; get `viewport`/`source`/`absolute` right).
- Per frame: for each pass, bind its declared inputs (`Source`=prev, `Original`=input, aliases=named earlier passes' RTs, LUTs), set per-input `SamplerState`, upload the pass UBO (fill `*Size`/`FrameCount`/params), set pipeline, `Draw(0,3)`, call `FinishedRendering()` before the next pass samples it.
- Final pass renders to the real backbuffer/format. Model on `BlitFromTexture` ([PostProcessing.cpp:473-606](Source/Core/VideoCommon/PostProcessing.cpp#L473-L606)); this replaces the current 2-pass body.
- Validate the ≤8-samplers-per-pass limit at load and report over-limit passes as errors.

### C4. LUT loader + mipmap generation (⚠️ partial reuse + ❌ new mips)
- Load preset PNGs via `Common::LoadPNG` → `CreateTexture` + `Load` (§2.3).
- **New:** CPU box-filter mip generator for `_mipmap = true` LUTs (or a backend blit path). Populate `TextureConfig.levels` and `Load` each level.
- Build `SamplerState` per LUT from `_wrap_mode`/`_linear`/`_mipmap`.

### C5. Format & sampler gap-filling (⚠️ new formats)
- Decide sRGB strategy: add `RGBA8_SRGB` (+ friends) to `AbstractTextureFormat` and thread through all 5 backends' format maps (touches every `VideoBackends/*/`), **or** implement sRGB↔linear in the generated shader wrapper (contained, no backend churn — recommended MVP).
- `float_framebufferN` → RGBA16F (already available).
- Optionally add `clamp_to_border` to `WrapMode` (+ backend maps) — skippable for crt-royale.

### C6. History / feedback frames (❌ new, deferrable)
- Ring buffer of prior-frame input textures (`OriginalHistoryN`) and per-pass feedback RTs. **Not needed for crt-royale**; scope out of MVP, note as a compatibility gap.

### C7. Parameters UI (⚠️ reuse existing options system)
- Map `#pragma parameter` to the existing `PostProcessingConfiguration` option model (`OptionRangeFloat` etc., [PostProcessing.cpp:187-192](Source/Core/VideoCommon/PostProcessing.cpp#L187-L192)) so the Qt/Android option editors work unchanged. Persist under the existing `"<preset>-options"` INI convention.

### C8. Selection / packaging & the Android file-selection interaction (⚠️ significant delta on top of the earlier plan)
This is where the two features intersect and the earlier import design must change:
- A `.glsl` effect is **one file**; a `.slangp` preset is a **tree** — the preset plus many `.slang` sources, shared `#include` headers, and PNG LUTs, wired by **relative paths including `../`**.
- The earlier "pick one file, copy it into `Shaders/`" flow ([2026-07-12-android-custom-post-processing-shader-import.md](docs/superpowers/plans/2026-07-12-android-custom-post-processing-shader-import.md)) does **not** work for presets. Options:
  1. **Import a directory tree** via SAF `ACTION_OPEN_DOCUMENT_TREE` (the pattern already used for game folders in `MainPresenter.onDirectorySelected`), recursively copying the preset dir into `Shaders/`, preserving relative structure so `../` and `#include` resolve. Requires walking `DocumentFile` children and reproducing the layout.
  2. **Reference in place** via a persisted tree URI and resolve every preset-relative path through the content-URI bridge. Dolphin's C++ file layer *can* read `content://` (`IOFile`/`ReadFileToString`/`ScanDirectoryTree` are content-aware), but path construction with `../` across SAF documents via `ContentHandler.unmangle` is fragile — copying (option 1) is far more robust.
- The native validation entry from the earlier plan (`ValidateShaderSource`) generalizes to **`ValidatePreset(path)`**: parse the preset, resolve every referenced file exists, compile each `.slang` (GPU when a backend is live, else parse-only), and report the first failing pass/file. The "compile deferred when no backend" caveat from the earlier analysis still applies.

---

## 4. Effort & risk summary

| Component | New/Reuse | Effort | Primary risk |
|---|---|---|---|
| C1 Preset/`.slang` parser | New | Medium | Faithful `scale_type`/path/`#reference` semantics |
| C2 Slang→Dolphin front-end | New on existing compile | High | Per-backend binding rewrite; semantic-uniform mapping correctness |
| C3 Render-graph executor | New on existing primitives | Medium-High | Pass sizing math; Vulkan layout transitions; 8-sampler limit |
| C4 LUT + mipmap gen | Partial + new | Medium | Runtime mipmap generation (none exists) |
| C5 Format/sampler gaps | New formats or in-shader | Low-Med | sRGB strategy; backend-wide format threading if done natively |
| C6 History/feedback | New | Medium | Deferrable (crt-royale doesn't need it) |
| C7 Parameters UI | Reuse | Low | Maps onto existing options system |
| C8 Selection/packaging + Android | New on earlier plan | Medium | Directory-tree import via SAF; relative-path preservation |

**Highest-risk items:** C2 (getting slang's semantic-uniform + per-backend binding model to compile identically across 4–5 backends) and C3 (exact RetroArch pass-sizing semantics; crt-royale's mask-resize passes are numerically sensitive, per the preset's own comments). The OpenGL SPIR-V→GLSL gap (C2) is the single clearest "cut it from MVP" lever.

---

## 5. Recommended sequencing (given no backwards-compat constraint)

Because compatibility with the old dialect isn't required, the cleanest path is to build a **parallel slang pipeline that replaces** the existing single-pass post-processor rather than bolting onto it:

1. **C1 + C2 (Vulkan only), single pass** — parse a trivial 1-pass `.slangp`, compile its `.slang` on Vulkan, render it. Proves the front-end and semantic UBO end-to-end.
2. **C3** — generalize to the N-pass graph with aliases and correct per-axis scaling; validate on a 2–3 pass preset.
3. **C4 + C5 (in-shader sRGB)** — LUTs with mipmap generation; run **crt-royale end-to-end on Vulkan**. This is the headline milestone.
4. **C2 breadth** — extend to D3D11/D3D12/Metal (reuse existing SPIRV-Cross paths); decide OpenGL (add `CompilerGLSL` path or gate the feature off on GL).
5. **C7** — expose `#pragma parameter`s in the settings UI via the existing options system.
6. **C8** — directory-tree import on Android (SAF `ACTION_OPEN_DOCUMENT_TREE` + recursive copy) and generalize native validation to `ValidatePreset`.
7. **C6** — history/feedback frames, only if broad preset compatibility beyond crt-royale is wanted.

**MVP definition:** crt-royale rendering correctly on Vulkan (steps 1–3), which demonstrates every hard part except cross-backend breadth and cross-frame history.

---

## 6. Key source references

- Render primitives: [AbstractGfx.h](Source/Core/VideoCommon/AbstractGfx.h), [AbstractPipeline.h](Source/Core/VideoCommon/AbstractPipeline.h), [TextureConfig.h](Source/Core/VideoCommon/TextureConfig.h), [AbstractTexture.h:32,37](Source/Core/VideoCommon/AbstractTexture.h#L32), [RenderState.h:163-216](Source/Core/VideoCommon/RenderState.h#L163-L216).
- Single-pass template to generalize: [PostProcessing.cpp:473-606](Source/Core/VideoCommon/PostProcessing.cpp#L473-L606), pipeline pattern [PostProcessing.cpp:1028-1067](Source/Core/VideoCommon/PostProcessing.cpp#L1028-L1067), uniforms [PostProcessing.cpp:608-758,839](Source/Core/VideoCommon/PostProcessing.cpp#L608-L758).
- Shader compile chokepoint: [Spirv.h:23-40](Source/Core/VideoCommon/Spirv.h#L23-L40), [Spirv.cpp:159-256](Source/Core/VideoCommon/Spirv.cpp#L159-L256); per-backend `CreateShaderFromSource` in `VideoBackends/{Vulkan,D3DCommon,D3D12,Metal,OGL}/`; SPIRV-Cross emitters in `Externals/spirv_cross/`.
- Image/texture: [Common/Image.h:14](Source/Core/Common/Image.h#L14), [Assets/CustomTextureData.cpp:566,579](Source/Core/VideoCommon/Assets/CustomTextureData.cpp#L566), [TextureCacheBase.cpp:1614-1646](Source/Core/VideoCommon/TextureCacheBase.cpp#L1614-L1646), [HiresTextures.cpp:79-240](Source/Core/VideoCommon/HiresTextures.cpp#L79).
- Format/wrap gaps: [TextureConfig.h:13-30](Source/Core/VideoCommon/TextureConfig.h#L13-L30), [BPMemory.h:886-893](Source/Core/VideoCommon/BPMemory.h#L886-L893), [VKTexture.cpp:170-189](Source/Core/VideoBackends/Vulkan/VKTexture.cpp#L170).
- Android import interaction: earlier plan `docs/superpowers/plans/2026-07-12-android-custom-post-processing-shader-import.md`; SAF tree pattern in `Source/Android/.../ui/main/MainPresenter.kt` (`onDirectorySelected`), content-URI bridge in `Source/Android/.../utils/ContentHandler.java` + `Source/Core/Common/IOFile.cpp:65-92`.
