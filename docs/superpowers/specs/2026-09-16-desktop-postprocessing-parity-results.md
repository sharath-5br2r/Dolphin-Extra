# Desktop post-processing parity: verification results (2026-09-16)

Branch `feature/desktop-postprocessing-parity`, verified at commit `a720e9a3ec` on two hosts:

- **macOS** (darwin arm64), `build-qt`, configured
  `cmake -S . -B build-qt -DUSE_SYSTEM_SDL3=OFF -DCMAKE_CXX_FLAGS= -DCMAKE_OBJCXX_FLAGS=`.
  Backends built: Metal, Vulkan, OGL, Null, Software.
- **Windows x64** (`pcsx2-win`, `E:\work\dolphin`), `build-win`, configured
  `cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=OFF -DUSE_SYSTEM_SDL3=OFF`
  under VS 18 Community, MSVC 19.51.36257.0 / toolset 14.51.36231.
  Backends built: D3D (D3D11), D3D12, D3DCommon, OGL, Vulkan, Null, Software.
  **This is the first MSVC compile of this branch**, and therefore the first compile of the D3D11
  and D3D12 mip paths at all.

Two constraints shaped what could be verified, and they are the reason most of §7 below is open:

1. **No Qt on the Windows host** (no `C:\Qt`, no `%QTDIR%`, no vcpkg). Installing a multi-gigabyte
   SDK was not authorized, so Windows was configured `-DENABLE_QT=OFF`. The DolphinQt changes have
   **no MSVC compile coverage**.
2. **No desktop backend has ever rendered a frame from this branch, on either host.** The Windows
   host has no Dolphin-format game image (`*.rvz`/`*.gcm`/`*.wbfs` searched across `C:\Users\Ilya`
   and `E:\`) and an ssh session lands in Windows session 0, which cannot create a
   D3D/OpenGL/Vulkan presentation surface (`query session`: `services` = 0 Disc, `console`/`Ilya` =
   1 Active). The macOS host has no game image either, and Tasks 8 and 9 both recorded their
   in-game checks as deferred. Every "renders" claim in the spec is therefore `open / manual`.

## Spec §7 success criteria

Criteria quoted verbatim from `2026-09-16-desktop-postprocessing-parity-design.md:205-219`. Each
carries exactly one of three statuses: **demonstrated**, **demonstrated on macOS only**, or
**open / manual**. There is no "should work".

| # | Criterion (verbatim) | Status | Evidence / why not |
|---|---|---|---|
| 7.1 | ``crt/crt-royale.slangp`` loads and renders on Windows (D3D11, D3D12, Vulkan, OGL) and macOS (Metal, Vulkan), with halation/bloom visibly present. | **open / manual** | *Loads and compiles*: yes, on both hosts — the `SLANG_PRESET` oracle parses the real 12-pass `crt-royale.slangp` (include-heavy, `#reference`-free) and compiles every pass for all four backend headers, `12/12` on each, on macOS and on Windows against a real backslash path. *Renders / halation visibly present*: *never observed on any desktop backend*. No game image on either host; no interactive graphics session on Windows. |
| 7.2 | The OpenGL backend renders the chain right-side up. | **open / manual** | Covered by unit tests and compilation only: `SlangTranslator.ClipYFlipMatchesFramebufferShaderGen`, `.EmitsClipYFlipWhenRequested`, `.OmitsClipYFlipWhenNotRequested`, plus the oracle's `RealPreset[OpenGL]: 12/12`. `videoogl` compiles on both hosts. **Nobody has looked at an OpenGL frame.** The flip's correctness rests on it matching `FramebufferShaderGen`'s existing convention, asserted in a test, not on a rendered image. |
| 7.3 | Qt exposes the renderer toggle, the category→shader picker with Select / Add to Chain, the chain summary, and all three pack downloads including the RetroCrisis profile prompt. | **open / manual** | Proven: the widgets are constructed, laid out and gated in `Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.cpp` (renderer combo `:147-149`, chain dialog `:418`), `PostProcessingChainDialog` exists, the macOS Qt build compiles and links all of it, and the pure helpers behind the UI are unit-tested (`ShaderChainSpec` ×7, `ShaderPackSource` ×4, `ShaderPackDownload` ×5, `RetroCrisisInstall` ×5). Not proven: **no one has opened the dialog.** Task 8 explicitly records "the brief's Step 8 (launch Dolphin, manually test the UI) was not performed"; no display on either host. No MSVC coverage of any of it (`ENABLE_QT=OFF`). |
| 7.4 | ``librashader.dylib`` / ``librashader.dll`` are packaged and ``librashader_load_instance()`` reports a loaded instance on both platforms. | **open / manual** | *Packaged*: demonstrated on both. Windows — `build-win\Binaries\librashader.dll`, 8,611,840 bytes, next to `DolphinNoGUI.exe` (18,291,200 bytes), copied by this build (ninja step `[1456/1904] Copying librashader.dll to the runtime output directory`). macOS — dylib installed into the `.app` bundle, verified in Task 9. *Loadable*: Task 9 recorded a real Windows `LoadLibraryA` + `GetProcAddress` run printing `ABI version: 2` / `API version: 5`, matching `LIBRASHADER_CURRENT_ABI 2`. **Not proven: `librashader_load_instance()` itself**, i.e. Dolphin's own `librashader_ld.h` path reporting `instance_loaded == true`. That needs a running Dolphin. |
| 7.5 | Output Resampling, Color Correction, Anaglyph and Passive are gone from the Qt graphics UI; HDR still works and still selects an scRGB swapchain. | **open / manual** | *Removals*: demonstrated on macOS only. `ColorCorrectionConfigWindow.{cpp,h}` deleted outright (236 lines), removed from `DolphinQt/CMakeLists.txt`, zero remaining references. The stereo combo now has exactly four entries — Off, Side-by-Side, Top-and-Bottom, HDMI 3D (`EnhancementsWidget.cpp:225-228`); Anaglyph and Passive are gone. No Output Resampling row remains under `Config/Graphics/`. The macOS Qt build compiles. *HDR*: the checkbox is still there (`:176`) and this branch does not touch `VKSwapChain.cpp`, `D3DCommon/SwapChain.cpp`, `VideoConfig.cpp` or `GraphicsSettings.cpp` — so scRGB selection is untouched by construction — but **HDR output was not exercised at runtime** and no scRGB swapchain was observed being created. |
| 7.6 | ``ninja -C build-qt unittests`` is green, including translator compilation against the D3D, Metal and OGL shader headers. | **demonstrated** | macOS: `ninja -C build-qt unittests` → `1/1 Test #1: tests ... Passed 6.44 sec`, `100% tests passed`. `./build-qt/Binaries/Tests/tests` → **1134 tests / 69 suites, 1133 passed, 1 skipped, 0 failed**. Windows: `cmake --build build-win --target unittests` → `100% tests passed, 0 tests failed out of 1`; `tests.exe` → **1456 tests / 69 suites, 1455 passed, 1 skipped, 0 failed**. The one skip on each host is `SlangCompile.RealPresetCompilesAllPasses`, which self-skips unless `SLANG_PRESET` is set; run separately below, green on both. All four backend headers (D3D, Metal, OGL, Vulkan) are exercised by `SlangCompile.StockShaderCompilesOnAllBackends`, `.CompatMacrosCompileOnAllBackends` and `.RealPresetCompilesAllPasses`. |

## Platform × backend matrix

"Preset compiles" is the `SLANG_PRESET` oracle against the real `crt-royale.slangp`. Note the oracle
is **host-independent**: it feeds each backend's GLSL preamble plus the matching `APIType` to
glslang and requires SPIR-V out, so all four rows run on both hosts regardless of which backend the
host can actually present with. It is a translate-and-compile proxy — see the caveat below.

| Platform | Backend | Compiles | Unit tests | Preset compiles | Renders |
|---|---|---|---|---|---|
| macOS arm64 | Metal | yes (`Metal/`) | green (1134/1134, 1 skip) | `RealPreset[Metal]: 12/12` | **manual — not run** |
| macOS arm64 | Vulkan | yes (`Vulkan/`) | green | `RealPreset[Vulkan]: 12/12` | **manual — not run** |
| macOS arm64 | OGL | yes (`OGL/`, built though not in the spec's matrix) | green | `RealPreset[OpenGL]: 12/12` | **manual — not run** |
| Windows x64 | D3D11 | yes (`videod3d.lib`, first ever) | green (1456/1456, 1 skip) | `RealPreset[D3D]: 12/12` | **manual — not run** |
| Windows x64 | D3D12 | yes (`videod3d12.lib`, first ever) | green | `RealPreset[D3D]: 12/12` (shared header) | **manual — not run** |
| Windows x64 | Vulkan | yes (`videovulkan.lib`) | green | `RealPreset[Vulkan]: 12/12` | **manual — not run** |
| Windows x64 | OGL | yes (`videoogl.lib`) | green | `RealPreset[OpenGL]: 12/12` | **manual — not run** |

Windows unit-test count reconciliation against macOS (1456 − 1134 = 322), fully accounted for and
all attributable to upstream host-architecture guards, not to this branch:

| Suite | macOS | Windows | Reason |
|---|---|---|---|
| `x64EmitterTest` | — | 338 | x86-64 only |
| `Jit64` | — | 3 | x86-64 only |
| `Arm64Emitter` | 10 | — | arm64 only |
| `JitArm64` | 10 | — | arm64 only |
| `StringUtil` | 23 | 24 | upstream `SplitPathWindowsPathWithDriveLetter` is `#ifdef _WIN32` |
| | | | 338 + 3 − 10 − 10 + 1 = **322** ✓ |

Every post-processing suite has **identical counts on both hosts**: `SlangPreset` 14, `SlangShader`
8, `SlangTranslator` 11, `SlangSamplers` 5, `SlangCompile` 3, `MipGen` 5, `PassSizing` 6,
`PassGraph` 9, `PresetArchive` 4, `PresetName` 5, `ShaderPackDownload` 5, `ShaderPackSource` 4,
`ShaderChainSpec` 7, `RetroCrisisInstall` 5, `SlangSourceDownscale` 6, `ChainOutputPolicy` 5.

## Conclusions

1. **A configure-breaking regression was found, and only a Windows configure could have found it.**
   The librashader DLL copy rule that Task 9's fix round relocated into `Source/Core/CMakeLists.txt`
   aborted the Windows configure outright:

   ```
   CMake Error at Source/Core/CMakeLists.txt:102 (add_custom_command):
     TARGET 'dolphin-nogui' was not created in this directory.
   ```

   Root cause: `add_custom_command(TARGET ...)` requires the target to have been created in the
   *same* directory as the call, but both frontends are created in subdirectories (`DolphinNoGUI/`,
   `DolphinQt/`). `if(TARGET ...)` is global while `add_custom_command(TARGET ...)` is
   directory-scoped, so the guard passes and the command then hard-errors. macOS never saw it
   because the whole block sits behind `if(WIN32 AND ENABLE_VULKAN)`. It would have failed for the
   Qt frontend too (line 98, `dolphin-emu`), so **no Windows build of the branch was possible at
   all** before this. Fixed in `a720e9a3ec` by a single `add_custom_target(librashader-dll ALL)`
   copying into the global `CMAKE_RUNTIME_OUTPUT_DIRECTORY` — which is where both frontends already
   land, making the two per-target copies redundant — ordered with `add_dependencies`, which is not
   directory-scoped. Diagnosis and both the failing and fixed forms were confirmed with an isolated
   CMake reproduction before the fix was written.

2. **The four-backend compile story is real; the four-backend *render* story is entirely unproven.**
   D3D11 and D3D12 had never been compiled before this run and both compile clean. `MipChainBuilder.cpp`,
   `DXTexture.cpp` and `OGLTexture.cpp` all produce objects under MSVC. **Zero MSVC warnings in
   `Source\Core`** — all 4,492 `warning C` lines in the build log come from `Externals`. But
   compiling is not rendering: the mip work (Tasks 4, 5) and the Y-flip (Task 2) rest on
   *unit tests plus compilation only*, and the unit tests cover the *arithmetic*
   (`MipGen.MipLevelCountCountsDownToOneByOne`, `.MipLevelSizeHalvesAndClampsToOne`) and the
   *translate-time decision* (`SlangTranslator.ClipYFlip*`), never the GPU code paths. Nothing
   verifies that `DXTexture::GenerateMipmaps` actually fills a chain, or that `MipChainBuilder`'s
   draw-based fallback produces correct mips on D3D12, or that the flipped chain lands right side
   up. Those four backends are `bSupportsGPUMipGeneration = true` for Metal / OGL / Vulkan / D3D11
   and `false` (fallback) for D3D12; the D3D12 fallback is the least-exercised path on the branch.

3. **The Windows `SLANG_PRESET` run is the strongest end-to-end evidence available without a game.**
   It drives Task 1's separator-agnostic `NormalizePath` and Task 10's `DirName` backslash handling
   with genuine Windows paths
   (`E:\Games\steamapps\common\RetroArch\shaders\shaders_slang\crt\crt-royale.slangp`) through a
   real, `#include`-heavy 12-pass preset, and produces byte-identical output to macOS. Combined with
   14 `SlangPreset` tests (six of them `NormalizePath*`/Windows-path specific) and
   `SlangShader.ExpandsIncludesFromWindowsDirectory`, Task 1 is the best-verified item on the branch.

4. **The compile oracle is a proxy, and its own source says so.** `SlangCompileTest.cpp:138-150`
   documents that it always targets SPIR-V, so the OGL row must define `VARYING_LOCATION` even
   though the real OGL backend leaves it empty and matches varyings by name — that name-based
   matching is *not* covered. The real OGL backend never calls `SPIRV::Compile*` at all. Likewise
   no row generates HLSL or MSL or hands anything to a driver. What the four rows do prove is that
   no Vulkan-only builtin or construct survives translation for any backend. That is real coverage,
   but it is not backend acceptance.

5. **Frame-time comparison of the D3D12 mip fallback vs D3D11's native path: out of scope.** It
   requires a rendered frame, which requires a game image; the Windows host has none. Not measured,
   not estimated.

## Still open

Ordered by how much a reader should worry about it.

1. **No desktop render verification on any backend, on either platform** — §7.1 and §7.2. The
   headline claim of the spec is unobserved. Blocks: no game image on either host; Windows ssh is
   session 0. See the manual checklist below.
2. **The D3D12 `MipChainBuilder` draw-based fallback has never run.** It is the only backend on the
   mipmap path without native `GenerateMipmaps`, is new code, and is covered by neither a test that
   executes it nor a rendered frame.
3. **No MSVC compile coverage of the DolphinQt changes** (Tasks 6 and 8) — no Qt on the verification
   host, and installing one was not authorized. Qt-side compile errors, if any, would not have been
   caught here.
4. **The Qt UI has never been looked at** — §7.3. Widget construction and layout were verified by
   reading code and by the macOS build linking, not by opening the dialog.
5. **`librashader_load_instance()` has not been observed returning a loaded instance** — §7.4. The
   raw DLL loads and reports ABI 2 / API 5, but Dolphin's own load path is unexercised. This
   matters more than it looks: `librashader_ld.h` checks the ABI before binding any other symbol and
   silently degrades to an all-no-op instance with `instance_loaded == false` on mismatch, so a
   wrong ABI is indistinguishable from "post-processing does nothing".
6. **HDR paper-white scaling** — pre-existing, unaddressed by this branch. `fHDRPaperWhiteNits`
   defaults to 203 (`VideoConfig.h:259`) with a configurable 80–500 range, and nothing in the slang
   chain consumes it, so slang presets are not paper-white-aware.
7. **HDR output not exercised at runtime** — §7.5's second clause. The swapchain code is untouched
   by this branch, so this is a no-regression claim by construction rather than an observation.
8. **librashader ships the Vulkan runtime only.** Both desktop binaries are built
   `--no-default-features --features runtime-vulkan` (`Externals/librashader/README.md:19,64,79`),
   so there is no Metal runtime and the Qt renderer toggle stays Vulkan-gated. On macOS that means
   the librashader renderer is reachable only under the Vulkan (MoltenVK) backend, not Metal.
9. **macOS `dolphin-nogui` cannot load librashader.** The dylib lives inside the `.app` bundle and
   `LibrashaderLibraryPath()` resolves through `File::GetBundleDirectory()`, so librashader is a
   Qt-frontend-only feature on macOS. Observation, not a defect; Task 9's CMake was left untouched.
10. **Android's RetroCrisis profile list is still duplicated.** `RetroCrisisInstall.cpp:180-188` and
    `Source/Android/.../res/values/strings.xml:249-257` both hard-code the same seven entries
    ("1080p Flat", "1440p Flat", "4K Flat", "1080p Curved", "1440p Curved", "4K Curved",
    "720p Steam Deck"). Qt reads the C++ list via `GetRetroCrisisProfiles()`; Android does not.
    `RetroCrisisInstall.ProfileListMatchesPackFolders` pins the C++ side only, so the two can drift.
11. **OGL name-based varying matching is untested**, per Conclusion 4 — a gap the oracle cannot
    close by design.

## What was verified, and on which host

**macOS, at `a720e9a3ec`:**

- `ninja -C build-qt unittests` → `100% tests passed`, 1 test, 6.44 s.
- `./build-qt/Binaries/Tests/tests` → 1134 tests / 69 suites, 1133 passed, 1 skipped, 0 failed.
- `SLANG_PRESET=…/PCSX2/shaders/shaders_slang/crt/crt-royale.slangp` +
  `--gtest_filter=SlangCompile.RealPreset*` → all four backends `12/12`.
- Metal, Vulkan and OGL backends compile.

**macOS, after the post-review fix wave** (the four commits that follow this document's original
revision; Windows was not revisited, so the figures below are macOS-only):

- `ninja -C build-qt unittests` → `100% tests passed`, 1 test.
- `./build-qt/Binaries/Tests/tests` → 1136 tests / 70 suites, 1135 passed, 1 skipped, 0 failed.
  The +2 tests are `ShaderChainSpec.TrimsWhitespaceAroundEntries` and
  `VideoConfig.VerifyValidityClampsRemovedStereoModes`; the +1 suite is `VideoConfig`.
- The `SLANG_PRESET` oracle still reports `12/12` on Vulkan, D3D, Metal and OpenGL.

**Windows x64, at `a720e9a3ec`** (checkout-and-build only; no commit, amend, rebase or push was
made on that host, and the working tree was clean before and after):

- Configure succeeds with `-DENABLE_QT=OFF -DUSE_SYSTEM_SDL3=OFF` — *after* the `a720e9a3ec` fix;
  it failed before it (Conclusion 1).
- Full default-target build: 1904 ninja steps, `BUILD_OK`. First MSVC compile of the branch.
- All four post-processing-relevant backends produce libraries: `videod3d.lib`, `videod3d12.lib`,
  `videovulkan.lib`, `videoogl.lib` (plus `videod3dcommon.lib`).
- Branch-specific objects built: `MipChainBuilder.cpp.obj`, `DXTexture.cpp.obj`,
  `OGLTexture.cpp.obj`.
- Zero `warning C` lines anywhere under `Source\Core` (4,492 total, all from `Externals`).
- `cmake --build build-win --target unittests` → `100% tests passed, 0 tests failed out of 1`.
- `tests.exe` → 1456 tests / 69 suites, 1455 passed, 1 skipped, 0 failed. Count delta vs macOS
  fully reconciled above.
- `tests.exe --gtest_filter=SlangCompile.*` with `SLANG_PRESET` set to a real backslash path →
  3 passed, and verbatim:

  ```
  RealPreset[Vulkan]: 12/12 passes compiled
  RealPreset[D3D]: 12/12 passes compiled
  RealPreset[Metal]: 12/12 passes compiled
  RealPreset[OpenGL]: 12/12 passes compiled
  ```

  Byte-identical to the macOS output.
- `build-win\Binaries\librashader.dll` present, 8,611,840 bytes, alongside `DolphinNoGUI.exe`
  (18,291,200 bytes) — the relocated copy rule fires for `dolphin-nogui` under `ENABLE_QT=OFF`.

**Not verified anywhere:** everything in "Still open" above.

## Manual checklist (spec §7.1, §7.2, §7.3, §7.5-HDR)

These require a console session on a machine with a display, a Dolphin-format game image, and a Qt
build. Run at the Windows console (not over ssh — session 0 has no presentation surface), or on
macOS with `Dolphin.app`. Each line pairs a command with the exact observation that would close it.

1. **Preset loads and renders on each backend** (§7.1). For each of `D3D`, `D3D12`, `Vulkan`, `OGL`:

   ```
   build-win\Binaries\Dolphin.exe -v D3D -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
   ```

   Confirms: no "failed to parse" / "missing shader" panic; scanline/mask geometry visible;
   halation/bloom glow around bright areas. In the log, no
   `Failed to create mip chain builder resources`. D3D12 exercises the `MipChainBuilder` fallback,
   D3D11 the native `GenerateMips` — compare the two for identical bloom, since a broken mip chain
   shows up as missing or blocky halation rather than as an error.

2. **OpenGL orientation** (§7.2):

   ```
   build-win\Binaries\Dolphin.exe -v OGL -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
   ```

   Confirms: the image is right side up. Compare against the same frame under Vulkan. This is the
   single highest-value manual check on the list — it is the defect Task 2 exists to fix, and no
   automated evidence distinguishes a correct flip from an inverted one.

3. **Qt UI** (§7.3), Graphics > Enhancements:
   - Post-Processing Renderer combo present, offering Builtin and librashader, and **enabled only on
     Vulkan** (switch backends and re-check).
   - The chain button opens `PostProcessingChainDialog`; a category picks a shader list;
     Select replaces and Add to Chain appends; the chain summary shows `a → b`.
   - The download menu offers all three pack sources; installing the RetroCrisis pack prompts for
     one of the seven display profiles; after install only the chosen profile's presets are listed.
   - Build a 2-preset chain through the dialog and confirm **both** passes affect the frame.
   - No Output Resampling row, no Color Correction row, and the Stereoscopic 3D Mode combo has
     exactly four entries (Off, Side-by-Side, Top-and-Bottom, HDMI 3D).

4. **librashader instance** (§7.4):

   ```
   build-win\Binaries\Dolphin.exe -v Vulkan -C GFX.Enhancements.PostProcessRenderer=1 -C GFX.Enhancements.PostShader=crt/crt-royale.slangp -e <game>
   ```

   Confirms: the log reports a loaded librashader instance (**not** `instance_loaded == false`) and
   the preset renders. A silently-no-op instance renders an unmodified frame, so check the log, not
   just the picture.

5. **HDR** (§7.5): enable HDR Post-Processing on an HDR display under Vulkan or D3D and confirm the
   log shows an scRGB / `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT` swapchain format being chosen
   (`VKSwapChain.cpp:189-200`).

## Reproduction

The Windows host has no Qt and is treated as checkout-and-build only. The branch carries ~19 MB of
vendored librashader binaries, so it is synced with a git bundle over ssh rather than pushed to a
remote. `cmd.exe` quoting through ssh is fragile, so both remote steps are `.bat` files copied over
and invoked in one call.

macOS:

```sh
cd /Users/ilya.lissoboi/work/dolphin
cmake -S . -B build-qt -DUSE_SYSTEM_SDL3=OFF -DCMAKE_CXX_FLAGS= -DCMAKE_OBJCXX_FLAGS=
ninja -C build-qt unittests
./build-qt/Binaries/Tests/tests
SLANG_PRESET="$HOME/Library/Application Support/PCSX2/shaders/shaders_slang/crt/crt-royale.slangp" \
  ./build-qt/Binaries/Tests/tests --gtest_filter='SlangCompile.RealPreset*'
```

Sync to Windows (bundle only the branch commits; land the bundle *outside* the repo so the tree
being built stays clean):

```sh
git -C /Users/ilya.lissoboi/work/dolphin bundle create /tmp/parity.bundle \
  master..feature/desktop-postprocessing-parity
scp -o BatchMode=yes /tmp/parity.bundle pcsx2-win:E:/work/parity.bundle
ssh -o BatchMode=yes pcsx2-win \
  "git -C E:\work\dolphin fetch E:\work\parity.bundle feature/desktop-postprocessing-parity && \
   git -C E:\work\dolphin merge --ff-only FETCH_HEAD && \
   git -C E:\work\dolphin submodule update --init --recursive && \
   git -C E:\work\dolphin rev-parse --short HEAD"
```

`E:\work\build-parity.bat` — configure and build:

```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d E:\work\dolphin || exit /b 1
echo === CONFIGURE ===
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=OFF -DUSE_SYSTEM_SDL3=OFF || exit /b 1
echo === CONFIGURE_OK ===
echo === BUILD ===
cmake --build build-win || exit /b 1
echo === BUILD_OK ===
```

`E:\work\test-parity.bat` — the `tests` target is `EXCLUDE_FROM_ALL`
(`Source/UnitTests/CMakeLists.txt:6`), so the default build compiles the per-test object libraries
but never links the binary; it must be requested explicitly. Do **not** use `ctest` from the build
root: every `add_dolphin_test` links into the one `tests` binary, so `ctest` there finds nothing.

```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul || exit /b 1
cd /d E:\work\dolphin || exit /b 1
echo === BUILD_TESTS ===
cmake --build build-win --target unittests
echo UNITTESTS_EXIT=%ERRORLEVEL%
echo === DLL_CHECK ===
dir build-win\Binaries\librashader.dll
echo === TESTS ===
build-win\Binaries\Tests\tests.exe
echo TESTS_EXIT=%ERRORLEVEL%
echo === ORACLE ===
set SLANG_PRESET=E:\Games\steamapps\common\RetroArch\shaders\shaders_slang\crt\crt-royale.slangp
build-win\Binaries\Tests\tests.exe --gtest_filter=SlangCompile.*
echo ORACLE_EXIT=%ERRORLEVEL%
echo === DONE ===
```

Invoke each with output captured on the Windows side, then read it back with `findstr`/`type` —
the remote shell is `cmd.exe`, with no `head`, `grep` or `tail`:

```sh
ssh -o BatchMode=yes pcsx2-win "cmd /c E:\work\build-parity.bat > E:\work\build-parity.log 2>&1"
ssh -o BatchMode=yes pcsx2-win "cmd /c E:\work\test-parity.bat  > E:\work\test-parity.log  2>&1"
```

Isolated reproduction of the CMake defect from Conclusion 1, independent of Dolphin — the parent
directory adds a `POST_BUILD` command to a target created in a subdirectory:

```sh
mkdir -p /tmp/cmtest/sub
printf 'int main(void){return 0;}\n'      > /tmp/cmtest/sub/main.c
printf 'add_executable(frontend main.c)\n' > /tmp/cmtest/sub/CMakeLists.txt
printf 'hello\n'                           > /tmp/cmtest/payload.bin
cat > /tmp/cmtest/CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.22)
project(cmtest C)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/Binaries>)
add_subdirectory(sub)
if(TARGET frontend)                      # global: passes
  add_custom_command(TARGET frontend POST_BUILD   # directory-scoped: hard error
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/payload.bin" "$<TARGET_FILE_DIR:frontend>")
endif()
EOF
cmake -S /tmp/cmtest -B /tmp/cmtest/b -G Ninja
# => CMake Error ... TARGET 'frontend' was not created in this directory.
```

Replacing that `add_custom_command(TARGET ...)` with an `add_custom_target(... ALL)` writing to
`${CMAKE_RUNTIME_OUTPUT_DIRECTORY}` plus `add_dependencies(frontend ...)` configures, builds, and
places `payload.bin` next to the `frontend` executable — the shape `a720e9a3ec` adopts.
